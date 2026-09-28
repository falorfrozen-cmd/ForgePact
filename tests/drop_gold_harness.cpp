// Behavioral harness for `dropmult gold` (ForgePact issue #77).
//
// The Python runner splices the REAL plugin/include/ForgePact/DropManager.hpp
// in at the marker below (its #pragma and #include lines removed). Only the
// runtime it names is replaced: HookOneScript records each hook body by name
// and hands back a fake original, RValue carries the kinds the runner really
// passes, and RewardScope, Out and the BP_* macros are stand-ins. The player
// build's bodies are the ones compiled (FORGEPACT_RELEASE is defined).
//
// The game's own shape, read statically and measured in Live 1 (2026-09-27):
// DropMonsterGold computes an amount and calls DropGold once, directly, with
// nine arguments; DropGold creates one coin carrying that amount. Argument 4
// of DropGold is the one that varies per coin (51, 59, 31, 29 in the capture)
// while the others stay constant. The fake DropMonsterGold original below
// therefore calls the HOOKED DropGold once, the way the inline detour sees the
// game's direct call.
//
// Baseline: the other drop targets keep their count semantics (x3 runs the
// original three times), and one hooked DropMonsterGold call at x100 runs each
// gold original once. Before the fix that second scenario printed
// `monster_originals=100 gold_originals=10000`: the monster body ran its
// original 100 times and each of those reached the DropGold body, which ran
// its own 100 times - 10,000 coins, the freeze #77 reports. Target: one coin
// whose amount is x100, arguments untouched at x1 and inside a reward scope, a
// non-number amount left alone and counted, and one log line per session.
#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <vector>

// ---- minimal runtime stand-ins -------------------------------------------
// Kind numbers as YYToolkit declares them, so a kind printed here matches one
// printed by the plugin's own research lines.
enum RValueKind {
    VALUE_REAL = 0, VALUE_STRING = 1, VALUE_ARRAY = 2, VALUE_PTR = 3,
    VALUE_UNDEFINED = 5, VALUE_OBJECT = 6, VALUE_INT32 = 7, VALUE_INT64 = 10,
    VALUE_BOOL = 13, VALUE_REF = 15,
};

struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    RValue() = default;
    // As in YYToolkit: a floating value is REAL, a whole number is INT64.
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(int n) : m_Kind(VALUE_INT64), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(bool b) : m_Kind(VALUE_BOOL), number(b ? 1 : 0) {}
    double ToDouble() const { return number; }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return m_Kind == VALUE_STRING ? text : std::to_string(number); }
};
struct CInstance { int id = 0; };
using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);

struct World {
    std::map<std::string, void*> hooks;        // what HookOneScript was handed, by name
    bool rewardScope = false;
    RValue monsterAmount = RValue(51.0);       // what the fake DropMonsterGold passes at index 4
    long monsterOriginals = 0;
    long goldOriginals = 0;
    long itemOriginals = 0;
    std::vector<std::vector<RValue>> goldArgs; // the values each DropGold original saw
    std::vector<RValue**> goldArrays;          // the array each DropGold original was handed
    std::vector<std::string> log;
};
static World world;

// Every scaling line the whole process logged; World is reset per scenario,
// this is not.
static long g_ScalingLinesTotal = 0;
static bool IsScalingLine(const std::string& s) {
    return s.rfind("dropmult gold: x", 0) == 0 && s.find("applied to the coin's amount") != std::string::npos;
}
static void Out(const std::string& s) {
    if (IsScalingLine(s)) ++g_ScalingLinesTotal;
    world.log.push_back(s);
}
static std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
namespace HeroSiege { namespace RewardScope {
inline bool Active() { return world.rewardScope; }
}}

// The fake originals. DropGold's records what it was handed; DropMonsterGold's
// calls the hooked DropGold once with nine arguments, the direct call the
// detour intercepts.
static RValue& FakeDropGold(CInstance*, CInstance*, RValue& R, int argc, RValue** A) {
    ++world.goldOriginals;
    std::vector<RValue> seen;
    for (int i = 0; i < argc; ++i) seen.push_back(A && A[i] ? *A[i] : RValue());
    world.goldArgs.push_back(seen);
    world.goldArrays.push_back(A);
    return R;
}
static RValue g_GoldArgs[9];
static RValue& FakeDropMonsterGold(CInstance* S, CInstance* O, RValue& R, int, RValue**) {
    ++world.monsterOriginals;
    g_GoldArgs[0] = RValue("389f7fbc8e058d61efa91d591e1dc5c5ad418fec");
    g_GoldArgs[1] = RValue(11024.0);
    g_GoldArgs[2] = RValue(4528.0);
    g_GoldArgs[3] = RValue(1.0);
    g_GoldArgs[4] = world.monsterAmount;
    for (int i = 5; i < 9; ++i) g_GoldArgs[i] = RValue();
    RValue* args[9];
    for (int i = 0; i < 9; ++i) args[i] = &g_GoldArgs[i];
    auto hooked = reinterpret_cast<PFUNC_YYGMLScript>(world.hooks["DropGold"]);
    RValue r;
    if (hooked) hooked(S, O, r, 9, args);
    return R;
}
static RValue& FakeDropItem(CInstance*, CInstance*, RValue& R, int, RValue**) {
    ++world.itemOriginals;
    return R;
}
static RValue& FakeOther(CInstance*, CInstance*, RValue& R, int, RValue**) { return R; }

static bool HookOneScript(const char* name, const char*, void* hook, PFUNC_YYGMLScript* origOut) {
    world.hooks[name] = hook;
    const std::string n = name;
    *origOut = n == "DropGold" ? &FakeDropGold
             : n == "DropMonsterGold" ? &FakeDropMonsterGold
             : n == "DropItem" ? &FakeDropItem
             : &FakeOther;
    return true;
}

#define FORGEPACT_RELEASE 1
#define BP_ANGELIC_PROBE_SCOPE(name, s, argc, a) ((void)0)
#define BP_DIAG_INCREMENT(counter) ((void)0)
#define BP_LOGDROP(name, res, argc, argv) ((void)0)
#ifndef _MSC_VER
#define sprintf_s(buf, ...) std::snprintf(buf, sizeof(buf), __VA_ARGS__)
#endif

// PRODUCTION_DROPMANAGER

using ForgePact::DropManager;

// ---- scenarios ------------------------------------------------------------
static int g_Failures = 0;
static void Report(const std::string& label, bool ok, const std::string& detail) {
    std::cout << (ok ? "PASS " : "FAIL ") << label << " " << detail << "\n";
    if (!ok) ++g_Failures;
}

// DropManager is a process-wide singleton, as in the plugin: each scenario
// sets every multiplier it relies on and clears the world's record.
static void Fresh(int goldMult) {
    auto hooks = world.hooks;
    world = World();
    world.hooks = hooks;
    auto& mgr = DropManager::Instance();
    mgr.InstallHooks();
    mgr.SetMultiplier("item", 1);
    mgr.SetMultiplier("gold", goldMult);
    world.log.clear();
}

static void CallHooked(const char* name, int argc = 0, RValue** A = nullptr) {
    auto hooked = reinterpret_cast<PFUNC_YYGMLScript>(world.hooks[name]);
    RValue r;
    hooked(nullptr, nullptr, r, argc, A);
}

static std::string Counts() {
    return "monster_originals=" + std::to_string(world.monsterOriginals)
         + " gold_originals=" + std::to_string(world.goldOriginals);
}

static long ScalingLines() {
    long n = 0;
    for (const auto& line : world.log) if (IsScalingLine(line)) ++n;
    return n;
}

// The count of amounts the hook left alone. Written so this harness also
// compiles against the header before the fix, which has no such counter; that
// run reports the scenario as failing rather than not compiling at all.
template <typename M>
static long UnscaledCount(M& mgr, bool& present) {
    if constexpr (requires { mgr.GoldUnscaledCount(); }) { present = true; return mgr.GoldUnscaledCount(); }
    else { present = false; return -1; }
}

// Control: the other targets keep the count semantics the fix leaves alone.
static void BaselineOtherTargetsKeepCountSemantics() {
    Fresh(1);
    DropManager::Instance().SetMultiplier("item", 3);
    CallHooked("DropItem");
    Report("baseline/other_targets_keep_count_semantics", world.itemOriginals == 3,
           "item_originals=" + std::to_string(world.itemOriginals));
}

// #77's "before": the red run printed monster_originals=100 gold_originals=10000.
// The assertion names the fixed count, one of each.
static void BaselineX100CompoundsToTenThousand() {
    Fresh(100);
    CallHooked("DropMonsterGold");
    Report("baseline/x100_compounds_to_ten_thousand",
           world.monsterOriginals == 1 && world.goldOriginals == 1, Counts());
}

static void TargetX100ScalesOneCoin() {
    Fresh(100);
    CallHooked("DropMonsterGold");
    bool ok = world.monsterOriginals == 1 && world.goldOriginals == 1 && world.goldArgs.size() == 1;
    std::string detail = Counts();
    if (ok) {
        const auto& seen = world.goldArgs[0];
        ok = seen.size() == 9
          && seen[4].m_Kind == VALUE_REAL && seen[4].number == 5100.0
          && seen[0].m_Kind == VALUE_STRING && seen[0].text == "389f7fbc8e058d61efa91d591e1dc5c5ad418fec"
          && seen[1].number == 11024.0 && seen[2].number == 4528.0 && seen[3].number == 1.0;
        for (int i = 5; ok && i < 9; ++i) ok = seen[i].m_Kind == VALUE_UNDEFINED;
        // The caller's own argument is never written: the hook hands the
        // original a copy of the array with one slot replaced.
        ok = ok && g_GoldArgs[4].number == 51.0 && world.goldArrays[0] != nullptr;
        detail += " a4=" + std::to_string(seen.size() > 4 ? seen[4].number : -1)
               + " caller_a4=" + std::to_string(g_GoldArgs[4].number);
    }
    Report("target/x100_scales_one_coin", ok, detail);
}

// A direct DropGold call (not through DropMonsterGold) is one coin too.
static void TargetDirectDropGoldIsOneCoin() {
    Fresh(100);
    RValue a[9] = { RValue("fp"), RValue(1.0), RValue(2.0), RValue(1.0), RValue(7.0) };
    RValue* args[9];
    for (int i = 0; i < 9; ++i) args[i] = &a[i];
    CallHooked("DropGold", 9, args);
    const bool ok = world.goldOriginals == 1 && world.goldArgs.size() == 1
                 && world.goldArgs[0][4].number == 700.0 && a[4].number == 7.0;
    Report("target/direct_drop_gold_is_one_coin", ok, Counts());
}

static void TargetX1PassesThrough() {
    Fresh(1);
    CallHooked("DropMonsterGold");
    bool ok = world.monsterOriginals == 1 && world.goldOriginals == 1 && world.goldArgs.size() == 1;
    if (ok) {
        const auto& seen = world.goldArgs[0];
        ok = seen[4].m_Kind == g_GoldArgs[4].m_Kind && seen[4].number == 51.0;
        ok = ok && ScalingLines() == 0;
    }
    Report("target/x1_passes_through", ok, Counts());
}

static void TargetRewardScopePassesThrough() {
    Fresh(100);
    world.rewardScope = true;
    CallHooked("DropMonsterGold");
    bool ok = world.monsterOriginals == 1 && world.goldOriginals == 1 && world.goldArgs.size() == 1;
    if (ok) ok = world.goldArgs[0][4].number == 51.0 && ScalingLines() == 0;
    Report("target/reward_scope_passes_through", ok, Counts());
}

static void TargetNonNumericAmountIsLeftAlone() {
    Fresh(100);
    bool present = false;
    const long before = UnscaledCount(DropManager::Instance(), present);
    world.monsterAmount = RValue("not a number");
    CallHooked("DropMonsterGold");
    const long after = UnscaledCount(DropManager::Instance(), present);
    bool ok = present && world.monsterOriginals == 1 && world.goldOriginals == 1 && world.goldArgs.size() == 1;
    if (ok) {
        const auto& seen = world.goldArgs[0];
        ok = seen[4].m_Kind == VALUE_STRING && seen[4].text == "not a number"
          && after - before == 1 && ScalingLines() == 0;
    }
    // An infinite amount is a number the hook must not scale either.
    if (ok) {
        world.monsterAmount = RValue(std::numeric_limits<double>::infinity());
        CallHooked("DropMonsterGold");
        const long again = UnscaledCount(DropManager::Instance(), present);
        ok = world.goldOriginals == 2 && std::isinf(world.goldArgs[1][4].number)
          && again - after == 1;
    }
    Report("target/non_numeric_amount_is_left_alone", ok,
           Counts() + " unscaled_delta=" + std::to_string(after - before)
           + (present ? "" : " (no GoldUnscaledCount)"));
}

// The line is once per session, and the process is the session: this runs
// before any other scenario scales, drops twice at x100 and sees one line.
// main() checks afterwards that no later scenario saw it again.
static void TargetFirstScalingLogsOnce() {
    Fresh(100);
    CallHooked("DropMonsterGold");
    CallHooked("DropMonsterGold");
    const long lines = ScalingLines();
    Report("target/first_scaling_logs_once", lines == 1 && g_ScalingLinesTotal == 1,
           "scaling_lines=" + std::to_string(lines) + " " + Counts());
    for (const auto& line : world.log) std::cout << "LOG " << line << "\n";
}

int main() {
    BaselineOtherTargetsKeepCountSemantics();
    TargetFirstScalingLogsOnce();
    BaselineX100CompoundsToTenThousand();
    TargetX100ScalesOneCoin();
    TargetDirectDropGoldIsOneCoin();
    TargetX1PassesThrough();
    TargetRewardScopePassesThrough();
    TargetNonNumericAmountIsLeftAlone();
    Report("target/scaling_line_is_not_repeated", g_ScalingLinesTotal == 1,
           "scaling_lines_total=" + std::to_string(g_ScalingLinesTotal));
    for (const auto& line : world.log) std::cout << "LOG " << line << "\n";
    std::cout << (g_Failures ? "RESULT FAIL " + std::to_string(g_Failures) : std::string("RESULT OK")) << "\n";
    return g_Failures ? 1 : 0;
}
