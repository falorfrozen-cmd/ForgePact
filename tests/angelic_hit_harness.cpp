// The Python test (test_angelic_hit_behavior.py) inserts the real plugin functions below,
// compiled as the player build (FORGEPACT_RELEASE). Only the game is modelled: the game's
// Angelic roll (DropItemAngelicChance), the game's CreateDefaultParams, the script table
// HookOneScript installs into, the pool and the spawn. No game process, character,
// installed DLL or release asset is touched.
//
// The model follows the static reading (docs/angelic-roll-hook-research.md, Session 2): the
// roll returns undefined on a hit exactly as on a miss, and only a hit calls
// CreateDefaultParams - by a direct call, so it reaches ForgePact only through a hook
// installed on that function itself.
#define FORGEPACT_RELEASE
#include <atomic>
#include <cmath>
#include <cstdint>
#include <deque>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_UNSET };
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    CInstance* instance = nullptr;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    double ToDouble() const {
        if (m_Kind != VALUE_REAL) throw std::runtime_error("not a number");
        return number;
    }
    bool ToBoolean() const { return number != 0; }
};
struct CInstance {
    bool alive = true;
    double x = 0, y = 0;
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};
using PVOID = void*;
using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);
static long InterlockedIncrement(volatile long* value) { return ++*const_cast<long*>(value); }
namespace HeroSiege::Scripts {
inline constexpr std::string_view gml_Script_DropItemAngelicChance = "gml_Script_DropItemAngelicChance";
inline constexpr std::string_view gml_Script_CreateDefaultParams = "gml_Script_CreateDefaultParams";
}
static constexpr const char* SdkShortScriptName(std::string_view sdkConstant) { return sdkConstant.data() + 11; }

// ---- log ---------------------------------------------------------------------------------
static std::vector<std::string> outLines;
static void Out(const std::string& line) { outLines.push_back(line); }
static int linesStartingWith(const char* prefix) {
    int n = 0;
    for (const auto& l : outLines) if (l.rfind(prefix, 0) == 0) ++n;
    return n;
}

// ---- what the production functions read ------------------------------------------------------
static std::atomic<bool> g_HhEnabled{ false };
static std::atomic<bool> g_TyEnabled{ false };
static std::mt19937& TyRng() { static std::mt19937 rng{ 7 }; return rng; }
static double HhReadNumber(const RValue& value, const char* field, double fallback) {
    CInstance* instance = value.instance;
    if (!instance) return fallback;
    const std::string name(field);
    if (name == "x") return instance->x;
    if (name == "y") return instance->y;
    return fallback;
}
// The slider's pool: N ordinary validated uniques, N under the scenario's control.
struct AngelicCandidate { int type, sub, b; std::string name; bool angelic; };
static std::vector<AngelicCandidate> g_AngelicPool;
static size_t poolSize = 49;
static int poolBuilds = 0;
static void BuildAngelicPool(bool) {
    ++poolBuilds;
    if (!g_AngelicPool.empty()) return;
    for (size_t i = 0; i < poolSize; ++i) g_AngelicPool.push_back({ 3, 1, (int)i, "unique", true });
}
// Spawn recorder: which item, where, with which self, and whether the game's own roll had
// already returned (its own item placed) when the signature item was spawned.
struct SpawnRecord { int which; double x, y; CInstance* self; bool selfAlive; int originalReturnsBefore; };
static std::vector<SpawnRecord> spawns;
static int originalReturns = 0;
static bool SpawnSignatureItem(int which, double x, double y, CInstance* ctx) {
    spawns.push_back({ which, x, y, ctx, ctx && ctx->alive, originalReturns });
    return true;
}

// ---- the game: CreateDefaultParams, the script table, DropItemAngelicChance -------------------
static int gameCdpCalls = 0;
static RValue& gameCreateDefaultParams(CInstance*, CInstance*, RValue& result, int, RValue**) {
    ++gameCdpCalls;
    result = RValue(1.0);
    return result;
}
// Where the roll's direct call lands: the game's own function, or a hook installed on it.
static PFUNC_YYGMLScript cdpEntry = gameCreateDefaultParams;

static std::deque<bool> outcomes;          // per original call: true = hit
static int originalCalls = 0;
static bool originalThrows = false;
static double lastChanceSeen = -1;
static RValue originalReturn;              // what the game's roll hands back: undefined, always
static RValue& gameAngelicChance(CInstance* self, CInstance* other, RValue&, int argc, RValue** A) {
    ++originalCalls;
    lastChanceSeen = (argc > 2 && A && A[2] && A[2]->m_Kind == VALUE_REAL) ? A[2]->number : -1;
    if (originalThrows) throw std::runtime_error("the game's roll threw");
    const bool hit = !outcomes.empty() && outcomes.front();
    if (!outcomes.empty()) outcomes.pop_front();
    if (hit) {   // only a hit builds the params, by a direct call
        RValue sub(1.0), b(15.0), c(1.0), params;
        RValue* args[] = { &sub, &b, &c };
        cdpEntry(self, other, params, 3, args);
    }
    originalReturn = RValue();
    ++originalReturns;
    return originalReturn;
}

// HookOneScript into a fake table: one install per name, a second call answers from the table.
static std::map<std::string, int> installs;
static bool HookOneScript(const char* shortName, const char* /*id*/, PVOID dest, PFUNC_YYGMLScript* origOut, bool* nativeOut = nullptr) {
    const std::string name(shortName);
    ++installs[name];
    if (nativeOut) *nativeOut = true;
    if (name == "CreateDefaultParams") { *origOut = gameCreateDefaultParams; cdpEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest); return true; }
    if (name == "DropItemAngelicChance") { *origOut = gameAngelicChance; return true; }
    return false;
}

// ---- globals the production hook and its helpers own -------------------------------------------
static PFUNC_YYGMLScript g_OrigAngChance = nullptr;
static double g_AngelicRateMult = 1.0;
static bool g_InAngelicExtra = false;
static volatile long g_AngRateHits = 0;
static volatile long g_SigGameRolls = 0, g_SigGameHits = 0, g_SigShareRolls = 0;
static volatile long g_SigFromGame = 0, g_SigFromGameCrown = 0, g_SigFromGameBelt = 0;
static PFUNC_YYGMLScript g_Orig_CreateDefaultParams = nullptr;
static thread_local int g_SigRollDepth = 0;
static thread_local bool g_SigHitSeen = false;
static double g_SigLastSub = -1.0, g_SigLastB = -1.0;

// PRODUCTION_FUNCTIONS

static void reset() {
    outLines.clear();
    g_HhEnabled = false; g_TyEnabled = false;
    g_AngelicPool.clear(); poolSize = 49; poolBuilds = 0;
    spawns.clear(); originalReturns = 0;
    gameCdpCalls = 0; cdpEntry = gameCreateDefaultParams;
    outcomes.clear(); originalCalls = 0; originalThrows = false; lastChanceSeen = -1;
    installs.clear();
    g_OrigAngChance = gameAngelicChance;   // as if `raredrop angelic` (or the switch) had installed it
    g_AngelicRateMult = 1.0; g_InAngelicExtra = false; g_AngRateHits = 0;
    g_SigGameRolls = 0; g_SigGameHits = 0; g_SigShareRolls = 0;
    g_SigFromGame = 0; g_SigFromGameCrown = 0; g_SigFromGameBelt = 0;
    g_Orig_CreateDefaultParams = nullptr; g_SigRollDepth = 0; g_SigHitSeen = false;
    g_SigLastSub = -1.0; g_SigLastB = -1.0;
}
static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
// What a switch turning on does: install the detection. Absent from a source without it.
static void installDetection() {
#ifdef HAS_INSTALLSIGNATUREANGELICHOOKS
    InstallSignatureAngelicHooks();
#endif
}
// One call of the game's roll as DropItem makes it: x, y, the chance, an undefined argument 3.
static RValue* roll(CInstance& monster, std::initializer_list<bool> perOriginalCall, double x = 640.5, double y = 320.25, bool realPosition = true) {
    outcomes.assign(perOriginalCall.begin(), perOriginalCall.end());
    static RValue a0, a1, a2, a3, result;
    a0 = realPosition ? RValue(x) : RValue();
    a1 = realPosition ? RValue(y) : RValue();
    a2 = RValue(1500.0);
    a3 = RValue();
    RValue* args[] = { &a0, &a1, &a2, &a3 };
    return &HookAngelicChance(&monster, nullptr, result, 4, args);
}
static int countWhich(int which) {
    int n = 0;
    for (const auto& s : spawns) if (s.which == which) ++n;
    return n;
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    CInstance monster;
    monster.x = 10; monster.y = 20;
    reset();
    const std::string test = argv[1];
    try {
        // ---- baseline: holds against the pre-#74 source too ----
        if (test == "both_off_miss_passthrough") {
            RValue* r = roll(monster, { false });
            require(originalCalls == 1, "the game's roll was not called exactly once");
            require(lastChanceSeen == 1500.0, "argument 2 (the chance) reached the game changed");
            require(r == &originalReturn, "the hook did not hand back the game's own return value");
            require(r->m_Kind == VALUE_UNDEFINED, "the return value was not the game's undefined");
            require(spawns.empty(), "a miss with both switches off spawned something");
        } else if (test == "both_off_hit_passthrough") {
            installDetection();   // e.g. a research lever: detection without a switch
            roll(monster, { true });
            require(originalCalls == 1 && gameCdpCalls == 1, "the game's roll or its CreateDefaultParams did not run once");
            require(spawns.empty(), "a hit with both switches off spawned a signature item");
            require(g_SigFromGame == 0, "a hit with both switches off counted a signature drop");
        } else if (test == "default_params_outside_roll_not_a_hit") {
#ifdef HAS_HOOK_CREATEDEFAULTPARAMS
            g_HhEnabled = true; g_TyEnabled = true;
            installDetection();
            RValue sub(1.0), b(15.0), c(1.0), params;
            RValue* args[] = { &sub, &b, &c };
            cdpEntry(&monster, nullptr, params, 3, args);   // DropItem building an ordinary drop
            require(gameCdpCalls == 1, "CreateDefaultParams outside the roll did not reach the game's own function");
            roll(monster, { false });
            require(g_SigGameHits == 0, "a CreateDefaultParams call outside the roll counted as a hit");
            require(spawns.empty(), "a CreateDefaultParams call outside the roll spawned a signature item");
#endif
        } else if (test == "extra_rolls_still_run") {
            g_AngelicRateMult = 3.0;
            roll(monster, { false, false, false });
            require(originalCalls == 3, "rate x3 did not run the game's roll three times");
            require(spawns.empty(), "misses spawned something");
        // ---- target: every scenario fails against the pre-#74 source ----
        } else if (test == "headhunter_on_hit_spawns_only_belt") {
            // N = 1: the share is 1 in 2 (N = 0 rolls nothing), so 300 hits give a band of drops.
            poolSize = 1; g_HhEnabled = true;
            installDetection();
            for (int i = 0; i < 300; ++i) roll(monster, { true });
            require(g_SigGameHits == 300, "hits were not detected: gameHits=" + std::to_string(g_SigGameHits));
            require(g_SigShareRolls == 300, "not every hit rolled the share once");
            require(spawns.size() >= 110 && spawns.size() <= 190, "one-switch share at N=1 is not about 1 in 2: " + std::to_string(spawns.size()));
            require(countWhich(1) == (int)spawns.size() && countWhich(0) == 0, "Headhunter alone on dropped a Tyrant's Crown");
            require(g_SigFromGameBelt == (long)spawns.size() && g_SigFromGameCrown == 0 && g_SigFromGame == (long)spawns.size(), "counters do not match the spawns");
        } else if (test == "tyrant_on_hit_spawns_only_crown") {
            poolSize = 1; g_TyEnabled = true;
            installDetection();
            for (int i = 0; i < 300; ++i) roll(monster, { true });
            require(g_SigGameHits == 300, "hits were not detected: gameHits=" + std::to_string(g_SigGameHits));
            require(spawns.size() >= 110 && spawns.size() <= 190, "one-switch share at N=1 is not about 1 in 2: " + std::to_string(spawns.size()));
            require(countWhich(0) == (int)spawns.size() && countWhich(1) == 0, "Tyrant's Crown alone on dropped a Headhunter");
            require(g_SigFromGameCrown == (long)spawns.size() && g_SigFromGameBelt == 0, "counters do not match the spawns");
        } else if (test == "both_on_equal_share") {
            poolSize = 49; g_HhEnabled = true; g_TyEnabled = true;
            installDetection();
            for (int i = 0; i < 51000; ++i) roll(monster, { true });
            const int crowns = countWhich(0), belts = countWhich(1);
            require(spawns.size() >= 1700 && spawns.size() <= 2300, "two switches at N=49 did not give about 2 in 51: " + std::to_string(spawns.size()));
            require(crowns >= 850 && crowns <= 1150, "Tyrant's Crown did not get half: " + std::to_string(crowns));
            require(belts >= 850 && belts <= 1150, "Headhunter did not get half: " + std::to_string(belts));
            require(linesStartingWith("angelic hit:") == 51000, "not one `angelic hit:` line per hit");
        } else if (test == "miss_never_spawns") {
            g_HhEnabled = true; g_TyEnabled = true;
            installDetection();
            for (int i = 0; i < 1000; ++i) roll(monster, { false });
            require(g_SigGameRolls == 1000, "game rolls were not counted: gameRolls=" + std::to_string(g_SigGameRolls));
            require(g_SigGameHits == 0 && g_SigShareRolls == 0, "a miss counted as a hit");
            require(spawns.empty() && gameCdpCalls == 0, "a miss spawned something");
            require(linesStartingWith("angelic hit:") == 0, "a miss logged a hit");
        } else if (test == "spawn_at_roll_position_with_monster_self") {
            poolSize = 1; g_HhEnabled = true;
            installDetection();
            for (int i = 0; i < 40 && spawns.empty(); ++i) roll(monster, { true }, 640.5, 320.25);
            require(spawns.size() == 1, "no spawn in 40 hits at a 1-in-2 share");
            const SpawnRecord s = spawns[0];
            require(s.x == 640.5 && s.y == 320.25, "not spawned at the roll's own x, y (arguments 0 and 1)");
            require(s.self == &monster && s.selfAlive, "not spawned with the live dying monster as self");
            require(s.originalReturnsBefore == originalReturns, "spawned before the game's own roll had returned");
            spawns.clear();
            for (int i = 0; i < 40 && spawns.empty(); ++i) roll(monster, { true }, 0, 0, false);
            require(spawns.size() == 1 && spawns[0].x == 10 && spawns[0].y == 20, "without real x, y arguments the monster's own x, y was not used");
        } else if (test == "hit_in_extra_roll_counts") {
            poolSize = 1; g_HhEnabled = true; g_AngelicRateMult = 2.0;
            installDetection();
            for (int i = 0; i < 100; ++i) roll(monster, { false, true });
            require(originalCalls == 200, "rate x2 did not run the game's roll twice per kill");
            require(g_SigGameRolls == 200, "extra rolls were not counted as game rolls");
            require(g_SigGameHits == 100, "a hit in the extra roll was not detected: gameHits=" + std::to_string(g_SigGameHits));
            require(!spawns.empty() && countWhich(1) == (int)spawns.size(), "a hit in the extra roll did not roll the share");
        } else if (test == "switch_off_after_on_passes_through") {
            poolSize = 1; g_HhEnabled = true;
            installDetection();
            for (int i = 0; i < 100; ++i) roll(monster, { true });
            require(!spawns.empty(), "switched on, 100 hits spawned nothing");
            const size_t before = spawns.size();
            const int installsBefore = installs["CreateDefaultParams"];
            g_HhEnabled = false;
            for (int i = 0; i < 100; ++i) roll(monster, { true });
            require(spawns.size() == before, "switched off, a hit still spawned a signature item");
            require(installs["CreateDefaultParams"] == installsBefore && cdpEntry != gameCreateDefaultParams, "switching off removed or reinstalled a hook");
            require(originalCalls == 200 && gameCdpCalls == 200, "switched off, the game's own roll did not run untouched");
        } else if (test == "original_throw_lowers_roll_flag") {
            poolSize = 1; g_HhEnabled = true;
            installDetection();
            originalThrows = true;
            bool threw = false;
            try { roll(monster, { true }); } catch (const std::runtime_error&) { threw = true; }
            require(threw, "the game's exception did not reach the caller");
            require(g_SigRollDepth == 0, "the roll-in-progress state stayed raised after a throw");
            originalThrows = false;
            RValue sub(1.0), b(15.0), c(1.0), params;
            RValue* args[] = { &sub, &b, &c };
            cdpEntry(&monster, nullptr, params, 3, args);   // after the throw, outside any roll
            require(!g_SigHitSeen, "a CreateDefaultParams call after a throw, outside any roll, was taken for a hit");
            roll(monster, { false });
            require(g_SigGameHits == 0 && spawns.empty(), "the roll-in-progress state stayed raised after a throw");
            for (int i = 0; i < 40 && spawns.empty(); ++i) roll(monster, { true });
            require(g_SigGameHits >= 1 && !spawns.empty(), "after a throw, a real hit was no longer detected");
        } else if (test == "install_is_idempotent") {
            g_OrigAngChance = nullptr;
#ifdef HAS_INSTALLSIGNATUREANGELICHOOKS
            InstallSignatureAngelicHooks();
            InstallSignatureAngelicHooks();
#endif
            require(installs["CreateDefaultParams"] == 1, "CreateDefaultParams was not hooked exactly once");
            require(installs["DropItemAngelicChance"] == 1, "DropItemAngelicChance was not hooked exactly once");
            require(g_OrigAngChance == gameAngelicChance, "the roll hook's saved original is not the game's roll");
            reset();
            installDetection();
            require(installs["DropItemAngelicChance"] == 0, "an already-held roll hook was installed again");
            RValue* r = roll(monster, { true });
            require(r == &originalReturn, "the return value is not the game's own");
        } else return 2;
        std::cout << "PASS " << test << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << test << ": " << error.what() << '\n';
        return 1;
    }
}
