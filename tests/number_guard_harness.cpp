// Behavioral regression harness for the number guard (`numguard`, #230).
//
// The Python runner injects the REAL ForgePact::NumberGuard header, the REAL
// ForgePact::StatsManager class (whose levers arm the guard) and ModuleMain's
// real Lower, FirstToken and NumGuardCmd below. Only the game API is
// replaced. No game process, character, installed DLL or release asset is
// touched. It is compiled twice: once with FORGEPACT_RELEASE defined (the
// player build) and once without (the research build).
//
// The game's number shortener is played by GameCompress: it shows a number
// the way the static reading of SteamCompressNumber describes, and for a
// number it cannot show (100 trillion or more, a million or more below zero,
// NaN) it records the fail-fast that ends the real game instead of showing
// anything. The game reaches num_compressSteam through a method value, so
// only a native detour sees the call; a table-only install leaves the game
// calling its own function. Each scenario reports what the shortener was
// handed and what it showed, not only what the plugin logged.
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
// The kinds a number reaches the shortener as on this runner (REAL, INT32 and
// INT64), with YYToolkit's values, plus the ones it must leave alone.
enum {
    VALUE_REAL = 0, VALUE_STRING = 1, VALUE_ARRAY = 2, VALUE_UNDEFINED = 5,
    VALUE_INT32 = 7, VALUE_INT64 = 10, VALUE_BOOL = 13, VALUE_REF = 15
};

struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    std::shared_ptr<std::vector<RValue>> items;   // shared: GML arrays are references
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const {
        if (m_Kind == VALUE_REAL || m_Kind == VALUE_INT32 || m_Kind == VALUE_INT64) return number;
        throw std::runtime_error("ToDouble on a non-number");
    }
    std::string ToString() const { return text; }
};
struct CInstance { int id = 0; };
using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);

static RValue Kind(int kind, double number) { RValue v; v.m_Kind = kind; v.number = number; return v; }

struct FakeRunner {
    long builtinCalls = 0;
    RValue CallBuiltin(const char* name, std::initializer_list<RValue> list) {
        ++builtinCalls;
        std::vector<RValue> a(list);
        const std::string n = name;
        if (n == "array_length") return RValue((double)a[0].items->size());
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
    std::vector<std::string> handed;    // every number the shortener was given
    int failFast = 0;                   // calls that would have ended the game
    bool markCalls = false;             // log a GAME line as the shortener starts
    std::string secondArg = "-";        // what a second argument read, when given
    double damage = 1234.0;             // what CalculateEndDamage builds
};
static World world;

static void Out(const std::string& s) { world.log.push_back(s); }

static std::string Num(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char b[48];
    std::snprintf(b, sizeof b, "%.15g", v);
    return b;
}

// The game's condition, written from the static reading on its own (not from
// NumberGuard.hpp's constants), so a wrong constant there cannot hide here.
static bool GameFails(double x) { return std::isnan(x) || x >= 1e14 || x <= -1e6; }

// What SteamCompressNumber shows for a number it can show (static reading).
static std::string Shorten(double x) {
    if (x == 0.0) return "0";
    if (x < 0.0) return "-" + std::to_string((long long)std::trunc(-x));
    if (x < 10000.0) return std::to_string((long long)std::trunc(x));
    long long q = 0;
    const char* suffix = "B";
    if (x < 1e6) { q = (long long)std::trunc(x / 100.0); suffix = "K"; }
    else if (x < 1e9) { q = (long long)std::trunc(x / 1e5); suffix = "M"; }
    else q = (long long)std::trunc(x / 1e8);
    std::string s = std::to_string(q / 10);
    if (q % 10) s += "," + std::to_string(q % 10);
    return s + suffix;
}

static RValue& GameCompress(CInstance*, CInstance*, RValue& R, int argc, RValue** A) {
    if (argc > 1 && A[1]) world.secondArg = Num(A[1]->number);
    if (!A || argc < 1 || !A[0]) { R = RValue(""); return R; }
    if (A[0]->m_Kind == VALUE_STRING) { R = RValue(A[0]->text); return R; }
    const double x = A[0]->ToDouble();
    world.handed.push_back(Num(x));
    if (world.markCalls) Out("GAME num_compressSteam " + Num(x));
    if (GameFails(x)) { ++world.failFast; R = RValue("FAILFAST"); return R; }
    R = RValue(Shorten(x));
    return R;
}
static RValue& GameEndDamage(CInstance*, CInstance*, RValue& R, int, RValue**) { R = RValue(world.damage); return R; }
static RValue& GameStatNumber(CInstance*, CInstance*, RValue& R, int, RValue**) { R = RValue(10.0); return R; }
static RValue& GameStatPoints(CInstance*, CInstance*, RValue& R, int, RValue**) { R = RValue(12.0); return R; }

static void ResetWorld() {
    world = World{};
    world.scripts["num_compressSteam"].game = GameCompress;
    world.scripts["CalculateEndDamage"].game = GameEndDamage;
    for (const char* name : { "StatMagicFind", "StatAttackSpeed", "StatExperienceGain", "StatMovementSpeed",
                              "StatExtraGold", "StatLifeReplenish", "StatManaReplenish", "StatDefense",
                              "StatCritDamage", "StatCritRate", "StatSpellCritDamage", "StatSpellCritRate",
                              "EnemyCalculateExperience" })
        world.scripts[name].game = GameStatNumber;
    for (const char* name : { "StatFasterCastRate", "StatSpellHaste", "StatAllSkills" })
        world.scripts[name].game = GameStatPoints;
}

// ModuleMain's installer, reduced to its contract: the first install saves
// the original (the trampoline on the native route, the table entry
// otherwise) and reports whether the native detour went in.
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

static consteval const char* SdkShortScriptName(std::string_view sdkConstant) {
    return sdkConstant.starts_with("gml_Script_") ? sdkConstant.substr(11).data()
                                                  : throw "SdkShortScriptName: missing the gml_Script_ prefix";
}

namespace HeroSiege { namespace RewardScope {
inline bool Active() { return false; }
inline void SetForgePactXp(double) {}
}}

// The shared CombatText detour, reduced to the surface StatsManager touches;
// no scenario here sets the Experience slider.
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

// PRODUCTION_NUMBERGUARD

// PRODUCTION_STATSMANAGER

// PRODUCTION_NUMGUARDCMD

using ForgePact::StatsManager;

// ---- the game's calls -----------------------------------------------------
static PFUNC_YYGMLScript Route(const char* name) {
    Script& s = world.scripts[name];
    return s.detour ? reinterpret_cast<PFUNC_YYGMLScript>(s.detour) : s.game;
}

static bool g_ArgWritten = false;

// The game handing one value to num_compressSteam; returns what it showed.
// The caller's RValue and its pointer are compared before and after.
static std::string Compress(RValue arg, const RValue* second = nullptr) {
    const RValue before = arg;
    RValue extra = second ? *second : RValue();
    RValue* args[2] = { &arg, &extra };
    RValue R;
    RValue& r = Route("num_compressSteam")(nullptr, nullptr, R, second ? 2 : 1, args);
    const bool sameNumber = (std::isnan(arg.number) && std::isnan(before.number)) || arg.number == before.number;
    if (args[0] != &arg || arg.m_Kind != before.m_Kind || !sameNumber || arg.text != before.text) g_ArgWritten = true;
    return r.text;
}
static std::string CompressAll(const std::vector<RValue>& values) {
    std::string s;
    for (size_t i = 0; i < values.size(); ++i) s += (i ? "|" : "") + Compress(values[i]);
    return s;
}
// The same values straight to the game's own function, as with no mod.
static std::string Vanilla(const std::vector<RValue>& values) {
    std::string s;
    const std::vector<std::string> keep = world.handed;
    const int keepFail = world.failFast;
    for (size_t i = 0; i < values.size(); ++i) {
        RValue arg = values[i];
        RValue* args[1] = { &arg };
        RValue R;
        s += (i ? "|" : "") + GameCompress(nullptr, nullptr, R, 1, args).text;
    }
    world.handed = keep;
    world.failFast = keepFail;
    return s;
}
static double EndDamage() {
    RValue R;
    return Route("CalculateEndDamage")(nullptr, nullptr, R, 0, nullptr).ToDouble();
}

static std::string Handed() {
    std::string s;
    for (size_t i = 0; i < world.handed.size(); ++i) s += (i ? "|" : "") + world.handed[i];
    return s;
}
static std::string Installs(const char* name) { return std::to_string(world.scripts[name].installs); }

// ---- scenarios ------------------------------------------------------------
static StatsManager* g_Manager = nullptr;
static void Stat(const std::string& line) { g_Manager->HandleStatCommand(line); }
static void StatAdd(const std::string& line) { g_Manager->HandleStatAddCommand(line); }
static void Guard(const std::string& line) { NumGuardCmd(line); }
// The status line alone; the scenario's log is left as it was.
static std::string Status() {
    const size_t at = world.log.size();
    Guard("stat");
    const std::string line = world.log.size() > at ? world.log.back() : "";
    world.log.resize(at);
    return line;
}

static void ResetAll() {
    ResetWorld();
    g_ArgWritten = false;
    g_Runner.builtinCalls = 0;
    ForgePact::NumberGuard::state = ForgePact::NumberGuard::State{};
#ifndef FORGEPACT_RELEASE
    ForgePact::NumberGuard::installEndDamageTrace = nullptr;
#endif
    static StatsManager& instance = StatsManager::Instance();
    new (&instance) StatsManager();   // NOLINT: re-run the default member initializers
    g_Manager = &instance;
}

static void Print(const std::string& label, const std::string& fields) {
    std::cout << "SCENARIO " << label << " " << fields << "\n";
    for (const auto& l : world.log) std::cout << "LOG " << label << " :: " << l << "\n";
}

static void Scenario(const std::string& label, const std::function<std::string()>& body) {
    ResetAll();
    std::string fields;
    try { fields = body(); } catch (const std::exception& e) { fields = std::string("threw=") + e.what(); }
    Print(label, fields);
}

static const double kNaN = std::numeric_limits<double>::quiet_NaN();
static const double kInf = std::numeric_limits<double>::infinity();

int main() {
    // ---- baseline: vanilla stays vanilla ----------------------------------
    // Every lever at vanilla: nothing is hooked, and a number past the limit
    // still reaches the game's shortener as given (which fails on it: the
    // harness's own positive control for the crash condition).
    Scenario("baseline/not_armed_passes_through", [] {
        Stat("damage 1");
        StatAdd("skillhaste 0");
        const std::string shown = CompressAll({ RValue(5e13), RValue(2e14) });
        return "shown=" + shown + " handed=" + Handed() + " failfast=" + std::to_string(world.failFast)
             + " installs=" + Installs("num_compressSteam") + " written=" + (g_ArgWritten ? "yes" : "no");
    });
    // Armed: every number the shortener can show reaches it unchanged, and
    // shows exactly what it shows with no mod; a text goes through as given.
    Scenario("baseline/in_range_unchanged_when_armed", [] {
        Stat("damage 11");
        const std::vector<RValue> values = {
            RValue(0.0), RValue(1234.7), RValue(12345.0), RValue(999999.0), RValue(123456789.0),
            RValue(99999000000000.0), RValue(99999999999999.0), RValue(-5.0), RValue(-999999.0),
            Kind(VALUE_INT32, 42), Kind(VALUE_INT64, 5e13), RValue("abc"),
        };
        const std::string vanilla = Vanilla(values);
        const std::string shown = CompressAll(values);
        return "shown=" + shown + " vanilla=" + vanilla + " handed=" + Handed()
             + " failfast=" + std::to_string(world.failFast) + " written=" + (g_ArgWritten ? "yes" : "no");
    });

    // ---- target: past the limit, the largest the shortener can show --------
    Scenario("target/positive_overflow_capped", [] {
        Stat("damage 11");
        const std::string shown = CompressAll({ RValue(1e14), RValue(123456789012345.0), RValue(1.5e17),
                                                RValue(1e300), RValue(kInf), Kind(VALUE_INT64, 5e15) });
        return "shown=" + shown + " handed=" + Handed() + " failfast=" + std::to_string(world.failFast);
    });
    Scenario("target/negative_overflow_capped", [] {
        Stat("damage 11");
        const std::string shown = CompressAll({ RValue(-1e6), RValue(-2.5e9), RValue(-kInf),
                                                Kind(VALUE_INT32, -2000000), Kind(VALUE_INT64, -1e18) });
        return "shown=" + shown + " handed=" + Handed() + " failfast=" + std::to_string(world.failFast);
    });
    Scenario("target/nan_capped_to_zero", [] {
        Stat("damage 11");
        const std::string shown = CompressAll({ RValue(kNaN) });
        return "shown=" + shown + " handed=" + Handed() + " failfast=" + std::to_string(world.failFast);
    });
    // The cap goes into a substitute array: the caller's RValue, its pointer
    // and any other argument are what the caller gave.
    Scenario("target/caller_argument_never_written", [] {
        Stat("damage 11");
        Compress(RValue(1e15));
        Compress(Kind(VALUE_INT64, 5e15));
        Compress(RValue(kNaN));
        const RValue second(7.0);
        const std::string shown = Compress(RValue(-3e6), &second);
        return "written=" + std::string(g_ArgWritten ? "yes" : "no") + " second=" + world.secondArg
             + " shown=" + shown + " handed=" + Handed();
    });
    // Each Modifiers lever arms the guard; vanilla values do not; the detour
    // goes in once however many levers ask.
    Scenario("target/arms_from_levers_once", [] {
        std::string out;
        Stat("damage 1");
        StatAdd("skillhaste 0");
        StatAdd("allskills -3");
        out += "vanilla=" + Installs("num_compressSteam");
        Stat("damage 11");
        out += " damage=" + Installs("num_compressSteam");
        Stat("critdamage 11");
        Stat("critchance 6");
        StatAdd("skillhaste 10");
        Stat("damage 1");
        out += " all=" + Installs("num_compressSteam");
        out += " shown=" + Compress(RValue(2e14));
        out += " armed=" + std::string(Status().rfind("numguard: armed route=native ", 0) == 0 ? "yes" : "no");
        ResetAll();
        Stat("critchance 6");
        out += " critchance=" + Installs("num_compressSteam");
        ResetAll();
        StatAdd("skillhaste 10");
        out += " statadd=" + Installs("num_compressSteam");
        ResetAll();
        world.scripts["StatSpellHaste"].nativeWorks = false;
        StatAdd("skillhaste 10");
        out += " refusedstatadd=" + Installs("num_compressSteam");
        ResetAll();
        world.scripts["CalculateEndDamage"].found = false;
        Stat("damage 11");
        out += " failedstat=" + Installs("num_compressSteam");
        return out;
    });
    // A table-only install is not armed: the game's calls go round it.
    Scenario("target/table_only_install_not_armed", [] {
        world.scripts["num_compressSteam"].nativeWorks = false;
        Stat("damage 11");
        Stat("critdamage 11");
        const std::string shown = Compress(RValue(2e14));
        const std::string status = Status();
        Out(status);
        return "shown=" + shown + " failfast=" + std::to_string(world.failFast)
             + " installs=" + Installs("num_compressSteam");
    });
    Scenario("target/failed_install_not_armed", [] {
        world.scripts["num_compressSteam"].found = false;
        Stat("damage 11");
        StatAdd("skillhaste 10");
        const std::string status = Status();
        Out(status);
        return "installs=" + Installs("num_compressSteam");
    });
    // A failed install is tried again at the next arming: two failed resolves
    // write the "not capped" line once, the third arming installs natively,
    // and a native route is kept for the session (a fourth arming installs
    // nothing).
    Scenario("target/failed_install_retried_on_next_arm", [] {
        world.scripts["num_compressSteam"].found = false;
        Stat("damage 11");
        StatAdd("skillhaste 10");
        const std::string failed = Installs("num_compressSteam");
        world.scripts["num_compressSteam"].found = true;
        Stat("critdamage 11");
        const std::string retried = Installs("num_compressSteam");
        Stat("critchance 6");
        const std::string shown = Compress(RValue(2e14));
        const std::string status = Status();
        Out(status);
        return "failed=" + failed + " retried=" + retried + " after=" + Installs("num_compressSteam")
             + " shown=" + shown + " failfast=" + std::to_string(world.failFast)
             + " armed=" + (status.rfind("numguard: armed route=native ", 0) == 0 ? "yes" : "no");
    });
    // Both builds count the calls the detour saw, so "it never ran" and
    // "nothing needed capping" read differently in a player's report.
    Scenario("target/calls_counted_in_player_build", [] {
        Stat("damage 11");
        Compress(RValue(42.0));
        Compress(RValue(2e14));
        Out(Status());
        return "failfast=" + std::to_string(world.failFast);
    });
    // Every cap is counted; the first one of a session writes one line.
    Scenario("target/first_cap_logged_once", [] {
        Stat("damage 11");
        Compress(RValue(2e14));
        Compress(RValue(3e15));
        Compress(RValue(-5e6));
        Compress(RValue(42.0));
        Out(Status());
        return "failfast=" + std::to_string(world.failFast);
    });
    // The command's surface: status in both builds, research verbs refused in
    // the player build.
    Scenario("target/command_surface", [] {
        Guard("");
        Guard("stat");
        Guard("off");
        Guard("trace on");
        Guard("auto");
        return "installs=" + Installs("num_compressSteam") + " enddamage=" + Installs("CalculateEndDamage");
    });

#ifndef FORGEPACT_RELEASE
    // ---- research build only ------------------------------------------------
    // `numguard off` lets every value through for the session, still counted;
    // `numguard auto` puts the cap back.
    Scenario("research/forced_off_passes_through", [] {
        Stat("damage 11");
        Guard("off");
        const std::string off = Compress(RValue(2e14));
        Out(Status());
        Guard("auto");
        const std::string on = Compress(RValue(2e14));
        Out(Status());
        return "off=" + off + " auto=" + on + " failfast=" + std::to_string(world.failFast) + " handed=" + Handed();
    });
    // The trace writes a value's line before the shortener runs, so the last
    // line before a fail-fast names the value that ended the game.
    Scenario("research/trace_line_before_original", [] {
        world.markCalls = true;
        Guard("trace on");
        Compress(RValue(5.0));
        Compress(RValue(50.0));
        Compress(RValue(60.0));
        Compress(RValue(5000.0));
        Compress(RValue(2e14));
        const double native = EndDamage();
        Stat("damage 11");
        const double scaled = EndDamage();
        Guard("trace off");
        Compress(RValue(5e13));
        Out(Status());
        return "installs=" + Installs("num_compressSteam") + " enddamage=" + Installs("CalculateEndDamage")
             + " native=" + Num(native) + " scaled=" + Num(scaled) + " failfast=" + std::to_string(world.failFast);
    });
    // With the cap forced off, a failing negative after a big positive of the
    // same magnitude still writes its line before the shortener fails on it:
    // the trace keeps a separate mark per sign, so 1e9 cannot hide -2e9.
    Scenario("research/trace_negative_after_positive", [] {
        world.markCalls = true;
        Guard("off");
        Guard("trace on");
        Compress(RValue(1e9));
        const std::string shown = Compress(RValue(-2e9));
        return "shown=" + shown + " handed=" + Handed() + " failfast=" + std::to_string(world.failFast);
    });
    // `numguard trace on` names the route CalculateEndDamage's hook took, not
    // only whether it is in: table-only when the trace installs it, table-only
    // when `stat damage` installed it first, failed when it cannot resolve.
    Scenario("research/trace_reply_names_end_damage_route", [] {
        const auto damageRoute = [] {
            const std::string open = "numguard trace -> on (";
            const std::string key = "CalculateEndDamage ";
            for (auto it = world.log.rbegin(); it != world.log.rend(); ++it) {
                if (it->rfind(open, 0) != 0) continue;
                const size_t at = it->find(key);
                if (at == std::string::npos) return std::string("?");
                const std::string rest = it->substr(at + key.size());
                return rest.substr(0, rest.find(')'));
            }
            return std::string("-");
        };
        world.scripts["CalculateEndDamage"].nativeWorks = false;
        Guard("trace on");
        const std::string fresh = damageRoute();
        const std::string table = Installs("CalculateEndDamage");
        ResetAll();
        world.scripts["CalculateEndDamage"].nativeWorks = false;
        Stat("damage 11");
        Guard("trace on");
        const std::string prior = damageRoute();
        const std::string priorInstalls = Installs("CalculateEndDamage");
        ResetAll();
        world.scripts["CalculateEndDamage"].found = false;
        Guard("trace on");
        const std::string failed = damageRoute();
        ResetAll();
        Guard("trace on");
        const std::string native = damageRoute();
        return "fresh=" + fresh + " freshinstalls=" + table + " prior=" + prior + " priorinstalls=" + priorInstalls
             + " failed=" + failed + " native=" + native;
    });
    // A non-finite value writes a line each time; the trace stops at 400
    // lines a session and says so once.
    Scenario("research/trace_limit", [] {
        Guard("trace on");
        for (int i = 0; i < 450; ++i) Compress(RValue(kNaN));
        Compress(RValue(kInf));
        int lines = 0, limit = 0;
        for (const auto& l : world.log) {
            if (l == "numguard trace: limit reached") ++limit;
            else if (l.rfind("numguard trace: num_compressSteam ", 0) == 0) ++lines;
        }
        world.log.clear();
        return "lines=" + std::to_string(lines) + " limit=" + std::to_string(limit);
    });
#endif
    std::cout << "HARNESS DONE\n";
    return 0;
}
