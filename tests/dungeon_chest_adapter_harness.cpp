// Behavioral harness for Dungeon chest opens early's adapter in ModuleMain.cpp
// (issue #31): the kill consumer's enemy check and the planned total's census.
//
// tests/dungeon_chest_harness.cpp runs DungeonChestMod.hpp's decisions; this
// one runs the REAL adapter functions around them, which the Python runner
// (test_dungeon_chest_adapter.py) cuts out of ModuleMain.cpp and splices in
// below, together with the real header. Only the game is replaced: a fake
// object table answers asset_get_index, object_is_ancestor,
// variable_instance_get, instance_number, instance_find and is_array.
//
// Two binaries come from this file. The kill binary (ADAPTER_KILL) runs
// DungeonChestOnKill through the real enemy check (IsEnemyObject and
// CallerIsEnemyInstance) in both start-up states the plugin can be in: the
// research build's, where InstallCreateHooks resolved Enemy_Parent_obj into
// g_EnemyParentIdx at load, and the player build's, where nothing did unless
// another mod installed the create hooks (Live 2, 2026-10-04: kills=0 on every
// read while the owner killed). It must compile from the base tag's
// ModuleMain.cpp too, so the globals those functions use are defined here
// under their own names. The census binary (ADAPTER_CENSUS) runs
// DungeonChestEstimateTotal as the header's total source.
//
// No game process is touched.
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_ARRAY };
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    CInstance* instance = nullptr;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    // The runner refuses a number from anything that is not one.
    double ToDouble() const {
        if (m_Kind != VALUE_REAL && m_Kind != VALUE_INT32 && m_Kind != VALUE_INT64 && m_Kind != VALUE_REF)
            throw std::runtime_error("not a number");
        return number;
    }
    bool ToBoolean() const { return number != 0; }
};

// ---- the fake object table ----------------------------------------------------
// Player_obj 10; Enemy_Parent_obj 500 and its child Skeleton 510; the creator
// family's first two names at 600 and 601.
constexpr int kPlayerObj = 10;
constexpr int kEnemyParentObj = 500;
constexpr int kSkeletonObj = 510;
constexpr int kCreatorObj = 600;
constexpr int kCreatorAmbushObj = 601;
static const std::map<int, int> kParentOf = { { kSkeletonObj, kEnemyParentObj } };

struct CInstance {
    int id;
    int object;
    bool fired = false;        // a creator: its enemyArray is an array once it has spawned
    bool readable = true;      // a creator: whether instance_find hands back a live instance
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};

// Whether asset_get_index can resolve Enemy_Parent_obj yet (an ask made
// before the runtime answers it is the early ask the cache must not keep).
static bool parentResolvable = true;
static int parentAsks = 0;
// Whether the creator family's names resolve; which creator object's
// instance_number throws (-1: none).
static bool creatorFamilyResolves = true;
static int countThrowsFor = -1;
static std::vector<CInstance*> instances;

static bool Descends(int object, int parent) {
    for (auto it = kParentOf.find(object); it != kParentOf.end(); it = kParentOf.find(it->second))
        if (it->second == parent) return true;
    return false;
}

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        const std::string key(name);
        if (key == "asset_get_index") {
            const std::string& asset = args.at(0).text;
            if (asset == "Enemy_Parent_obj") { ++parentAsks; return RValue(parentResolvable ? (double)kEnemyParentObj : -1.0); }
            if (asset == "Player_obj") return RValue((double)kPlayerObj);
            if (asset == "Enemy_Creator_obj") return RValue(creatorFamilyResolves ? (double)kCreatorObj : -1.0);
            if (asset == "Enemy_Creator_Ambush_obj") return RValue(creatorFamilyResolves ? (double)kCreatorAmbushObj : -1.0);
            return RValue(-1.0);
        }
        if (key == "object_is_ancestor")
            return RValue(Descends((int)args.at(0).ToDouble(), (int)args.at(1).ToDouble()) ? 1.0 : 0.0);
        if (key == "variable_instance_get") {
            CInstance* inst = args.at(0).instance;
            if (!inst) throw std::runtime_error("variable_instance_get on no instance");
            const std::string& var = args.at(1).text;
            if (var == "object_index") return RValue((double)inst->object);
            if (var == "id") return RValue((double)inst->id);
            if (var == "enemyArray") { RValue v; if (inst->fired) v.m_Kind = VALUE_ARRAY; return v; }
            return RValue();
        }
        if (key == "is_array") return RValue(args.at(0).m_Kind == VALUE_ARRAY ? 1.0 : 0.0);
        if (key == "instance_number") {
            const int obj = (int)args.at(0).ToDouble();
            if (obj == countThrowsFor) throw std::runtime_error("instance_number failed");
            double n = 0;
            for (auto* i : instances) if (i->object == obj || Descends(i->object, obj)) ++n;
            return RValue(n);
        }
        if (key == "instance_find") {
            const int obj = (int)args.at(0).ToDouble();
            int k = (int)args.at(1).ToDouble();
            for (auto* i : instances)
                if ((i->object == obj || Descends(i->object, obj)) && k-- == 0)
                    return i->readable ? i->ToRValue() : RValue(-4.0);
            return RValue(-4.0);
        }
        throw std::runtime_error("builtin not faked: " + key);
    }
};
static FakeRunner runner;
static FakeRunner* g_Yytk = &runner;

static std::vector<std::string> outLines;
static void Out(const std::string& line) { outLines.push_back(line); }

namespace HeroSiege::Objects {
inline bool IsDescendantOf(int32_t object, int32_t parent) { return Descends(object, parent); }
}

// ModuleMain's globals the extracted functions read, under their own names.
static int g_EnemyParentIdx = -1;
static std::unordered_map<int, bool> g_IsEnemyCache;
static long g_DcKillNotEnemySelf = 0;
static constexpr const char* kKnownDensityCreatorObjects[] = { "Enemy_Creator_obj", "Enemy_Creator_Ambush_obj" };
static std::vector<int> g_DcCreatorObjects;
// The census resolves a creator through the plugin's own lookup; here an
// instance is what instance_find handed back, or nothing.
static CInstance* HhResolveInstance(const RValue& value) { return value.m_Kind == VALUE_OBJECT ? value.instance : nullptr; }

// PRODUCTION_DUNGEONCHEST

// PRODUCTION_FUNCTIONS

namespace DC = ForgePact::DungeonChest;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static int chestDummy = 0;
static long FixedSource(long, DC::Census& census) { census = DC::Census{}; return 100; }
static std::string Status() { return DC::StatusLine(DC::state, "ok", "ok"); }

#ifdef ADAPTER_KILL
// ---- the kill path ------------------------------------------------------------
static CInstance player{ 1, kPlayerObj };
static CInstance skeletonA{ 1001, kSkeletonObj };
static CInstance skeletonB{ 1002, kSkeletonObj };

// The research build: InstallCreateHooks resolved the parent at load. The name
// lookup is refused, so only the preset can make a kill count.
static void ResearchSetup() { g_EnemyParentIdx = kEnemyParentObj; parentResolvable = false; }
// The player build with no create-hook feature on (Live 2): nothing wrote the
// global, and the name lookup works.
static void ReleaseSetup() { g_EnemyParentIdx = -1; parentResolvable = true; }

// The mode on at 50 % and the room's chest seen: a tally is running.
static void Track() {
    DC::state.totalSource = &FixedSource;
    DC::SetMode(DC::state, 50);
    DC::Poll(DC::state, true, 1, 1, 10, &chestDummy);
}

// Kill-hook calls refused as not an enemy `self`: on the status line once the
// adapter reports them there, else the research probe's counter (the base tag).
static long NotEnemy() {
#ifdef HAS_STATUS_NOT_ENEMY
    const std::string line = Status();
    const size_t at = line.find(" notEnemy=");
    if (at == std::string::npos) return -1;
    return std::stol(line.substr(at + 10));
#else
    return g_DcKillNotEnemySelf;
#endif
}
static std::string Detail() {
    return "kills=" + std::to_string(DC::state.tally.kills) + " notEnemy=" + std::to_string(NotEnemy())
        + " g_EnemyParentIdx=" + std::to_string(g_EnemyParentIdx) + " parentAsks=" + std::to_string(parentAsks)
        + " | " + Status();
}

static int Run(const std::string& scenario) {
    if (scenario == "kill-research-setup") {
        ResearchSetup(); Track();
        DungeonChestOnKill(&skeletonA);
        check(scenario, DC::state.tally.kills == 1 && NotEnemy() == 0, Detail());
    } else if (scenario == "kill-player-self") {
        // The player-`self` call of a kill counts nothing, also once the
        // parent resolves: the enemy check did not become "anything".
        ReleaseSetup(); Track();
        DungeonChestOnKill(&player);
        check(scenario, DC::state.tally.kills == 0 && NotEnemy() == 1, Detail());
    } else if (scenario == "kill-not-tracking") {
        // The mode on with no chest in the room, then the mode off: nothing.
        ResearchSetup();
        DC::state.totalSource = &FixedSource;
        DC::SetMode(DC::state, 50);
        DC::Poll(DC::state, true, 1, 0, 10, nullptr);
        DungeonChestOnKill(&skeletonA);
        const long noChest = DC::state.tally.kills;
        DC::SetMode(DC::state, 0);
        DungeonChestOnKill(&skeletonB);
        check(scenario, noChest == 0 && DC::state.tally.kills == 0 && NotEnemy() == 0 && DC::state.counters.kills == 0, Detail());
    } else if (scenario == "kill-same-id-twice") {
        ResearchSetup(); Track();
        DungeonChestOnKill(&skeletonA);
        DungeonChestOnKill(&skeletonA);
        check(scenario, DC::state.tally.kills == 1 && DC::state.counters.duplicates == 1, Detail());
    } else if (scenario == "kill-release-setup") {
        // Live 2's start-up state: an enemy-`self` call counts, by name, and
        // the global the create hooks own is left as it was.
        ReleaseSetup(); Track();
        DungeonChestOnKill(&skeletonA);
        check(scenario, DC::state.tally.kills == 1 && NotEnemy() == 0 && g_EnemyParentIdx == -1, Detail());
    } else if (scenario == "enemy-check-not-poisoned") {
        // An ask made while the parent cannot be resolved yet is refused, and
        // that refusal is not kept: the same object counts once it resolves.
        g_EnemyParentIdx = -1; parentResolvable = false; Track();
        DungeonChestOnKill(&skeletonA);
        const long early = DC::state.tally.kills;
        const long earlyRefused = NotEnemy();
        parentResolvable = true;
        DungeonChestOnKill(&skeletonB);
        check(scenario, early == 0 && earlyRefused == 1 && DC::state.tally.kills == 1 && NotEnemy() == 1
                && IsEnemyObject(kSkeletonObj) && !IsEnemyObject(kPlayerObj) && g_EnemyParentIdx == -1,
            Detail());
    } else {
        std::cout << "unknown scenario " << scenario << std::endl;
        return 2;
    }
    return failures ? 1 : 0;
}
#endif

#ifdef ADAPTER_CENSUS
// ---- the census ---------------------------------------------------------------
// Three creators of Enemy_Creator_obj (one has spawned) and one ambush creator
// still to spawn: 3 pending, so with 5 alive the estimate is 5 + ceil(3 x 614 /
// 117) = 21.
static CInstance c1{ 2001, kCreatorObj, false }, c2{ 2002, kCreatorObj, false }, c3{ 2003, kCreatorObj, true };
static CInstance c4{ 2004, kCreatorAmbushObj, false };
static void Room() { instances = { &c1, &c2, &c3, &c4 }; }

static void Track(int64_t room) {
    DC::state.totalSource = &DungeonChestEstimateTotal;
    if (DC::Pct(DC::state) == 0) DC::SetMode(DC::state, 50);
    DC::Poll(DC::state, true, room, 1, 5, &chestDummy);
}
static long RefusalLines() {
    long n = 0;
    for (const auto& l : outLines) if (l.rfind("dungeonchest: no planned total in this room (", 0) == 0) ++n;
    return n;
}
static std::string Detail() {
    std::string lines;
    for (const auto& l : outLines) lines += " [" + l + "]";
    return Status() + " | out:" + lines;
}
static bool Shows(const std::string& word) { return Status().find(" total=unavailable(" + word + ") ") != std::string::npos; }

static int Run(const std::string& scenario) {
    Room();
    if (scenario == "census-estimates") {
        // The control: a readable census estimates and logs nothing.
        Track(1);
        check(scenario, DC::state.tally.total == 21 && Status().find(" total=21 creators=4 pending=3 unreadable=0 ") != std::string::npos
                && RefusalLines() == 0, Detail());
    } else if (scenario == "census-count-failed") {
        // instance_number fails for the ambush creators: the census refuses
        // rather than estimate from the three it could count.
        countThrowsFor = kCreatorAmbushObj;
        Track(1);
        check(scenario, DC::state.tally.total == 0 && Shows("count-failed") && !DC::state.tally.latched, Detail());
    } else if (scenario == "census-family-unresolved") {
        creatorFamilyResolves = false;
        Track(1);
        check(scenario, DC::state.tally.total == 0 && Shows("family-unresolved"), Detail());
    } else if (scenario == "census-no-creators") {
        instances.clear();
        Track(1);
        check(scenario, DC::state.tally.total == 0 && Shows("no-creators"), Detail());
    } else if (scenario == "census-refusal-logged-once") {
        // Five polls of a refusing room print one line, naming the cause and
        // that the game's own rule stays; the next room prints its own.
        countThrowsFor = kCreatorAmbushObj;
        for (int i = 0; i < 5; ++i) Track(1);
        const long first = RefusalLines();
        const long asks = DC::state.counters.totalAsks;
        const std::string line = outLines.empty() ? std::string() : outLines.front();
        for (int i = 0; i < 3; ++i) Track(2);
        check(scenario, first == 1 && asks == 5 && RefusalLines() == 2 && line.find("count-failed") != std::string::npos
                && line.find("game's own rule") != std::string::npos, Detail());
    } else {
        std::cout << "unknown scenario " << scenario << std::endl;
        return 2;
    }
    return failures ? 1 : 0;
}
#endif

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "usage: <scenario>" << std::endl; return 2; }
    return Run(argv[1]);
}
