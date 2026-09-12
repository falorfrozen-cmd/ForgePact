// Behavioral regression harness for the map-reveal pack pass.
//
// The Python runner injects the REAL ForgePact::MapRevealManager class and the
// REAL Hook_distance_to_object body below. Only the game API is replaced; no
// game process, character, installed DLL or release asset is touched.
//
// Why this exists rather than more source-string assertions: origin's review
// of PR #2 pointed out that the window's authorization is consumed by the
// distance hook during creator *step* events, while EVENT_FRAME (OnFrame) is
// dispatched from HkPresent at the END of the frame. Only a test that calls
// the hook at the right point in that order can tell the difference, and the
// string assertions passed the whole time the ordering was wrong.
#include <atomic>
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
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
    CInstance* ToInstance() const { return instance; }
};
struct CInstance {
    int id = 0;
    int object = 0;
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};
using AurieStatus = int;
static bool AurieSuccess(int s) { return s == 0; }

// ---- the controlled world -------------------------------------------------
struct Creator { int id; bool timerDefined; };
struct World {
    int64_t room = 100;
    bool roomReadable = true;
    double minimapInstance = 7000;   // objMinimap instance id; <0 = absent
    double grid = 500;               // minimapDiscoveredGrid; <0 = absent
    bool gridVarExists = true;
    bool dsExists = true;
    bool playerPresent = true;
    std::vector<Creator> creators;
    long gridClears = 0;
};
static World world;
static CInstance g_GlobalInstance;
static RValue g_RoomMember;

static Creator* findCreator(int id) {
    for (auto& c : world.creators) if (c.id == id) return &c;
    return nullptr;
}

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        const std::string key(name);
        if (key == "asset_get_index") {
            const std::string a = args[0].ToString();
            if (a == "objMinimap") return RValue(9000.0);
            if (a == "Player_obj") return RValue(9100.0);
            if (a == "Enemy_Creator_obj") return RValue(9200.0);
            return RValue(-1.0);
        }
        if (key == "instance_find") {
            const double which = args[0].ToDouble();
            if (which == 9000.0) return RValue(world.minimapInstance);
            if (which == 9200.0) return world.creators.empty() ? RValue(-4.0) : RValue((double)world.creators[0].id);
            return RValue(-4.0);
        }
        if (key == "instance_number") {
            const double which = args[0].ToDouble();
            if (which == 9100.0) return RValue(world.playerPresent ? 1.0 : 0.0);
            if (which == 9200.0) return RValue((double)world.creators.size());
            return RValue(0.0);
        }
        if (key == "variable_instance_exists") {
            const std::string v = args[1].ToString();
            if (v == "minimapDiscoveredGrid") return RValue(world.gridVarExists ? 1.0 : 0.0);
            return RValue(1.0);
        }
        if (key == "variable_instance_get") {
            const std::string v = args[1].ToString();
            if (v == "minimapDiscoveredGrid") return RValue(world.grid);
            if (v == "object_index") {
                if (args[0].m_Kind == VALUE_OBJECT && args[0].instance) return RValue((double)args[0].instance->object);
                return RValue(-1.0);
            }
            if (v == "enemyCreatorTimer") {
                int id = -1;
                if (args[0].m_Kind == VALUE_OBJECT && args[0].instance) id = args[0].instance->id;
                else id = (int)args[0].ToDouble();
                Creator* c = findCreator(id);
                if (!c || !c->timerDefined) return RValue();     // VALUE_UNDEFINED
                return RValue(116.0);
            }
            if (v == "x" || v == "y") return RValue(0.0);
            return RValue();
        }
        if (key == "ds_exists") return RValue(world.dsExists ? 1.0 : 0.0);
        if (key == "ds_grid_clear") { ++world.gridClears; return RValue(); }
        if (key == "object_get_name") {
            return RValue(args[0].ToDouble() == 9200.0 ? "Enemy_Creator_obj" : "Other_obj");
        }
        return RValue();
    }
    AurieStatus GetGlobalInstance(CInstance** out) { *out = &g_GlobalInstance; return 0; }
    AurieStatus GetInstanceMember(RValue, const char* name, RValue*& out) {
        if (std::string(name) == "room" && world.roomReadable) {
            g_RoomMember = RValue((double)world.room);
            out = &g_RoomMember;
            return 0;
        }
        out = nullptr;
        return 1;
    }
};
static FakeRunner runnerStorage;
static FakeRunner* g_Yytk = &runnerStorage;

static std::vector<std::string> outLog;
static void Out(const std::string& s) { outLog.push_back(s); }

// ---- what the injected production code leans on ---------------------------
static bool g_BeSpawnNear = false;
static double g_BeWakeRadius = 0.0;
static bool g_BeaconActive = false;
static bool BeaconActive() { return g_BeaconActive; }
static bool HhResolveLocalPlayer(RValue& out) { out = RValue(1.0); return world.playerPresent; }
static bool IsCreatorObject(int objIdx) { return objIdx == 9200; }
static long g_RevealSpawnLies = 0;
static long g_BeSpawnLies = 0;

// distance_to_object's real answer for a far-away creator.
static constexpr double kRealDistance = 2500.0;
static void (*g_OrigDistanceToObject)(RValue&, CInstance*, CInstance*, int, RValue*) = nullptr;
static void OrigDistance(RValue& Result, CInstance*, CInstance*, int, RValue*) { Result = RValue(kRealDistance); }

// PRODUCTION_MAPREVEAL

// PRODUCTION_FUNCTIONS

// ---- scenarios ------------------------------------------------------------
static int failures = 0;
static void check(const std::string& label, double got, double want) {
    const bool ok = std::fabs(got - want) < 1e-9;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label
              << " got=" << got << " want=" << want << "\n";
}
static void checkInt(const std::string& label, long long got, long long want) {
    const bool ok = (got == want);
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label
              << " got=" << got << " want=" << want << "\n";
}

// Call the real hook for one creator, exactly as the builtin detour would.
static double distanceFor(int creatorId) {
    CInstance inst; inst.id = creatorId; inst.object = 9200;
    RValue result;
    Hook_distance_to_object(result, &inst, nullptr, 0, nullptr);
    return result.ToDouble();
}

static ForgePact::MapRevealManager& mgr() { return ForgePact::MapRevealManager::Instance(); }

int main() {
    g_OrigDistanceToObject = &OrigDistance;

    // --- 1. a ready zone populates -----------------------------------------
    world = World{};
    world.creators = { { 1, true }, { 2, true } };
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    mgr().OnFrame(20);
    checkInt("ready_zone/window", mgr().SpawnWindowLeft(), 900);
    check("ready_zone/distance", distanceFor(1), 0.0);

    // --- 2. THE REGRESSION: a new zone, consumed before the next Present ----
    // The room, minimap instance and grid all change and the new zone's
    // creators are unready - the state during a room load. The distance hook
    // runs in the creator's step event, i.e. BEFORE OnFrame(21).
    world.room = 200;
    world.minimapInstance = 7001;
    world.grid = 501;
    world.creators = { { 10, false } };
    check("new_room_before_present/unready_creator", distanceFor(10), kRealDistance);

    // A creator that IS initialised is still safe to answer - the damage is
    // specific to uninitialised ones, and this is what keeps the pass working
    // across the transition rather than silently doing nothing.
    world.creators.push_back({ 11, true });
    check("new_room_before_present/ready_creator", distanceFor(11), 0.0);

    // --- 3. ...and the frame boundary still closes the stale window ---------
    mgr().OnFrame(21);
    checkInt("new_room_after_present/window", mgr().SpawnWindowLeft(), 0);
    check("new_room_after_present/distance", distanceFor(11), kRealDistance);

    // --- 4. same room key, replaced map identity ---------------------------
    world = World{};
    world.creators = { { 1, true } };
    mgr().SetEnabled(false);
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    mgr().OnFrame(40);
    checkInt("same_room_new_map/opened", mgr().SpawnWindowLeft(), 900);
    world.minimapInstance = 7002;          // map replaced, room key unchanged
    world.grid = 502;
    world.creators = { { 20, false } };
    check("same_room_new_map/unready_before_present", distanceFor(20), kRealDistance);
    mgr().OnFrame(41);
    checkInt("same_room_new_map/window_closed", mgr().SpawnWindowLeft(), 0);

    // --- 5. minimap gone entirely, room key unchanged ----------------------
    world = World{};
    world.creators = { { 1, true } };
    mgr().SetEnabled(false);
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    mgr().OnFrame(60);
    checkInt("map_lost/opened", mgr().SpawnWindowLeft(), 900);
    world.minimapInstance = -4;
    world.creators = { { 30, false } };
    check("map_lost/unready_before_present", distanceFor(30), kRealDistance);
    mgr().OnFrame(61);
    checkInt("map_lost/window_closed", mgr().SpawnWindowLeft(), 0);

    // --- 6. unreadable room must not open a window -------------------------
    world = World{};
    world.creators = { { 1, true } };
    world.roomReadable = false;
    mgr().SetEnabled(false);
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    mgr().OnFrame(80);
    checkInt("unreadable_room/never_opens", mgr().SpawnWindowLeft(), 0);
    check("unreadable_room/distance", distanceFor(1), kRealDistance);
    // ...and it opens on a later tick once the room can be read again.
    world.roomReadable = true;
    mgr().OnFrame(100);
    checkInt("unreadable_room/opens_when_readable", mgr().SpawnWindowLeft(), 900);

    // --- 7. enabling packs applies to the zone already being stood in ------
    world = World{};
    world.creators = { { 1, true } };
    mgr().SetEnabled(false);
    mgr().SetPacks(false);
    mgr().SetEnabled(true);
    mgr().OnFrame(120);                    // reveal on, packs off: no window
    checkInt("packs_off/no_window", mgr().SpawnWindowLeft(), 0);
    mgr().SetPacks(true);                  // ...turned on, same zone
    mgr().OnFrame(140);
    checkInt("packs_on_current_zone/window", mgr().SpawnWindowLeft(), 900);

    // --- 8. an unready zone is never lied to, however long it waits --------
    world = World{};
    world.creators = { { 1, false } };
    mgr().SetEnabled(false);
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    for (int f = 160; f <= 260; f += 20) mgr().OnFrame(f);
    checkInt("never_ready/no_window", mgr().SpawnWindowLeft(), 0);
    check("never_ready/distance", distanceFor(1), kRealDistance);

    // --- 9. the Beacon's lie is gated by the same invariant ----------------
    // The damage mechanism does not care which feature lied, so the Beacon
    // gets the readiness guard too. Its own wake radius and continuous
    // behaviour are otherwise unchanged - reveal is off throughout here.
    world = World{};
    world.creators = { { 40, false }, { 41, true } };
    mgr().SetEnabled(false);               // reveal off: beacon path only
    g_BeSpawnNear = true;
    g_BeaconActive = true;
    g_BeWakeRadius = 0.0;                  // no radius limit, so only readiness gates
    checkInt("beacon/reveal_is_off", mgr().SpawnWindowLeft(), 0);
    check("beacon/unready_creator", distanceFor(40), kRealDistance);
    check("beacon/ready_creator", distanceFor(41), 0.0);
    g_BeSpawnNear = false;
    g_BeaconActive = false;

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
