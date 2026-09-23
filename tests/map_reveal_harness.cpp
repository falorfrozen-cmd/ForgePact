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
//
// The hook is compiled the way the PLAYER build sees it. Since 1.3.20 its
// body also carries a research-only object_index probe (finding 8), which is
// stripped by /DFORGEPACT_RELEASE and is not what these scenarios are about -
// tests/test_release_hook_contract.py is what pins it out of the player build.
#define FORGEPACT_RELEASE 1
#include <atomic>
#include <algorithm>
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
struct Creator { int id; bool timerDefined; };
struct World {
    int64_t room = 100;
    bool roomReadable = true;
    // The live runner answers `room` as a REF, not a real; true switches
    // the fake to a number so both kinds meet the same key derivation.
    bool roomIsReal = false;
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

// What one call through the hook costs the runner. The readiness check is one
// variable_instance_get("enemyCreatorTimer") on the builtin every spawner
// polls, so counting those is how "the check runs twice" is visible at all -
// every decision the hook makes is identical either way, which is precisely
// why a behavioural assertion cannot see the difference.
struct CallCounts {
    long total = 0;          // every CallBuiltin, whatever its name
    long timerReads = 0;     // variable_instance_get(..., "enemyCreatorTimer")
    long objectIndexReads = 0;
};
static CallCounts counts;
static void resetCounts() { counts = CallCounts{}; }

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        ++counts.total;
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
            if (which == 9200.0) {
                const int i=static_cast<int>(args[1].ToDouble());
                return i<0 || i>=static_cast<int>(world.creators.size()) ? RValue(-4.0) : RValue((double)world.creators[i].id);
            }
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
                ++counts.objectIndexReads;
                if (args[0].m_Kind == VALUE_OBJECT && args[0].instance) return RValue((double)args[0].instance->object);
                return RValue(-1.0);
            }
            if (v == "id") {
                if (args[0].m_Kind == VALUE_OBJECT && args[0].instance) return RValue((double)args[0].instance->id);
                return RValue();
            }
            if (v == "enemyCreatorTimer") {
                ++counts.timerReads;
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
    // `room` is a BUILT-IN: the instance-member read never answers for it on
    // the real runner, which is why RoomKey() returned INT64_MIN forever and
    // the pack pass never opened a window. Kept, always failing, as the
    // negative control that pins why this route was abandoned.
    AurieStatus GetInstanceMember(RValue, const char*, RValue*& out) {
        out = nullptr;
        return 1;
    }
    // Measured live 2026-09-15 (`roomprobe`): GetBuiltin answers, as a
    // VALUE_REF stringifying to "ref room Act_06_01". The fake returns a ref by
    // default so the scenarios run against the shape the runner really
    // produces, not a convenient number.
    AurieStatus GetBuiltin(const char* name, CInstance*, int, RValue& out) {
        if (std::string(name) != "room" || !world.roomReadable) return 1;
        out = RValue();
        if (world.roomIsReal) { out.m_Kind = VALUE_REAL; out.number = (double)world.room; }
        else { out.m_Kind = VALUE_REF; out.text = "ref room Act_" + std::to_string(world.room); }
        return 0;
    }
};
static FakeRunner runnerStorage;
static FakeRunner* g_Yytk = &runnerStorage;

static std::vector<std::string> outLog;
static void Out(const std::string& s) { outLog.push_back(s); }
static bool capacityReady = true;
static size_t DeferredDensityPending(){return 0;}
static bool reserveAvailable = true;
static bool PreparePopulationCapacity() { return capacityReady; }
static bool PopulationCapacityAvailable() { return capacityReady && reserveAvailable; }

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
static uint64_t populationNow=0;
static uint64_t populationClock(){return populationNow;}

int main() {
    g_OrigDistanceToObject = &OrigDistance;

    // --- 1. a ready zone populates -----------------------------------------
    world = World{};
    world.creators = { { 1, true }, { 2, true } };
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    mgr().OnFrame(20);
    checkInt("ready_zone/window", mgr().SpawnWindowLeft(), 900);
    resetCounts();
    check("ready_zone/distance", distanceFor(1), 0.0);
    // The readiness check used to run twice for the same creator on this
    // path: once standalone, then again inside MayPopulate. Same decision,
    // twice the runtime calls, on the builtin every spawner polls.
    checkInt("ready_zone/timer_reads", counts.timerReads, 1);
    // Finding 8's first threshold condition, printed as a number rather than
    // argued in a comment: what one lied-to creator costs the runner, and how
    // much of that is the object_index read a struct read would remove.
    checkInt("liedto/callbuiltins", counts.total, 3);
    checkInt("liedto/object_index_share_pct",
             counts.total ? (100 * counts.objectIndexReads) / counts.total : 0, 33);

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
    resetCounts();
    check("beacon/ready_creator", distanceFor(41), 0.0);
    // Reveal off means MayPopulate short-circuits on WantsPackSpawn(), so the
    // Beacon path already paid exactly one readiness read and still does.
    checkInt("beacon/ready_creator_timer_reads", counts.timerReads, 1);

    // --- 10. the counter's positive control --------------------------------
    // A scenario whose expected count is something other than 1, proving the
    // counter can report a difference at all. Window open, Beacon on, creator
    // unready: reveal asks (and declines), then the Beacon asks for itself.
    // This is the one accepted regression of the dedup - a transient
    // zone-load case whose decision is identical either way.
    world = World{};
    world.creators = { { 50, true } };
    mgr().SetEnabled(false);
    mgr().SetEnabled(true);
    mgr().SetPacks(true);
    mgr().OnFrame(300);
    checkInt("beacon_and_window/window", mgr().SpawnWindowLeft(), 900);
    world.creators.push_back({ 51, false });
    resetCounts();
    check("beacon_and_window/unready_creator", distanceFor(51), kRealDistance);
    checkInt("beacon_and_window/timer_reads", counts.timerReads, 2);
    g_BeSpawnNear = false;
    g_BeaconActive = false;

    // Capacity failures never force early creation; fog and natural distance stay usable.
    world = World{}; world.creators = {{60,true}};
    mgr().SetEnabled(false); capacityReady = false; mgr().SetEnabled(true); mgr().OnFrame(320);
    checkInt("capacity/unavailable_window",mgr().SpawnWindowLeft(),0);
    check("capacity/unavailable_distance",distanceFor(60),kRealDistance);
    checkInt("capacity/fog_still_revealed",world.gridClears,1);
    capacityReady = true; mgr().OnFrame(340);
    check("capacity/recovered",distanceFor(60),0);
    reserveAvailable = false;
    check("capacity/exhausted_reserve",distanceFor(60),kRealDistance);
    reserveAvailable = true;

    // Full production manager + real hook: a capacity pause exceeds the old
    // 900-frame window. After recovery the tail must still run exactly once.
    world = World{}; for(int id=1;id<=2100;++id)world.creators.push_back({id,true});
    mgr().SetEnabled(false); mgr().SetEnabled(true);mgr().OnFrame(360);
    reserveAvailable=false;
    std::vector<bool> completed(2101,false);int served=0,maxPerFrame=0;
    for(int frame=361;frame<3000 && served<2100;++frame) {
        if(frame==1262)reserveAvailable=true;
        int thisFrame=0;
        for(int id=1;id<=2100;++id)if(!completed[id] && distanceFor(id)==0) {
            completed[id]=true;++served;++thisFrame;
        }
        maxPerFrame=(std::max)(maxPerFrame,thisFrame);
        mgr().OnFrame(frame);
        if(frame==1261)checkInt("queue/tail_survives_timeout",mgr().WantsPackSpawn(),1);
    }
    checkInt("queue/all_groups",served,2100);
    checkInt("queue/per_frame_limit",maxPerFrame>0 && maxPerFrame<=32,1);
    checkInt("queue/no_tail",mgr().QueuedPacks(),0);
    for(int f=3000;f<=3900;++f)mgr().OnFrame(f);
    checkInt("queue/closes_when_done",mgr().SpawnWindowLeft(),0);
    mgr().SetEnabled(false);checkInt("queue/disabled_clears",mgr().QueuedPacks(),0);

    // The live report had 448 admitted groups. Exercise the default production
    // adaptive policy through the real hook, with staggered native polls.
    world=World{};for(int id=1;id<=448;++id)world.creators.push_back({id,true});
    mgr().SetEnabled(true);mgr().OnFrame(3920);
    std::vector<bool> fastDone(449,false);int fastServed=0;
    for(int frame=3921;frame<=4420;++frame) {
        for(int id=1;id<=448;++id) {
            if(!fastDone[id] && (frame+id)%15==0 && distanceFor(id)==0) {
                fastDone[id]=true;++fastServed;
            }
        }
        mgr().OnFrame(frame);
    }
    checkInt("queue/staggered_448_eventually_complete",fastServed,448);
    mgr().SetEnabled(false);

    // The actual manager and distance hook, with 50fps frame cadence,
    // staggered native polls and measured construction work, must also meet
    // the deadline. No game/room-readiness or capacity gate is bypassed.
    auto& budget=ForgePact::AdaptivePopulationBudget::Instance();
    budget=ForgePact::AdaptivePopulationBudget(&populationClock);
    world=World{};for(int id=1;id<=1536;++id)world.creators.push_back({id,true});
    mgr().SetEnabled(true);mgr().OnFrame(10000);
    std::vector<bool> timedDone(1537,false);int timedServed=0;
    for(int frame=1;frame<=250 && timedServed<1536;++frame){
        populationNow=static_cast<uint64_t>(frame)*20000;
        for(int id=1;id<=1536;++id)if(!timedDone[id] && (frame+id)%80==0 && distanceFor(id)==0){
            timedDone[id]=true;++timedServed;populationNow+=150;budget.RecordNative(150);
        }
        mgr().OnFrame(10000+frame);
    }
    checkInt("deadline/production_groups",timedServed,1536);
    checkInt("deadline/production_under_five_seconds",populationNow<=5000000,1);
    checkInt("deadline/production_not_reported_late",budget.TargetExceeded(),0);
    checkInt("deadline/production_no_tail",mgr().QueuedPacks(),0);
    mgr().SetEnabled(false);

    // Real live failure shape: some ready callers stop polling after denial.
    // The grace window can end, but that must not erase their unconfirmed
    // identities or inflate the admitted count. A zone/toggle reset clears it.
    world=World{};for(int id=1;id<=64;++id)world.creators.push_back({id,true});
    mgr().SetEnabled(true);mgr().OnFrame(10600);
    for(int id=1;id<=64;++id)distanceFor(id);
    const auto admittedBeforeSilence=mgr().AdmittedPacks();
    const auto waitingBeforeSilence=mgr().QueuedPacks();
    checkInt("silent/positive_control_denied",waitingBeforeSilence>0,1);
    for(int frame=10601;frame<=11900;++frame)mgr().OnFrame(frame);
    checkInt("silent/not_reported_as_admitted",mgr().AdmittedPacks(),admittedBeforeSilence);
    checkInt("silent/unconfirmed_retained",mgr().UnconfirmedPacks(),waitingBeforeSilence);
    checkInt("silent/bounded_window",mgr().SpawnWindowLeft(),0);
    const auto closedPassElapsed=budget.PassElapsedMs();
    populationNow+=1000000;
    checkInt("silent/closed_pass_clock_stops",budget.PassElapsedMs(),closedPassElapsed);
    mgr().ObserveNativeBirth(64);
    checkInt("silent/observed_birth_resolves_one",mgr().UnconfirmedPacks(),waitingBeforeSilence-1);
    checkInt("silent/native_birth_not_admission",mgr().AdmittedPacks(),admittedBeforeSilence);
    checkInt("silent/native_birth_count",mgr().NativeBirthPacks(),1);
    mgr().ObserveNativeBirth(64);
    checkInt("silent/native_birth_deduplicated",mgr().NativeBirthPacks(),1);
    world.minimapInstance+=1;mgr().ObserveNativeBirth(63);
    checkInt("silent/new_map_cannot_resolve_old_identity",mgr().NativeBirthPacks(),1);
    mgr().SetEnabled(false);
    checkInt("silent/reset_clears_unconfirmed",mgr().UnconfirmedPacks(),0);

    // A readable minimap can precede its creators. An empty first poll is
    // not proof that this zone will remain empty.
    world=World{};
    mgr().SetEnabled(true);mgr().OnFrame(11000);
    checkInt("late_creators/still_pending",mgr().PacksPending(),1);
    checkInt("late_creators/no_early_window",mgr().SpawnWindowLeft(),0);
    world.creators={{1,false}};mgr().OnFrame(11020);
    check("late_creators/unready_untouched",distanceFor(1),kRealDistance);
    world.creators[0].timerDefined=true;mgr().OnFrame(11040);
    check("late_creators/eventually_admitted",distanceFor(1),0);
    mgr().SetEnabled(false);

    // One uninitialised creator must not block all ready siblings. Checking
    // candidates is bounded, so a room full of unready creators cannot hitch.
    world=World{};
    for(int id=1;id<=40;++id)world.creators.push_back({id,id==40});
    mgr().SetEnabled(true);resetCounts();mgr().OnFrame(11100);
    checkInt("ready_sibling/bounded_probe",counts.timerReads<=32,1);
    mgr().OnFrame(11120);
    check("ready_sibling/admitted",distanceFor(40),0);
    check("ready_sibling/unready_untouched",distanceFor(1),kRealDistance);
    mgr().SetEnabled(false);

    world=World{};mgr().SetEnabled(true);
    for(int tick=0;tick<1802;++tick)mgr().OnFrame(12000+tick*20);
    checkInt("empty_zone/eventually_stops_polling",mgr().PacksPending(),0);
    checkInt("empty_zone/no_spawn_window",mgr().SpawnWindowLeft(),0);
    mgr().SetEnabled(false);

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
