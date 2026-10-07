// Behavioral harness for "Fill the map as you approach" (`fillroll`, #183).
//
// The Python runner injects the REAL ForgePact::RollingFill and
// ForgePact::MapRevealManager classes and the REAL Hook_distance_to_object
// body below. Only the game API is replaced; no game process, character,
// installed DLL or release asset is touched.
//
// The world is positional: every creator and the local player have x and y,
// and distance_to_object's native answer is the real distance between them,
// so "held back for distance" means what it says. The fakes count what the
// rule costs the runner (local-player resolves, position reads, admission
// slots) because none of that is visible in the answer itself.
//
// Compiled the way the PLAYER build sees the hook (see map_reveal_harness.cpp).
#define FORGEPACT_RELEASE 1
#include <atomic>
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_BOOL };
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    CInstance* instance = nullptr;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    double ToDouble() const { return number; }
    int64_t ToInt64() const { return (int64_t)number; }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
    CInstance* ToInstance() const { return instance; }
};
#ifndef NULL_INDEX
#define NULL_INDEX INT_MIN
#endif
struct CInstance {
    int id = 0;
    int object = 0;
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};
using AurieStatus = int;
static bool AurieSuccess(int s) { return s == 0; }

// ---- the controlled world -------------------------------------------------
static constexpr int kCreatorObject = 9200;
static constexpr int kOtherObject = 9300;
// The local player's handle. The runner resolves it as a REF, not an object
// (AGENTS.md, "Accept the kinds the runtime actually produces").
static constexpr double kPlayerHandle = 100001.0;

struct Creator { int id; bool timerDefined; double x; double y; };
struct World {
    int64_t room = 100;
    double minimapInstance = 7000;
    double grid = 500;
    bool playerObjectPresent = true;   // instance_number(Player_obj)
    bool localPlayerResolves = true;   // HhResolveLocalPlayer
    double playerX = 0, playerY = 0;
    std::vector<Creator> creators;
};
static World world;
static CInstance g_GlobalInstance;

static Creator* findCreator(int id) {
    for (auto& c : world.creators) if (c.id == id) return &c;
    return nullptr;
}

struct CallCounts {
    long total = 0;
    long timerReads = 0;
    long playerResolves = 0;     // HhResolveLocalPlayer calls
    long playerPositionReads = 0;
    long creatorPositionReads = 0;
};
static CallCounts counts;
static void resetCounts() { counts = CallCounts{}; }

static bool isPlayer(const RValue& v) { return v.m_Kind == VALUE_REF && v.number == kPlayerHandle; }
static int creatorIdOf(const RValue& v) {
    if (v.m_Kind == VALUE_OBJECT && v.instance) return v.instance->id;
    return (int)v.ToDouble();
}

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        ++counts.total;
        const std::string key(name);
        if (key == "asset_get_index") {
            const std::string a = args[0].ToString();
            if (a == "objMinimap") return RValue(9000.0);
            if (a == "Player_obj") return RValue(9100.0);
            if (a == "Enemy_Creator_obj") return RValue((double)kCreatorObject);
            return RValue(-1.0);
        }
        if (key == "instance_find") {
            const double which = args[0].ToDouble();
            if (which == 9000.0) return RValue(world.minimapInstance);
            if (which == kCreatorObject) {
                const int i = static_cast<int>(args[1].ToDouble());
                return i < 0 || i >= static_cast<int>(world.creators.size()) ? RValue(-4.0) : RValue((double)world.creators[i].id);
            }
            return RValue(-4.0);
        }
        if (key == "instance_number") {
            const double which = args[0].ToDouble();
            if (which == 9100.0) return RValue(world.playerObjectPresent ? 1.0 : 0.0);
            if (which == kCreatorObject) return RValue((double)world.creators.size());
            return RValue(0.0);
        }
        if (key == "variable_instance_exists") return RValue(1.0);
        if (key == "variable_instance_get") {
            const std::string v = args[1].ToString();
            if (v == "minimapDiscoveredGrid") return RValue(world.grid);
            if (v == "object_index") {
                if (args[0].m_Kind == VALUE_OBJECT && args[0].instance) return RValue((double)args[0].instance->object);
                return RValue(-1.0);
            }
            if (v == "id") {
                if (args[0].m_Kind == VALUE_OBJECT && args[0].instance) return RValue((double)args[0].instance->id);
                return RValue();
            }
            if (v == "enemyCreatorTimer") {
                ++counts.timerReads;
                Creator* c = findCreator(creatorIdOf(args[0]));
                if (!c || !c->timerDefined) return RValue();     // VALUE_UNDEFINED
                return RValue(116.0);
            }
            if (v == "x" || v == "y") {
                if (isPlayer(args[0])) {
                    ++counts.playerPositionReads;
                    return RValue(v == "x" ? world.playerX : world.playerY);
                }
                ++counts.creatorPositionReads;
                Creator* c = findCreator(creatorIdOf(args[0]));
                if (!c) return RValue();
                return RValue(v == "x" ? c->x : c->y);
            }
            return RValue();
        }
        if (key == "ds_exists") return RValue(1.0);
        if (key == "ds_grid_clear") return RValue();
        return RValue();
    }
    AurieStatus GetGlobalInstance(CInstance** out) { *out = &g_GlobalInstance; return 0; }
    AurieStatus GetBuiltin(const char* name, CInstance*, int, RValue& out) {
        if (std::string(name) != "room") return 1;
        out = RValue();
        out.m_Kind = VALUE_REF; out.text = "ref room Act_" + std::to_string(world.room);
        return 0;
    }
};
static FakeRunner runnerStorage;
static FakeRunner* g_Yytk = &runnerStorage;

static std::vector<std::string> outLog;
static void Out(const std::string& s) { outLog.push_back(s); }
static size_t DeferredDensityPending() { return 0; }
static bool reserveAvailable = true;
static bool PreparePopulationCapacity() { return true; }
static bool PopulationCapacityAvailable() { return reserveAvailable; }

// ---- what the injected production code leans on ---------------------------
static bool g_BeSpawnNear = false;
static double g_BeWakeRadius = 0.0;
static bool BeaconActive() { return false; }
// ModuleMain's real signature carries a defaulted `how` out-parameter.
static bool HhResolveLocalPlayer(RValue& out, std::string* how = nullptr) {
    (void)how;
    ++counts.playerResolves;
    if (!world.localPlayerResolves) return false;
    out = RValue(kPlayerHandle);
    out.m_Kind = VALUE_REF;
    return true;
}
static bool IsCreatorObject(int objIdx) { return objIdx == kCreatorObject; }
static long g_RevealSpawnLies = 0;
static long g_BeSpawnLies = 0;

// distance_to_object's native answer: the real distance to the player.
static void (*g_OrigDistanceToObject)(RValue&, CInstance*, CInstance*, int, RValue*) = nullptr;
static double nativeDistance(int creatorId) {
    const Creator* c = findCreator(creatorId);
    if (!c) return -1.0;
    return std::hypot(c->x - world.playerX, c->y - world.playerY);
}
static void OrigDistance(RValue& Result, CInstance* S, CInstance*, int, RValue*) {
    Result = RValue(S ? nativeDistance(S->id) : -1.0);
}

// PRODUCTION_ROLLINGFILL

// PRODUCTION_MAPREVEAL

// PRODUCTION_FUNCTIONS

// ---- scenarios ------------------------------------------------------------
static int failures = 0;
static void check(const std::string& label, double got, double want) {
    const bool ok = std::fabs(got - want) < 1e-9;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}
static void checkInt(const std::string& label, long long got, long long want) {
    const bool ok = (got == want);
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}

// Call the real hook for one caller, exactly as the builtin detour would.
static double distanceFor(int id, int object = kCreatorObject) {
    CInstance inst; inst.id = id; inst.object = object;
    RValue result;
    Hook_distance_to_object(result, &inst, nullptr, 0, nullptr);
    return result.ToDouble();
}

static ForgePact::MapRevealManager& mgr() { return ForgePact::MapRevealManager::Instance(); }

// A fresh zone with the fill on (`reveal 1` + `reveal spawn 1`), its pass
// opened by the tick at `frame` (a multiple of 20). Returns the next frame.
static uint64_t openFilledZone(uint64_t frame, std::vector<Creator> creators) {
    world = World{};
    world.creators = std::move(creators);
    mgr().SetEnabled(false);
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    mgr().OnFrame(frame);
    return frame + 1;
}
// Frames with no creator asking: nothing deferred, so today's pass ends.
static uint64_t idleFrames(uint64_t frame, int n) {
    for (int i = 0; i < n; ++i) mgr().OnFrame(frame++);
    return frame;
}

int main() {
    g_OrigDistanceToObject = &OrigDistance;

    // ===== Baseline: today's fill, with `fillroll` off ======================
    // --- the entry pass answers 0 to a ready creator anywhere in the zone ---
    uint64_t f = openFilledZone(20, { { 1, true, 9000.0, 0.0 } });
    checkInt("baseline/entry_pass_open", mgr().SpawnWindowLeft(), 900);
    check("baseline/far_native_is_real", nativeDistance(1), 9000.0);
    check("baseline/entry_pass_far_answered", distanceFor(1), 0.0);

    // --- after the pass closes, a creator asking gets the native answer -----
    f = openFilledZone(1000, { { 2, true, 9000.0, 0.0 } });
    f = idleFrames(f, 920);
    checkInt("baseline/pass_closed", mgr().SpawnWindowLeft(), 0);
    check("baseline/after_pass_native", distanceFor(2), 9000.0);

    // --- with the fill off, every creator gets the native answer ------------
    world = World{};
    world.creators = { { 3, true, 2000.0, 0.0 }, { 4, true, 9000.0, 0.0 } };
    mgr().SetEnabled(false);
    mgr().SetPacks(false);
    mgr().SetEnabled(true);          // reveal on, `reveal spawn` off
    mgr().OnFrame(3000);
    check("baseline/fill_off_near_native", distanceFor(3), 2000.0);
    check("baseline/fill_off_far_native", distanceFor(4), 9000.0);
    mgr().SetEnabled(false);         // reveal off altogether
    check("baseline/reveal_off_native", distanceFor(3), 2000.0);

    // ===== Target: `fillroll` ===============================================
    auto& roll = ForgePact::RollingFill::Instance();
    checkInt("target/starts_off", roll.IsOn(), 0);
    check("target/default_reach", roll.Reach(), 3000.0);

    // --- (baseline) with the fill off, `fillroll` on changes nothing --------
    mgr().SetFillRolling(true);
    world = World{};
    world.creators = { { 5, true, 2000.0, 0.0 }, { 6, true, 9000.0, 0.0 } };
    mgr().SetEnabled(false);
    mgr().SetPacks(false);
    mgr().SetEnabled(true);
    mgr().OnFrame(4000);
    resetCounts();
    check("baseline/fill_off_rolling_on_near_native", distanceFor(5), 2000.0);
    check("baseline/fill_off_rolling_on_far_native", distanceFor(6), 9000.0);
    checkInt("baseline/fill_off_rolling_on_no_player_read", counts.playerResolves, 0);
    mgr().SetEnabled(false);
    check("baseline/reveal_off_rolling_on_native", distanceFor(5), 2000.0);

    // --- far creators are held back, near ones answered ----------------------
    f = openFilledZone(5000, { { 10, true, 9000.0, 0.0 }, { 11, true, 2000.0, 0.0 } });
    resetCounts();
    check("target/far_native", distanceFor(10), 9000.0);
    checkInt("target/far_held_back", roll.HeldBack(), 1);
    // The reach is decided before the admission request, so a creator held
    // back for distance never takes (or queues for) an admission slot.
    checkInt("target/far_takes_no_slot",
             (long long)(mgr().AdmittedPacks() + mgr().QueuedPacks() + mgr().UnconfirmedPacks()), 0);
    check("target/near_answered", distanceFor(11), 0.0);
    checkInt("target/near_counted", roll.Answered(), 1);
    checkInt("target/near_admitted", (long long)mgr().AdmittedPacks(), 1);
    // Cost: two creators, one frame - one resolve, one x and one y.
    checkInt("target/player_resolved_once_a_frame", counts.playerResolves, 1);
    checkInt("target/player_position_read_once_a_frame", counts.playerPositionReads, 2);
    {
        double px = -1, py = -1;
        checkInt("target/stat_player_known", roll.LastPlayer(px, py), 1);
        check("target/stat_player_x", px, 0.0);
    }
    // The counter's positive control: the next frame reads the player again.
    mgr().OnFrame(f++);
    distanceFor(10);
    checkInt("target/player_read_again_next_frame", counts.playerResolves, 2);

    // --- many creators in one frame still read the player once ---------------
    {
        std::vector<Creator> many;
        // Ten near, ten far: few enough near that one frame's admission
        // budget answers them all.
        for (int id = 100; id < 120; ++id) many.push_back({ id, true, (id % 2) ? 1000.0 : 12000.0, 0.0 });
        f = openFilledZone(6000, many);
        resetCounts();
        int answered = 0;
        for (int id = 100; id < 120; ++id) if (distanceFor(id) == 0.0) ++answered;
        checkInt("target/many_one_resolve", counts.playerResolves, 1);
        checkInt("target/many_player_reads", counts.playerPositionReads, 2);
        checkInt("target/many_held_back", roll.HeldBack(), 10);
        checkInt("target/many_only_near_answered", answered, 10);
    }

    // --- the admission budget still applies within the reach -----------------
    {
        std::vector<Creator> crowd;
        for (int id = 200; id < 260; ++id) crowd.push_back({ id, true, 500.0, 0.0 });
        f = openFilledZone(7000, crowd);
        int answered = 0;
        for (int id = 200; id < 260; ++id) if (distanceFor(id) == 0.0) ++answered;
        checkInt("target/admission_still_limits", answered > 0 && answered <= 32, 1);
        checkInt("target/admission_defers_the_rest", mgr().QueuedPacks() > 0, 1);
    }

    // --- capacity still gates within the reach --------------------------------
    f = openFilledZone(8000, { { 70, true, 1000.0, 0.0 } });
    reserveAvailable = false;
    check("target/capacity_still_gates", distanceFor(70), 1000.0);
    reserveAvailable = true;
    check("target/capacity_recovered", distanceFor(70), 0.0);

    // --- the pass stays armed for the whole zone visit ------------------------
    f = openFilledZone(9000, { { 20, true, 9000.0, 0.0 } });
    check("target/armed_far_first_native", distanceFor(20), 9000.0);
    f = idleFrames(f, 1000);         // nothing deferred: today's pass would end
    checkInt("target/armed_after_900_frames", mgr().WantsPackSpawn(), 1);
    checkInt("target/armed_window_steady", mgr().SpawnWindowLeft(), 1);
    world.playerX = 8000.0;          // the player came within 1,000 px of it
    mgr().OnFrame(f++);
    check("target/approached_answered", distanceFor(20), 0.0);

    // --- an unready creator within reach never gets 0 -------------------------
    {
        f = openFilledZone(11000, { { 30, true, 9000.0, 0.0 }, { 31, false, 500.0, 0.0 } });
        int lies = 0;
        for (int i = 0; i < 1000; ++i) { if (distanceFor(31) == 0.0) ++lies; mgr().OnFrame(f++); }
        checkInt("target/unready_near_never_answered", lies, 0);
        world.creators[1].timerDefined = true;   // positive control: once ready, it is
        check("target/unready_near_once_ready", distanceFor(31), 0.0);
    }

    // --- no local player: the native answer for all --------------------------
    f = openFilledZone(13000, { { 40, true, 500.0, 0.0 }, { 41, true, 9000.0, 0.0 } });
    world.localPlayerResolves = false;
    check("target/no_player_near_native", distanceFor(40), 500.0);
    check("target/no_player_far_native", distanceFor(41), 9000.0);
    checkInt("target/no_player_counts_nothing", (long long)(roll.Answered() + roll.HeldBack()), 0);
    checkInt("target/no_player_takes_no_slot", (long long)mgr().AdmittedPacks(), 0);
    world.localPlayerResolves = true;
    mgr().OnFrame(f++);
    check("target/player_back_near_answered", distanceFor(40), 0.0);

    // --- a non-creator caller pays nothing new ----------------------------------
    resetCounts();
    check("target/non_creator_native", distanceFor(999, kOtherObject), -1.0);
    checkInt("target/non_creator_pays_nothing", counts.total, 1);   // its object_index, as today

    // --- turning it off re-arms the full pass for the current zone -----------
    f = openFilledZone(15000, { { 50, true, 9000.0, 0.0 } });
    f = idleFrames(f, 1000);
    check("target/rearm_far_held", distanceFor(50), 9000.0);
    mgr().SetFillRolling(false);
    checkInt("target/off_rearms_pending", mgr().PacksPending(), 1);
    while (f % 20 != 0) mgr().OnFrame(f++);
    mgr().OnFrame(f++);
    checkInt("target/off_rearms_window", mgr().SpawnWindowLeft(), 900);
    check("target/off_rest_of_zone_fills", distanceFor(50), 0.0);
    // ...but only with the fill on: off with `reveal spawn` off arms nothing.
    mgr().SetPacks(false);
    mgr().SetFillRolling(true);
    mgr().SetFillRolling(false);
    checkInt("target/off_with_fill_off_arms_nothing", mgr().PacksPending(), 0);
    mgr().SetFillRolling(true);

    // --- a zone change resets the counters ------------------------------------
    f = openFilledZone(18000, { { 60, true, 9000.0, 0.0 }, { 61, true, 2000.0, 0.0 } });
    distanceFor(60);
    distanceFor(61);
    checkInt("target/counters_before_zone_change", roll.Answered() == 1 && roll.HeldBack() == 1, 1);
    world.room = 300; world.minimapInstance = 7100; world.grid = 600;
    while (f % 20 != 0) mgr().OnFrame(f++);
    mgr().OnFrame(f++);
    checkInt("target/zone_change_resets_answered", (long long)roll.Answered(), 0);
    checkInt("target/zone_change_resets_held_back", (long long)roll.HeldBack(), 0);

    // --- the reach: 1 gives 3,000, a number 1,500-20,000 sets it --------------
    {
        using C = ForgePact::RollingFill::Command;
        auto parse = [](const char* arg, double& reach) { return ForgePact::RollingFill::Parse(arg, reach); };
        double r = -1;
        checkInt("target/parse_1_on", parse("1", r) == C::On, 1);
        check("target/parse_1_gives_3000", r, 3000.0);
        checkInt("target/parse_0_off", parse("0", r) == C::Off, 1);
        checkInt("target/parse_stat", parse("stat", r) == C::Stat, 1);
        r = -1;
        checkInt("target/parse_1500_on", parse("1500", r) == C::On, 1);
        check("target/parse_1500_reach", r, 1500.0);
        r = -1;
        checkInt("target/parse_20000_on", parse("20000", r) == C::On, 1);
        check("target/parse_20000_reach", r, 20000.0);
        int usage = 0;
        for (const char* bad : { "1499", "20001", "2", "-3000", "abc", "", "3000px", "nan", "inf" })
            if (parse(bad, r) == C::Usage) ++usage;
        checkInt("target/parse_out_of_bounds_is_usage", usage, 9);
        checkInt("target/usage_text",
                 std::string(ForgePact::RollingFill::kUsage) == "fillroll: usage fillroll 1 | 0 | <reach px, 1500-20000> | stat", 1);
        checkInt("target/within_reach_edge", ForgePact::RollingFill::WithinReach(3000.0, 0.0, 0.0, 0.0, 3000.0), 1);
        checkInt("target/within_reach_nan_is_out", ForgePact::RollingFill::WithinReach(std::nan(""), 0.0, 0.0, 0.0, 3000.0), 0);
    }
    // ...and a set reach is the one the fill uses.
    mgr().SetFillRolling(true, 5000.0);
    f = openFilledZone(20000, { { 90, true, 4000.0, 0.0 }, { 91, true, 6000.0, 0.0 } });
    check("target/custom_reach_inside", distanceFor(90), 0.0);
    check("target/custom_reach_outside", distanceFor(91), 6000.0);
    mgr().SetFillRolling(true, 50.0);  // never reachable through the verb
    check("target/out_of_range_reach_refused", roll.Reach(), 3000.0);
    mgr().SetFillRolling(false);
    mgr().SetEnabled(false);

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
