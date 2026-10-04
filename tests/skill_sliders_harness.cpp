// Behavioral regression harness for the skill sliders (`skillslider`, #160):
// Projectile Amount, Area of Effect and Projectile Speed.
//
// The Python runner injects the REAL ForgePact::SkillSlidersMod class, and
// ModuleMain's real Lower, FirstToken and SdkShortScriptName, below. Only the
// game API is replaced. No game process, character, installed DLL or release
// asset is touched.
//
// The game is played by Dispatch below: when HookOneScript has put a native
// detour in, a call goes through the hook, which is how the game's direct
// calls reach it; otherwise it goes straight to the game's own function. So
// each scenario reports the value the game actually received, not only what
// the plugin logged.
//
// The stand-in answers with the kinds the runtime does (AGENTS.md, "A stub
// that cannot represent the failing input cannot catch the bug"): a `self`
// can arrive as VALUE_REF or as VALUE_OBJECT, `object_index` comes back as a
// real (or as a reference with a flag bit above the kind), and a `self` can
// have no readable `object_index` at all, or be null.
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <hs_game_sdk/objects.hpp>
#include <hs_game_sdk/scripts.hpp>

// ---- minimal game-API stand-ins ------------------------------------------
// The values match tests/cpp/stubs/YYToolkit/YYTK_Shared.hpp (VALUE_REF is
// YYToolkit's own 15).
enum { VALUE_REAL = 0, VALUE_STRING = 1, VALUE_ARRAY = 2, VALUE_UNDEFINED = 5, VALUE_OBJECT = 6,
       VALUE_INT32 = 7, VALUE_INT64 = 8, VALUE_BOOL = 13, VALUE_REF = 15 };
static const int kKindFlagBit = 0x10000000;   // a flag bit above the low 28 bits of a kind

// A script's `self`: the kind RValue(self) arrives as, and its object_index
// as variable_instance_get answers it (or a throw).
struct CInstance {
    std::string tag;
    int selfKind = VALUE_REF;
    double objectIndex = -1;
    int objectIndexKind = VALUE_REAL;   // VALUE_UNDEFINED: no readable object_index
    bool objectIndexThrows = false;
};

struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    CInstance* inst = nullptr;
    std::shared_ptr<std::vector<RValue>> items;   // shared: GML arrays are references
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(CInstance* p) : m_Kind(p ? p->selfKind : VALUE_UNDEFINED), inst(p) {}
    static RValue Array(std::vector<double> values) {
        RValue r;
        r.m_Kind = VALUE_ARRAY;
        r.items = std::make_shared<std::vector<RValue>>();
        for (double v : values) r.items->push_back(RValue(v));
        return r;
    }
    // The runner's REAL_RValue raises its own error on a kind it cannot
    // convert; here that is a throw, so a scenario reading one shows threw=.
    double ToDouble() const {
        const int kind = m_Kind & 0x0FFFFFFF;
        if (kind == VALUE_REAL || kind == VALUE_INT32 || kind == VALUE_INT64 || kind == VALUE_REF) return number;
        throw std::runtime_error("ToDouble on kind " + std::to_string(m_Kind));
    }
    std::string ToString() const { return text; }
};
using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);

// ---- the controlled world -------------------------------------------------
struct Script {
    PFUNC_YYGMLScript game = nullptr;   // the game's own function
    void* detour = nullptr;             // what the native route calls instead
    bool found = true;                  // GetNamedRoutinePointer succeeds
    bool nativeWorks = true;            // MmCreateHook succeeds
    int installs = 0;
};
struct World {
    std::map<std::string, Script> scripts;
    std::vector<std::string> log;
    // What the game's own scripts return this call.
    RValue spell = RValue(0.0);         // ReturnExtraSpellProjectiles: the extra projectiles
    RValue ranged = RValue(1.0);        // ReturnExtraProjectilesRanged: the extra only
    std::vector<double> aoe{ 0, 7, 0, 0 };   // StatAOESkillSize: built fresh, 4 elements
    std::map<double, double> stats;     // ReturnSpecificStat by stat id; 0 when absent
    std::vector<double> lamStatIds{ 75, 74, 554 };   // what LoadAllModifiers reads
    CInstance* inner = nullptr;         // a LoadAllModifiers that runs inside the outer one
    std::vector<std::string> reads;     // every stat LoadAllModifiers read, as tag:id=value
    std::vector<RValue> built;          // every AoE array the game built, to check it is untouched
};
static World world;

struct FakeRunner {
    long builtinCalls = 0;
    RValue CallBuiltin(const char* name, std::initializer_list<RValue> list) {
        ++builtinCalls;
        std::vector<RValue> a(list);
        const std::string n = name;
        if (n == "array_length") return RValue((double)a[0].items->size());
        if (n == "array_create") {
            RValue r = RValue::Array({});
            r.items->resize((size_t)a[0].ToDouble());
            return r;
        }
        if (n == "array_get") return (*a[0].items)[(size_t)a[1].ToDouble()];
        if (n == "array_set") { (*a[0].items)[(size_t)a[1].ToDouble()] = a[2]; return RValue(); }
        if (n == "asset_get_index") {
            for (int i = 0; i < 7000; ++i) {
                const auto object = static_cast<HeroSiege::Objects::GameObject>(i);
                if (HeroSiege::Objects::GetObjectName(object) == a[0].text) return RValue((double)i);
            }
            return RValue(-1.0);
        }
        if (n == "object_get_name") {
            const auto object = static_cast<HeroSiege::Objects::GameObject>((int)a[0].ToDouble());
            return RValue(std::string(HeroSiege::Objects::GetObjectName(object)));
        }
        if (n == "variable_instance_get") {
            CInstance* inst = a[0].inst;
            if (!inst) throw std::runtime_error("variable_instance_get on no instance");
            if (a[1].text != "object_index") throw std::runtime_error("unexpected variable " + a[1].text);
            if (inst->objectIndexThrows) throw std::runtime_error("object_index read threw");
            RValue v;
            v.m_Kind = inst->objectIndexKind;
            v.number = inst->objectIndex;
            return v;
        }
        throw std::runtime_error("unexpected builtin " + n);
    }
};
static FakeRunner g_Runner;
static FakeRunner* g_Yytk = &g_Runner;

static void Out(const std::string& s) { world.log.push_back(s); }

static std::string Num(double v) { std::ostringstream o; o << v; return o.str(); }

static RValue& Dispatch(const char* name, CInstance* self, std::vector<RValue> args, RValue& R);

// The value a caller reads from a result: element 0 of an array.
static double Read(const RValue& r) {
    if (r.m_Kind == VALUE_ARRAY) return (*r.items)[0].ToDouble();
    return r.ToDouble();
}

// ReturnSpecificStat as the game calls it: (1, <id>, 0).
static double Stat(CInstance* self, double id) {
    RValue R;
    return Read(Dispatch("ReturnSpecificStat", self, { RValue(1.0), RValue(id), RValue(0.0) }, R));
}

static RValue& GameSpell(CInstance*, CInstance*, RValue& R, int, RValue**) { R = world.spell; return R; }
static RValue& GameRanged(CInstance*, CInstance*, RValue& R, int, RValue**) { R = world.ranged; return R; }
static RValue& GameAoe(CInstance*, CInstance*, RValue& R, int, RValue**) {
    R = RValue::Array(world.aoe);
    world.built.push_back(R);
    return R;
}
static RValue& GameSpecificStat(CInstance*, CInstance*, RValue& R, int argc, RValue** A) {
    const double id = argc >= 2 ? A[1]->ToDouble() : -1;
    const auto it = world.stats.find(id);
    R = RValue(it == world.stats.end() ? 0.0 : it->second);
    return R;
}
// LoadAllModifiers reads its stats through ReturnSpecificStat. With an
// `inner` instance set, a second LoadAllModifiers runs inside the first (the
// mercenary's, say), and the outer one then reads stat 75 again.
static int g_LamDepth = 0;
static std::string Tag(CInstance* self) { return self ? self->tag : std::string("null"); }
static RValue& GameLoadAllModifiers(CInstance* S, CInstance*, RValue& R, int, RValue**) {
    ++g_LamDepth;
    for (double id : world.lamStatIds) world.reads.push_back(Tag(S) + ":" + Num(id) + "=" + Num(Stat(S, id)));
    if (world.inner && g_LamDepth == 1) {
        RValue inner;
        Dispatch("LoadAllModifiers", world.inner, {}, inner);
        world.reads.push_back(Tag(S) + ":75after=" + Num(Stat(S, 75)));
    }
    --g_LamDepth;
    R = RValue(0.0);
    return R;
}

static RValue& Dispatch(const char* name, CInstance* self, std::vector<RValue> args, RValue& R) {
    Script& s = world.scripts[name];
    std::vector<RValue*> pointers;
    for (RValue& a : args) pointers.push_back(&a);
    RValue** A = pointers.empty() ? nullptr : pointers.data();
    const PFUNC_YYGMLScript fn = s.detour ? reinterpret_cast<PFUNC_YYGMLScript>(s.detour) : s.game;
    return fn(self, nullptr, R, (int)args.size(), A);
}

static void ResetWorld() {
    world = World{};
    g_LamDepth = 0;
    world.scripts["ReturnExtraSpellProjectiles"].game = GameSpell;
    world.scripts["ReturnExtraProjectilesRanged"].game = GameRanged;
    world.scripts["StatAOESkillSize"].game = GameAoe;
    world.scripts["ReturnSpecificStat"].game = GameSpecificStat;
    world.scripts["LoadAllModifiers"].game = GameLoadAllModifiers;
}

// ModuleMain's installer, reduced to its contract with the mod: the first
// install saves the original (the trampoline on the native route, the table
// entry otherwise) and reports whether the native detour went in.
static bool HookOneScript(const char* shortName, const char* id, void* dest, PFUNC_YYGMLScript* origOut,
                          bool* nativeOut = nullptr) {
    (void)id;
    if (nativeOut) *nativeOut = false;
    Script& s = world.scripts[shortName];
    ++s.installs;
    if (!s.found) { Out(std::string("hook ") + shortName + ": not found st=1"); return false; }
    if (origOut && !*origOut) {
        *origOut = s.game;
        if (s.nativeWorks) {
            s.detour = dest;
            if (nativeOut) *nativeOut = true;
        } else {
            Out(std::string("hook ") + shortName + ": TABLE-ONLY (MmCreateHook st=2) - direct compiled-GML calls will bypass this hook");
        }
    }
    Out(std::string("HOOK INSTALLED on ") + shortName);
    return true;
}

#define BP_DIAG_INCREMENT(counter) (++(counter))
#ifndef _MSC_VER
#define sprintf_s(buf, ...) std::snprintf(buf, sizeof(buf), __VA_ARGS__)
#endif

// PRODUCTION_HELPERS

// PRODUCTION_SKILLSLIDERS

using ForgePact::SkillSlidersMod;

// ---- the instances --------------------------------------------------------
static double ObjectIndex(HeroSiege::Objects::GameObject object) { return (double)static_cast<int>(object); }
struct Cast {
    CInstance player{ "player", VALUE_REF, ObjectIndex(HeroSiege::Objects::GameObject::Player_obj) };
    CInstance playerObject{ "playerobj", VALUE_OBJECT, ObjectIndex(HeroSiege::Objects::GameObject::Player_obj) };
    CInstance playerRefIndex{ "playerrefindex", VALUE_REF, ObjectIndex(HeroSiege::Objects::GameObject::Player_obj),
                              VALUE_REF | kKindFlagBit };
    CInstance doubleCast{ "double", VALUE_OBJECT, ObjectIndex(HeroSiege::Objects::GameObject::Universal_Double_Cast_obj) };
    CInstance merc{ "merc", VALUE_REF, ObjectIndex(HeroSiege::Objects::GameObject::Mercenary_obj) };
    CInstance enemy{ "enemy", VALUE_REF, ObjectIndex(HeroSiege::Objects::GameObject::Aztec_Sword_Skeleton_obj) };
    CInstance basic{ "basic", VALUE_REF, ObjectIndex(HeroSiege::Objects::GameObject::Projectile_Player_obj) };
    CInstance noIndex{ "noindex", VALUE_OBJECT, -1, VALUE_UNDEFINED };
    CInstance throwing{ "throwing", VALUE_OBJECT, -1, VALUE_REAL, true };
};
static Cast cast;

// ---- scenarios ------------------------------------------------------------
// The mod is a singleton in the plugin; each scenario takes the class's own
// Instance() and resets it by placement on a default-built image.
static SkillSlidersMod* g_Mod = nullptr;
static SkillSlidersMod& Mod() { return *g_Mod; }

static void Command(const std::string& line) { Mod().HandleCommand(line); }

static double Spell(CInstance* self) { RValue R; return Read(Dispatch("ReturnExtraSpellProjectiles", self, { RValue(0.0) }, R)); }
static double Ranged(CInstance* self) { RValue R; return Read(Dispatch("ReturnExtraProjectilesRanged", self, { RValue(0.0) }, R)); }
static std::string AoeArray(CInstance* self) {
    RValue R;
    RValue& r = Dispatch("StatAOESkillSize", self, {}, R);
    std::string s = "[";
    for (size_t i = 0; i < r.items->size(); ++i) s += (i ? "," : "") + Num((*r.items)[i].ToDouble());
    return s + "]";
}
static void Lam(CInstance* self) { RValue R; Dispatch("LoadAllModifiers", self, { RValue(0.0), RValue(0.0), RValue(0.0) }, R); }
static std::string Reads() {
    std::string s;
    for (size_t i = 0; i < world.reads.size(); ++i) s += (i ? "," : "") + world.reads[i];
    world.reads.clear();
    return s;
}
static std::string Built() {
    std::string s;
    for (const auto& a : world.built) {
        s += "[";
        for (size_t i = 0; i < a.items->size(); ++i) s += (i ? "," : "") + Num((*a.items)[i].ToDouble());
        s += "]";
    }
    return s;
}
static int Installs() {
    int n = 0;
    for (const auto& [name, s] : world.scripts) n += s.installs;
    return n;
}

static void Print(const std::string& label, const std::string& fields) {
    std::cout << "SCENARIO " << label << " " << fields << "\n";
    for (const auto& l : world.log) std::cout << "LOG " << label << " :: " << l << "\n";
}

static void Scenario(const std::string& label, const std::function<std::string()>& body) {
    ResetWorld();
    g_Runner.builtinCalls = 0;
    static SkillSlidersMod& instance = SkillSlidersMod::Instance();
    instance.~SkillSlidersMod();
    new (&instance) SkillSlidersMod();   // NOLINT: re-run the default member initializers
    g_Mod = &instance;
    std::string fields;
    try { fields = body(); } catch (const std::exception& e) { fields = std::string("threw=") + e.what(); }
    Print(label, fields);
}

int main() {
    // ---- baseline: vanilla stays vanilla ----------------------------------
    // Nothing armed: every script's result reaches its caller unchanged, no
    // hook is installed and the mod calls no builtin.
    Scenario("baseline_off", [] {
        std::string out = "spell=" + Num(Spell(&cast.player)) + " ranged=" + Num(Ranged(&cast.player))
                        + " aoe=" + AoeArray(&cast.player);
        Lam(&cast.player);
        out += " lam=" + Reads() + " out75=" + Num(Stat(&cast.player, 75));
        return out + " installs=" + std::to_string(Installs()) + " builtins=" + std::to_string(g_Runner.builtinCalls);
    });
    // 0 with no hook installs nothing.
    Scenario("zero_installs_nothing", [] {
        Command("projamount 0");
        Command("aoesize 0");
        Command("projspeed 0");
        return "spell=" + Num(Spell(&cast.player)) + " installs=" + std::to_string(Installs())
             + " builtins=" + std::to_string(g_Runner.builtinCalls);
    });
    // Armed, then set to 0: a pure pass-through, counters unchanged, no builtin.
    Scenario("off_after_on", [] {
        Command("projamount 2");
        Command("aoesize 50");
        Command("projspeed 50");
        const std::string on = Num(Spell(&cast.player)) + "," + Num(Ranged(&cast.player)) + "," + AoeArray(&cast.player);
        Lam(&cast.player);
        const std::string onReads = Reads();
        Command("projamount 0");
        Command("aoesize 0");
        Command("projspeed 0");
        world.log.clear();
        Command("list");
        const std::vector<std::string> before = world.log;
        const long builtins = g_Runner.builtinCalls;
        std::string off = Num(Spell(&cast.player)) + "," + Num(Ranged(&cast.player)) + "," + AoeArray(&cast.player)
                        + "," + Num(Spell(&cast.merc));
        Lam(&cast.player);
        Lam(&cast.merc);
        const std::string offReads = Reads();
        const long builtinsWhileOff = g_Runner.builtinCalls - builtins;
        world.log.clear();
        Command("list");
        const bool same = world.log == before;
        return "on=" + on + " onreads=" + onReads + " off=" + off + " offreads=" + offReads
             + " builtinsWhileOff=" + std::to_string(builtinsWhileOff) + " countersUnchanged=" + (same ? "yes" : "no")
             + " installs=" + std::to_string(Installs());
    });
    // Armed, and the self is not the player: unchanged, counted `other`.
    Scenario("amount_scope", [] {
        Command("projamount 2");
        // One call per statement: the order of the calls is part of what the
        // status line reports (first=, last-other=).
        std::string out;
        out += "player=" + Num(Spell(&cast.player));
        out += " double=" + Num(Spell(&cast.doubleCast));
        out += " merc=" + Num(Spell(&cast.merc));
        out += " enemy=" + Num(Spell(&cast.enemy));
        out += " basic=" + Num(Spell(&cast.basic));
        out += " null=" + Num(Spell(nullptr));
        out += " noindex=" + Num(Spell(&cast.noIndex));
        out += " throwing=" + Num(Spell(&cast.throwing));
        out += " rangedplayer=" + Num(Ranged(&cast.player));
        out += " rangeddouble=" + Num(Ranged(&cast.doubleCast));
        out += " rangedmerc=" + Num(Ranged(&cast.merc));
        Command("list");
        return out;
    });
    // `last-other=` names the last out-of-scope object, by its name, and
    // reads `?` when that self was null or had no readable object_index.
    Scenario("last_other_named", [] {
        Command("projamount 1");
        Spell(&cast.noIndex);
        Spell(&cast.enemy);
        Command("list");
        Spell(nullptr);
        Command("list");
        Spell(&cast.merc);
        Spell(&cast.throwing);
        Command("list");
        return std::string("listed");
    });
    // AoE: element 0 of a copy, the game's array untouched.
    Scenario("aoe_target", [] {
        Command("aoesize 50");
        const std::string player = AoeArray(&cast.player);
        const std::string dbl = AoeArray(&cast.doubleCast);
        const std::string merc = AoeArray(&cast.merc);
        return "player=" + player + " double=" + dbl + " merc=" + merc + " built=" + Built()
             + " installs=" + std::to_string(world.scripts["StatAOESkillSize"].installs);
    });
    // Speed: stat 75 inside a Player_obj LoadAllModifiers only.
    Scenario("speed_target", [] {
        Command("projspeed 50");
        std::string out = "out75=" + Num(Stat(&cast.player, 75));
        Lam(&cast.player);
        out += " player=" + Reads();
        Lam(&cast.merc);
        out += " merc=" + Reads();
        Lam(&cast.doubleCast);
        out += " double=" + Reads();
        Lam(&cast.basic);
        out += " basic=" + Reads();
        out += " after75=" + Num(Stat(&cast.player, 75));
        Command("list");
        return out;
    });
    // A Mercenary_obj LoadAllModifiers nested inside the player's is
    // unchanged, and the outer scope applies again after it returns.
    Scenario("speed_nested", [] {
        Command("projspeed 50");
        world.inner = &cast.merc;
        Lam(&cast.player);
        const std::string playerOuter = Reads();
        world.inner = &cast.player;
        Lam(&cast.merc);
        return "playerouter=" + playerOuter + " mercouter=" + Reads() + " out75=" + Num(Stat(&cast.player, 75));
    });
    // Gear that already gives stat 75 keeps it; the lever adds on top.
    Scenario("speed_adds_to_native", [] {
        world.stats[75] = 20;
        world.stats[74] = 3;
        Command("projspeed 30");
        Lam(&cast.player);
        return "player=" + Reads();
    });
    // The player as a reference, as an instance pointer, and with its
    // object_index answered as a reference carrying a flag bit.
    Scenario("self_kinds", [] {
        Command("projamount 2");
        Command("projspeed 40");
        std::string out = "ref=" + Num(Spell(&cast.player)) + " obj=" + Num(Spell(&cast.playerObject))
                        + " refindex=" + Num(Spell(&cast.playerRefIndex));
        Lam(&cast.playerObject);
        out += " lamobj=" + Reads();
        Lam(&cast.playerRefIndex);
        return out + " lamrefindex=" + Reads();
    });
    // Results that are not a plain number.
    Scenario("amount_shapes", [] {
        Command("projamount 2");
        world.spell = RValue::Array({ 1, 9 });
        RValue first = world.spell;
        RValue R;
        RValue& r = Dispatch("ReturnExtraSpellProjectiles", &cast.player, {}, R);
        std::string out = "array=" + Num((*r.items)[0].ToDouble()) + "," + Num((*r.items)[1].ToDouble())
                        + " gamearray=" + Num((*first.items)[0].ToDouble()) + "," + Num((*first.items)[1].ToDouble());
        world.spell = RValue();
        RValue R2;
        out += " undefinedkind=" + std::to_string(Dispatch("ReturnExtraSpellProjectiles", &cast.player, {}, R2).m_Kind);
        world.spell = RValue("text");
        RValue R3;
        RValue& s = Dispatch("ReturnExtraSpellProjectiles", &cast.player, {}, R3);
        out += " string=" + s.text + ":" + std::to_string(s.m_Kind);
        world.spell = RValue(6.0);   // the base-6 call that rides along with a cast
        out += " base6=" + Num(Spell(&cast.player));
        return out;
    });
    // The first boosted call is reported once per arming; an out-of-scope
    // call does not use it up.
    Scenario("first_call_once_per_arming", [] {
        Command("projamount 2");
        Spell(&cast.merc);
        Spell(&cast.player);
        Ranged(&cast.player);
        Spell(&cast.player);
        world.spell = RValue(1.0);
        Command("projamount 3");
        Spell(&cast.player);
        Spell(&cast.player);
        Command("projamount 0");
        return "off=" + Num(Spell(&cast.player));
    });
    Scenario("first_call_aoe_speed", [] {
        Command("aoesize 50");
        AoeArray(&cast.player);
        AoeArray(&cast.player);
        Command("projspeed 50");
        Lam(&cast.player);
        Lam(&cast.player);
        return std::string("done");
    });
    // Clamps: whole projectiles, each capped at its ceiling, junk is 0.
    Scenario("clamps", [] {
        std::string out;
        Command("projamount 2.6");  out += "a=" + Num(Spell(&cast.player));
        Command("projamount 2.4");  out += " b=" + Num(Spell(&cast.player));
        Command("projamount 9");    out += " c=" + Num(Spell(&cast.player));
        Command("projamount -1");   out += " d=" + Num(Spell(&cast.player));
        Command("projamount nan");  out += " e=" + Num(Spell(&cast.player));
        Command("projamount inf");  out += " f=" + Num(Spell(&cast.player));
        Command("aoesize 12.5");    out += " g=" + AoeArray(&cast.player);
        Command("aoesize 250");     out += " h=" + AoeArray(&cast.player);
        Command("projspeed 150");   out += " i=" + Num(Stat(&cast.player, 75)); Lam(&cast.player); out += ":" + Reads();
        Command("projspeed -5");    Lam(&cast.player); out += " j=" + Reads();
        return out;
    });
    // A table-only install is refused: the game calls these scripts directly.
    Scenario("table_only_refused", [] {
        world.scripts["StatAOESkillSize"].nativeWorks = false;
        Command("aoesize 20");
        Command("aoesize 30");
        const std::string refused = AoeArray(&cast.player);
        Command("aoesize 0");
        Command("list");
        return "aoe=" + refused + " installs=" + std::to_string(world.scripts["StatAOESkillSize"].installs);
    });
    // One of two helpers table-only: the lever stays off on both.
    Scenario("partial_refused", [] {
        world.scripts["ReturnExtraProjectilesRanged"].nativeWorks = false;
        Command("projamount 2");
        return "spell=" + Num(Spell(&cast.player)) + " ranged=" + Num(Ranged(&cast.player));
    });
    Scenario("not_found_refused", [] {
        world.scripts["LoadAllModifiers"].found = false;
        Command("projspeed 50");
        Lam(&cast.player);
        const std::string reads = Reads();
        Command("list");
        Command("projspeed 0");
        return "lam=" + reads + " out75=" + Num(Stat(&cast.player, 75));
    });
    // The status lines at a fresh launch, and after arming.
    Scenario("list_fresh", [] {
        Command("");
        Command("list");
        return "builtins=" + std::to_string(g_Runner.builtinCalls);
    });
    Scenario("list_after_arming", [] {
        Command("projamount 2");
        Spell(&cast.player);
        Spell(&cast.merc);
        Command("aoesize 50");
        Command("list");
        return std::string("listed");
    });
    Scenario("bad_input", [] {
        Command("foo 3");
        Command("projamount abc");
        Command("projamount");
        Command("ProjAmount 1");
        return "installs=" + std::to_string(Installs()) + " spell=" + Num(Spell(&cast.player));
    });
    std::cout << "HARNESS DONE\n";
    return 0;
}
