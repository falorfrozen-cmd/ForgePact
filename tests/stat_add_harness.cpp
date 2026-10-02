// Behavioral regression harness for the additive stat boosts (`statadd`, #114):
// Faster Cast Rate, Skill Haste and All Skills.
//
// The Python runner injects the REAL ForgePact::StatsManager class, and
// ModuleMain's real Lower and FirstToken, below. Only the game API is
// replaced. No game process, character, installed DLL or release asset is
// touched.
//
// The game is played by GameCall below: a Stat* script builds its array
// fresh, as the game's own does, and a caller reads element 0. When
// HookOneScript has put a native detour in, the call goes through the hook,
// which is how the game's direct calls reach it. So each scenario reports the
// value the game actually received, not only what the plugin logged.
//
// Why a harness rather than source-string assertions: a log line can claim an
// effect that did not happen (ForgePact #125, and origin's review of PR #4).
// Only running the hook in the game's place tells those apart.
#include <cmath>
#include <cstdio>
#include <cctype>
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

// ---- minimal game-API stand-ins ------------------------------------------
enum { VALUE_REAL, VALUE_STRING, VALUE_ARRAY, VALUE_UNDEFINED = 5 };

struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    std::shared_ptr<std::vector<RValue>> items;   // shared: GML arrays are references
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    static RValue Array(std::vector<double> values) {
        RValue r;
        r.m_Kind = VALUE_ARRAY;
        r.items = std::make_shared<std::vector<RValue>>();
        for (double v : values) r.items->push_back(RValue(v));
        return r;
    }
    double ToDouble() const {
        if (m_Kind == VALUE_REAL) return number;
        throw std::runtime_error("ToDouble on a non-number");
    }
    std::string ToString() const { return text; }
};
struct CInstance { int id = 0; };
using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);

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
        throw std::runtime_error("unexpected builtin " + n);
    }
};
static FakeRunner g_Runner;
static FakeRunner* g_Yytk = &g_Runner;

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
    // What the game's own Stat* scripts build this call. A result shaped as
    // an array is built fresh every call; `scalar` returns a plain number.
    std::vector<double> fasterCastRate{ 33, 0, 0 };
    std::vector<double> spellHaste{ 12, 5, 3 };
    std::vector<double> allSkills{ 2, 1 };
    bool scalar = false;
    std::vector<RValue> built;          // every array the game built, to check it is untouched
};
static World world;

static void Out(const std::string& s) { world.log.push_back(s); }

static RValue& GameStat(RValue& R, const std::vector<double>& values) {
    if (world.scalar) { R = RValue(values[0]); return R; }
    R = RValue::Array(values);
    world.built.push_back(R);
    return R;
}
static RValue& GameFasterCastRate(CInstance*, CInstance*, RValue& R, int, RValue**) { return GameStat(R, world.fasterCastRate); }
static RValue& GameSpellHaste(CInstance*, CInstance*, RValue& R, int, RValue**) { return GameStat(R, world.spellHaste); }
static RValue& GameAllSkills(CInstance*, CInstance*, RValue& R, int, RValue**) { return GameStat(R, world.allSkills); }

static void ResetWorld() {
    world = World{};
    world.scripts["StatFasterCastRate"].game = GameFasterCastRate;
    world.scripts["StatSpellHaste"].game = GameSpellHaste;
    world.scripts["StatAllSkills"].game = GameAllSkills;
}

// ModuleMain's installer, reduced to its contract with StatsManager: the
// first install saves the original (the trampoline on the native route, the
// table entry otherwise) and reports whether the native detour went in.
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

// The game calling one of its Stat* scripts directly, as compiled GML does:
// only a native detour sees it. Returns element 0, what the caller reads.
static double GameCall(const char* name) {
    Script& s = world.scripts[name];
    RValue R;
    RValue& r = s.detour ? reinterpret_cast<PFUNC_YYGMLScript>(s.detour)(nullptr, nullptr, R, 0, nullptr)
                         : s.game(nullptr, nullptr, R, 0, nullptr);
    if (r.m_Kind == VALUE_ARRAY) return (*r.items)[0].ToDouble();
    return r.ToDouble();
}

namespace HeroSiege { namespace RewardScope {
inline bool Active() { return false; }
inline void SetForgePactXp(double) {}
}}

// The shared CombatText detour (CombatTextHook.hpp), reduced to the surface
// StatsManager touches. Only the Experience multiplier reaches it, which no
// statadd scenario sets; tests/mining_ore_harness.cpp runs the real one.
namespace ForgePact { namespace CombatText {
inline double xpMultiplier = 1.0;
inline bool (*rewardScopeActive)() = nullptr;
inline bool Install() { return false; }
}}

#define BP_DIAG_INCREMENT(counter) (++(counter))
#ifndef _MSC_VER
#define sprintf_s(buf, ...) std::snprintf(buf, sizeof(buf), __VA_ARGS__)
#endif

// PRODUCTION_HELPERS

// PRODUCTION_STATSMANAGER

using ForgePact::StatsManager;

// ---- scenarios ------------------------------------------------------------
// Each scenario starts from a fresh world and a fresh manager; the manager is
// a singleton in the plugin, so a scenario that needs a clean one builds it.
static StatsManager* g_Manager = nullptr;
static StatsManager& Manager() { return *g_Manager; }

static void Command(const std::string& line) { Manager().HandleStatAddCommand(line); }

static void Print(const std::string& label, const std::string& fields) {
    std::cout << "SCENARIO " << label << " " << fields << "\n";
    for (const auto& l : world.log) std::cout << "LOG " << label << " :: " << l << "\n";
}

static std::string Num(double v) { std::ostringstream o; o << v; return o.str(); }

static std::string Built() {
    // What the game's own arrays still hold: a hook must never write them.
    std::string s;
    for (const auto& a : world.built) {
        s += "[";
        for (size_t i = 0; i < a.items->size(); ++i) s += (i ? "," : "") + Num((*a.items)[i].ToDouble());
        s += "]";
    }
    return s;
}

static void Scenario(const std::string& label, const std::function<std::string()>& body) {
    ResetWorld();
    // StatsManager's constructor is private (a singleton): the harness takes
    // one fresh object per scenario through the class's own Instance(), then
    // resets its state by placement on a copy of a default-built image.
    static StatsManager& instance = StatsManager::Instance();
    new (&instance) StatsManager();   // NOLINT: re-run the default member initializers
    g_Manager = &instance;
    std::string fields;
    try { fields = body(); } catch (const std::exception& e) { fields = std::string("threw=") + e.what(); }
    Print(label, fields);
}

int main() {
    // Baseline: nothing armed, the game's own values reach its callers.
    Scenario("baseline_off", [] {
        return "haste=" + Num(GameCall("StatSpellHaste")) + " all=" + Num(GameCall("StatAllSkills"))
             + " fcr=" + Num(GameCall("StatFasterCastRate"))
             + " installs=" + std::to_string(world.scripts["StatSpellHaste"].installs + world.scripts["StatAllSkills"].installs);
    });
    // 0 with no hook installs nothing: vanilla stays hook-free.
    Scenario("zero_installs_nothing", [] {
        Command("skillhaste 0");
        Command("allskills 0");
        return "haste=" + Num(GameCall("StatSpellHaste")) + " all=" + Num(GameCall("StatAllSkills"))
             + " installs=" + std::to_string(world.scripts["StatSpellHaste"].installs + world.scripts["StatAllSkills"].installs);
    });
    // Target: Skill Haste +100 over a native 12 reaches the caller as 112,
    // on every call, while the game's own array keeps 12.
    Scenario("skillhaste_target", [] {
        Command("skillhaste 100");
        const double a = GameCall("StatSpellHaste");
        const double b = GameCall("StatSpellHaste");
        const double c = GameCall("StatSpellHaste");
        return "haste=" + Num(a) + "," + Num(b) + "," + Num(c) + " built=" + Built()
             + " installs=" + std::to_string(world.scripts["StatSpellHaste"].installs);
    });
    // All Skills +19 over a native 2: 21, the rest of the array untouched.
    Scenario("allskills_target", [] {
        Command("allskills 19");
        RValue R;
        Script& s = world.scripts["StatAllSkills"];
        RValue& r = reinterpret_cast<PFUNC_YYGMLScript>(s.detour)(nullptr, nullptr, R, 0, nullptr);
        return "all=" + Num((*r.items)[0].ToDouble()) + " second=" + Num((*r.items)[1].ToDouble())
             + " length=" + std::to_string(r.items->size()) + " built=" + Built();
    });
    // Whole levels, capped at kAllSkillsMax; a negative or junk bonus is 0.
    Scenario("allskills_whole_and_capped", [] {
        std::string out;
        Command("allskills 19.6");  out += "a=" + Num(GameCall("StatAllSkills"));
        Command("allskills 19.4");  out += " b=" + Num(GameCall("StatAllSkills"));
        Command("allskills 250");   out += " c=" + Num(GameCall("StatAllSkills"));
        Command("allskills -3");    out += " d=" + Num(GameCall("StatAllSkills"));
        Command("allskills nan");   out += " e=" + Num(GameCall("StatAllSkills"));
        return out;
    });
    // Skill Haste keeps a fraction and has no plugin cap (the panel's slider
    // ceiling is the limit); a negative bonus is 0.
    Scenario("skillhaste_fraction_and_negative", [] {
        std::string out;
        Command("skillhaste 2.5");  out += "a=" + Num(GameCall("StatSpellHaste"));
        Command("skillhaste 900");  out += " b=" + Num(GameCall("StatSpellHaste"));
        Command("skillhaste -40");  out += " c=" + Num(GameCall("StatSpellHaste"));
        return out;
    });
    // The first boosted call is reported once per arming, native -> boosted.
    Scenario("first_call_once_per_arming", [] {
        Command("skillhaste 100");
        GameCall("StatSpellHaste"); GameCall("StatSpellHaste");
        world.spellHaste[0] = 30;          // gear changed: the next arming reports the new native
        Command("skillhaste 50");
        GameCall("StatSpellHaste"); GameCall("StatSpellHaste");
        Command("skillhaste 0");
        const double off = GameCall("StatSpellHaste");
        return "off=" + Num(off);
    });
    // Turning a boost off leaves the hook a pure pass-through.
    Scenario("off_after_on", [] {
        Command("allskills 10");
        const double on = GameCall("StatAllSkills");
        const long before = g_Runner.builtinCalls;
        Command("allskills 0");
        const double off = GameCall("StatAllSkills");
        return "on=" + Num(on) + " off=" + Num(off) + " builtinsWhileOff=" + std::to_string(g_Runner.builtinCalls - before);
    });
    // A table-only install is refused: the game calls these scripts directly,
    // so the bonus could never apply.
    // Turning it off afterwards is quiet: 0 needs no route.
    Scenario("table_only_refused", [] {
        world.scripts["StatAllSkills"].nativeWorks = false;
        Command("allskills 20");
        Command("allskills 30");
        const double refused = GameCall("StatAllSkills");
        Command("allskills 0");
        return "all=" + Num(refused) + " installs=" + std::to_string(world.scripts["StatAllSkills"].installs);
    });
    Scenario("not_found_refused", [] {
        world.scripts["StatSpellHaste"].found = false;
        Command("skillhaste 20");
        return "haste=" + Num(GameCall("StatSpellHaste"));
    });
    // Faster Cast Rate keeps its old behaviour through the shared table, and
    // every spelling the old command took still works.
    Scenario("castrate_unchanged", [] {
        std::string out;
        Command("castrate 40");            out += "a=" + Num(GameCall("StatFasterCastRate"));
        Command("fastercastrate 50");      out += " b=" + Num(GameCall("StatFasterCastRate"));
        Command("StatFasterCastRate 60");  out += " c=" + Num(GameCall("StatFasterCastRate"));
        return out;
    });
    Scenario("aliases", [] {
        std::string out;
        Command("spellhaste 10");      out += "a=" + Num(GameCall("StatSpellHaste"));
        Command("StatSpellHaste 20");  out += " b=" + Num(GameCall("StatSpellHaste"));
        Command("statallskills 3");    out += " c=" + Num(GameCall("StatAllSkills"));
        return out;
    });
    // A stat that comes back as a plain number is added to the same way.
    Scenario("scalar_result", [] {
        world.scalar = true;
        Command("skillhaste 100");
        return "haste=" + Num(GameCall("StatSpellHaste"));
    });
    Scenario("list", [] {
        Command("skillhaste 100");
        GameCall("StatSpellHaste");
        Command("allskills 19");
        Command("list");
        return std::string("listed");
    });
    Scenario("bad_input", [] {
        Command("foo 3");
        Command("skillhaste abc");
        return "installs=" + std::to_string(world.scripts["StatSpellHaste"].installs);
    });
    std::cout << "HARNESS DONE\n";
    return 0;
}
