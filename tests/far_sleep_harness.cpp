// Behavioral regression harness for far sleep (FarSleep.hpp).
//
// The Python runner injects the REAL ForgePact::FarSleep class below. Only the
// game API is replaced; no game process is touched. The controlled world has
// an object table with the scenery families and the parents far sleep must
// never touch, and instances that are active or asleep exactly as the runner
// keeps them: a sleeping instance is invisible to instance_number,
// instance_find, instance_exists and variable_instance_get.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_BOOL };
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const { if (m_Kind == VALUE_STRING || m_Kind == VALUE_UNDEFINED) throw std::runtime_error("not a number"); return number; }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};

// ---- the controlled world -------------------------------------------------
struct Object { std::string name; int parent; };
struct Instance { int64_t id; int object; double x, y; bool active; bool exists; double grid = -1; };
struct World {
    std::map<int, Object> objects;
    std::vector<Instance> instances;
    std::set<std::string> frameOwners;          // objects that own a Step / Draw GUI event
    double viewW = 1229, viewH = 691;
    long calls = 0, deactivates = 0, activates = 0, finds = 0, propFinds = 0, reads = 0, exists = 0;
    long callsOnMissing = 0;                    // activate/deactivate on an id that is not in the room
    long readsOnMissing = 0;                    // variable_instance_get on an id that is not in the room
    bool throwOnDeactivate = false;
    std::vector<std::string> ownerQueries;
};
static World world;
static Instance* byId(int64_t id) { for (auto& i : world.instances) if (i.id == id && i.exists) return &i; return nullptr; }
static bool isA(int object, int ancestor) {
    for (int o = object, d = 0; o >= 0 && d < 64; ++d) {
        if (o == ancestor) return true;
        auto it = world.objects.find(o);
        if (it == world.objects.end()) return false;
        o = it->second.parent;
    }
    return false;
}
static int64_t idOf(const RValue& v) { return static_cast<int64_t>(v.ToDouble()); }

struct Runner {
    RValue CallBuiltin(const char* key, std::vector<RValue> args) {
        ++world.calls;
        const std::string name(key);
        if (name == "asset_get_index") {
            for (const auto& [i, o] : world.objects) if (o.name == args[0].text) return RValue((double)i);
            return RValue(-1.0);
        }
        if (name == "object_exists") return RValue(world.objects.count((int)args[0].number) ? 1.0 : 0.0);
        if (name == "object_get_parent") {
            auto it = world.objects.find((int)args[0].number);
            if (it == world.objects.end()) return RValue(-1.0);
            return RValue(it->second.parent < 0 ? -100.0 : (double)it->second.parent);
        }
        if (name == "object_get_name") {
            auto it = world.objects.find((int)args[0].number);
            RValue r(it == world.objects.end() ? "<undefined>" : it->second.name.c_str());
            return r;
        }
        if (name == "instance_number") {
            const int obj = (int)args[0].number;
            long n = 0;
            for (const auto& i : world.instances) if (i.exists && i.active && (obj == -3 || isA(i.object, obj))) ++n;
            return RValue((double)n);
        }
        if (name == "instance_find") {
            ++world.finds;
            const int obj = (int)args[0].number; int remaining = (int)args[1].number;
            if (obj != 9 && obj != 40) ++world.propFinds;   // neither Player_obj nor objMinimap
            for (const auto& i : world.instances)
                if (i.exists && i.active && isA(i.object, obj) && remaining-- == 0) { RValue v((double)i.id); v.m_Kind = VALUE_REF; return v; }
            return RValue(-4.0);
        }
        if (name == "variable_instance_get") {
            ++world.reads;
            Instance* i = byId(idOf(args[0]));
            if (!i) ++world.readsOnMissing;
            if (!i || !i->active) return RValue();   // undefined, as the runner answers for a sleeping or gone instance
            if (args[1].text == "x") return RValue(i->x);
            if (args[1].text == "y") return RValue(i->y);
            if (args[1].text == "minimapDiscoveredGrid") return i->grid >= 0 ? RValue(i->grid) : RValue();
            throw std::runtime_error("unexpected field " + args[1].text);
        }
        if (name == "instance_deactivate_object") {
            if (world.throwOnDeactivate) throw std::runtime_error("refused");
            ++world.deactivates;
            Instance* i = byId(idOf(args[0]));
            if (!i) { ++world.callsOnMissing; return RValue(); }
            i->active = false;
            return RValue();
        }
        if (name == "instance_activate_object") {
            ++world.activates;
            Instance* i = byId(idOf(args[0]));
            if (!i) { ++world.callsOnMissing; return RValue(); }
            i->active = true;
            return RValue();
        }
        if (name == "instance_exists") { ++world.exists; Instance* i = byId(idOf(args[0])); return RValue(i && i->active ? 1.0 : 0.0); }
        if (name == "view_get_camera") return RValue(7.0);
        if (name == "camera_get_view_width") return RValue(world.viewW);
        if (name == "camera_get_view_height") return RValue(world.viewH);
        throw std::runtime_error("unexpected builtin " + name);
    }
} runner;
static Runner* g_Yytk = &runner;

// PRODUCTION_FARSLEEP

using ForgePact::FarSleep;
static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}
static bool owner(const std::string& name) { world.ownerQueries.push_back(name); return world.frameOwners.count(name) != 0; }

enum Obj {
    Visual = 1, NoCollision = 2, Solid = 3, CollisionParent = 4, TreeParent = 5, ShrineParent = 6, PileParent = 7,
    EnemyParent = 8, Player = 9, WallParent = 10,
    Bush = 20, Hay = 21, Tree = 22, Rock = 23, Shrine = 24, Pile = 25, Trap = 26, Stepper = 27, Raven = 28, Zombie = 29,
    Unrelated = 30, Wall = 31, Minimap = 40,
};
static void resetWorld() {
    world = World();
    world.objects = {
        { Visual, { "Visual_Parent_obj", -1 } }, { NoCollision, { "Destructible_NoCollision_Parent_obj", -1 } },
        { CollisionParent, { "Collision_Parent_obj", -1 } }, { Solid, { "Collision_Prop_obj", CollisionParent } },
        { TreeParent, { "Tree_Parent_obj", Solid } }, { ShrineParent, { "Shrine_Parent_obj", Solid } },
        { PileParent, { "Pile_Parent_obj", Solid } }, { EnemyParent, { "Enemy_Parent_obj", -1 } },
        { Player, { "Player_obj", -1 } }, { WallParent, { "Wall_Parent_obj", CollisionParent } },
        { Bush, { "Fall_Bush_01_obj", Visual } }, { Hay, { "Fall_Hay_01_obj", NoCollision } },
        { Tree, { "Fall_Dead_Tree_01_obj", TreeParent } }, { Rock, { "Fall_Rock_01_obj", Solid } },
        { Shrine, { "Health_Shrine_obj", ShrineParent } }, { Pile, { "Fall_Pile_obj", PileParent } },
        { Trap, { "Trap_Hand_Statue_obj", Solid } }, { Stepper, { "Animated_Stepper_obj", Visual } },
        { Raven, { "Fall_Raven_Flying_obj", Visual } }, { Zombie, { "Zombie_obj", EnemyParent } },
        { Unrelated, { "Controller_obj", -1 } }, { Wall, { "Invisible_Wall_obj", WallParent } },
        { Minimap, { "objMinimap", -1 } },
        // index 0 exists and a gap of missing indices sits between 31 and 200:
        // the object table walk must not stop there.
        { 0, { "Menu_Controller_obj", -1 } }, { 200, { "Fall_Rock_02_obj", Visual } },
    };
    world.frameOwners = { "Pile_Parent_obj", "Animated_Stepper_obj" };
}
static int64_t g_NextId = 100001;
static void addGrid(int object, int n, double x0, double y0, double step) {
    for (int i = 0; i < n; ++i)
        world.instances.push_back({ g_NextId++, object, x0 + step * (i % 20), y0 + step * (i / 20), true, true });
}
static void addPlayer(double x, double y) { world.instances.push_back({ g_NextId++, Player, x, y, true, true }); }
// The zone's minimap holder: a new instance and a new grid each time a zone
// is (re)built, which is what the zone token watches.
static double g_NextGrid = 3;
static void addMinimap() { world.instances.push_back({ g_NextId++, Minimap, 0, 0, true, true, g_NextGrid++ }); }
static Instance& player(int n = 0) {
    for (auto& i : world.instances) if (i.object == Player && i.exists && n-- == 0) return i;
    throw std::runtime_error("no player");
}
static double dist(const Instance& a, const Instance& b) { return std::hypot(a.x - b.x, a.y - b.y); }
static double nearestPlayer(const Instance& i) {
    double best = 1e18;
    for (const auto& p : world.instances) if (p.object == Player && p.exists) best = std::min(best, dist(i, p));
    return best;
}
static bool isScenery(const Instance& i) { return i.object == Bush || i.object == Hay || i.object == Tree || i.object == Rock || i.object == Raven || i.object == 200; }
static FarSleep& fs() { return FarSleep::Instance(); }
static FarSleep::RoomInfo zone() { return { "Act_01_01", false, true }; }
static FarSleep::RoomInfo town() { return { "Town_01_rm", false, true }; }
static FarSleep::RoomInfo menu() { return { "Main_Menu_rm", false, true }; }
static FarSleep::RoomInfo dev() { return { "Dev_10_rm", false, true }; }
static FarSleep::RoomInfo persistent() { return { "Act_09_01", true, true }; }
static uint64_t frame = 1000;
static int maxCalls = 0;
template <class Probe>
static void run(int frames, int64_t room, Probe probe) {
    for (int k = 0; k < frames; ++k) {
        fs().OnFrame(frame++, room, probe);
        maxCalls = std::max(maxCalls, fs().CallsThisFrame());
    }
}
// Every scenery prop farther than the sleep radius (solid: the solid radius)
// asleep, every one inside the wake radius awake.
static int64_t g_NotYetKnown = -1;   // a prop made after the first scan, before the top-up scan
static std::string misplaced() {
    long awakeFar = 0, asleepNear = 0;
    for (const auto& i : world.instances) {
        if (!i.exists || !isScenery(i) || i.id == g_NotYetKnown) continue;
        const bool solid = i.object == Tree || i.object == Rock;
        const double r = solid ? fs().SolidSleepRadius() : fs().SleepRadius();
        const double w = solid ? fs().SolidWakeRadius() : fs().WakeRadius();
        const double d = nearestPlayer(i);
        if (i.active && d > r + 1) ++awakeFar;
        if (!i.active && d < w - 1) ++asleepNear;
    }
    return (awakeFar || asleepNear) ? "awakeFar=" + std::to_string(awakeFar) + " asleepNear=" + std::to_string(asleepNear) : "";
}
static long countInactive() { long n = 0; for (const auto& i : world.instances) if (i.exists && !i.active) ++n; return n; }
static long countInactiveOf(int object) { long n = 0; for (const auto& i : world.instances) if (i.exists && !i.active && i.object == object) ++n; return n; }

int main() {
    resetWorld();
    // A zone 20,000 px wide: 400 bushes, 300 hay, 300 trees, 100 rocks, 60
    // ravens, 20 of Fall_Rock_02_obj (object index 200, past the gap), plus
    // the objects far sleep must leave alone, and one player.
    addGrid(Bush, 400, 0, 0, 500);
    addGrid(Hay, 300, 250, 250, 500);
    addGrid(Tree, 300, 100, 300, 480);
    addGrid(Rock, 100, 300, 100, 900);
    addGrid(Raven, 60, 50, 50, 900);
    addGrid(200, 20, 700, 700, 950);
    addGrid(Shrine, 20, 0, 0, 1000);
    addGrid(Pile, 20, 400, 0, 1000);
    addGrid(Trap, 20, 0, 400, 1000);
    addGrid(Stepper, 20, 400, 400, 1000);
    addGrid(Zombie, 40, 0, 0, 500);
    addGrid(Wall, 40, 100, 100, 500);
    addGrid(Unrelated, 1, 0, 0, 0);
    addMinimap();
    addPlayer(5000, 3000);
    // A raven next to the player: it will fly along with the player later.
    world.instances.push_back({ g_NextId++, Raven, 5100, 3000, true, true });
    const int64_t ravenId = g_NextId - 1;

    // Off: nothing is asked of the game.
    run(50, 1, zone);
    check("off/no_calls", world.calls == 0, "calls=" + std::to_string(world.calls));

    // On: the object table is classified, a bounded slice a frame.
    fs().SetFrameEventOwner(&owner);
    fs().SetEnabled(true);
    for (int k = 0; k < 400 && !fs().Classified(); ++k) run(1, 1, zone);
    check("classify/finished", fs().Classified());
    check("classify/bounded_per_frame", maxCalls <= FarSleep::kCallsPerFrame, "max=" + std::to_string(maxCalls));
    check("classify/past_the_gap", fs().EligibleObjects() == 6, "eligible=" + std::to_string(fs().EligibleObjects()));
    const bool askedOwner = std::find(world.ownerQueries.begin(), world.ownerQueries.end(), "Animated_Stepper_obj") != world.ownerQueries.end();
    check("classify/asks_the_code_table", askedOwner);

    // Settling: nothing is scanned or put to sleep during the first 180
    // frames of a room.
    long findsBefore = world.propFinds;
    run(100, 1, zone);
    check("settle/no_scan_while_settling", world.propFinds == findsBefore && world.deactivates == 0, "finds=" + std::to_string(world.propFinds - findsBefore));

    // Then: a scan, and every far prop asleep; the near ones awake.
    run(600, 1, zone);
    check("zone/running", std::string(fs().ZoneStateName()) == "running", fs().ZoneStateName());
    check("zone/radii_follow_view", std::fabs(fs().WakeRadius() - (0.5 * std::hypot(1229.0, 691.0) + FarSleep::kWakeMarginPx)) < 1
        && std::fabs(fs().SleepRadius() - fs().WakeRadius() - FarSleep::kHysteresisPx) < 1);
    check("zone/everything_in_place", misplaced().empty(), misplaced());
    check("zone/some_asleep", countInactive() > 800, "inactive=" + std::to_string(countInactive()));
    check("zone/never_the_others", countInactiveOf(Shrine) == 0 && countInactiveOf(Pile) == 0 && countInactiveOf(Trap) == 0
        && countInactiveOf(Stepper) == 0 && countInactiveOf(Zombie) == 0 && countInactiveOf(Wall) == 0
        && countInactiveOf(Unrelated) == 0 && countInactiveOf(Player) == 0);
    check("zone/budget", maxCalls <= FarSleep::kCallsPerFrame, "max=" + std::to_string(maxCalls));
    // A prop the zone makes after the first scan (checked much later, once
    // the top-up scan has run).
    world.instances.push_back({ g_NextId++, Bush, 19900, 9900, true, true });
    const int64_t late = g_NextId - 1;
    g_NotYetKnown = late;

    // A raven that flew along with the player while its stored position says
    // far is not put to sleep: a stale position is read again first.
    run(100, 1, zone);
    player().y += 6000;
    byId(ravenId)->y += 6000;
    run(30, 1, zone);
    check("moving/stale_position_reread", byId(ravenId)->active && misplaced().empty(), misplaced());
    maxCalls = 0;
    run(30, 1, zone);

    // A prop broken (destroyed) while awake is never read again: when the
    // player walks away, the pass asks only whether it still exists.
    {
        int64_t broken = -1;
        for (auto& i : world.instances)
            if (isScenery(i) && i.active && i.id != ravenId && i.object != Raven) { broken = i.id; break; }
        byId(broken)->exists = false;
        const long missingReads = world.readsOnMissing;
        const double px = player().x, py = player().y;
        run(80, 1, zone);
        player().x += 8000;
        run(40, 1, zone);
        check("broken/never_read", world.readsOnMissing == missingReads, "reads=" + std::to_string(world.readsOnMissing - missingReads));
        player().x = px; player().y = py;
        run(40, 1, zone);
    }

    // Quiet frames with nobody moving: a pass costs no runner calls beyond
    // reading the player (and a verification slice once a second).
    long callsBefore = world.calls;
    run(60, 1, zone);
    check("steady/cheap", world.calls - callsBefore <= 60 * 4 + 2 * (long)FarSleep::kVerifySlice + 8, "calls=" + std::to_string(world.calls - callsBefore));

    // Walking: 150 px every 10 frames toward the east for 600 frames; after
    // each pass nothing inside the wake radius is asleep.
    std::string walkProblem;
    for (int step = 0; step < 60; ++step) {
        player().x += 150;
        run(10, 1, zone);
        run(2, 1, zone);
        const std::string m = misplaced();
        if (!m.empty() && walkProblem.empty()) walkProblem = "step " + std::to_string(step) + ": " + m;
    }
    check("walk/props_wake_ahead", walkProblem.empty(), walkProblem);
    check("walk/budget", maxCalls <= FarSleep::kCallsPerFrame, "max=" + std::to_string(maxCalls));

    // Hysteresis: in the middle of the props, a player stepping back and
    // forth by 300 px does not flip props that sit between the two radii.
    player().x = 6000; player().y = 5000;
    run(60, 1, zone);
    long flipsBefore = world.deactivates + world.activates;
    for (int k = 0; k < 20; ++k) { player().x += (k % 2) ? 300 : -300; run(12, 1, zone); }
    const long flips = world.deactivates + world.activates - flipsBefore;
    check("hysteresis/few_flips", flips < 60, "flips=" + std::to_string(flips));

    // A jump (a teleport) into another part of the props wakes the new
    // neighbourhood at once, with the bigger budget, and the old one sleeps.
    maxCalls = 0;
    const long activatesBefore = world.activates;
    player().x = 1500; player().y = 1500;
    run(1, 1, zone);
    run(1, 1, zone);
    check("jump/woken_at_once", misplaced().empty() && world.activates - activatesBefore > 20, misplaced()
        + " woken=" + std::to_string(world.activates - activatesBefore));
    check("jump/urgent_budget", maxCalls > FarSleep::kCallsPerFrame / 8 && maxCalls <= FarSleep::kUrgentCallsPerFrame
        && fs().StatsRef().urgentPasses >= 1,
        "max=" + std::to_string(maxCalls) + " urgent=" + std::to_string(fs().StatsRef().urgentPasses));

    // A second player keeps its own neighbourhood awake.
    addPlayer(2000, 8000);
    run(40, 1, zone);
    check("players/second_player_neighbourhood", misplaced().empty(), misplaced());

    // Solid props stay awake out to the hunt radius (+ margin) while monsters
    // hunt that far; the rest keep the ordinary radius.
    fs().SetHuntRadius(4000);
    run(40, 1, zone);
    check("hunt/solid_radius", std::fabs(fs().SolidWakeRadius() - (4000 + FarSleep::kSolidMarginPx)) < 1
        && std::fabs(fs().SolidSleepRadius() - fs().SolidWakeRadius() - FarSleep::kHysteresisPx) < 1 && misplaced().empty(), misplaced());
    fs().SetHuntRadius(-1);   // the whole map hunts: solid props never sleep
    run(60, 1, zone);
    check("hunt/whole_map_keeps_solid_awake", countInactiveOf(Tree) == 0 && countInactiveOf(Rock) == 0 && countInactiveOf(Bush) > 0,
        "trees asleep=" + std::to_string(countInactiveOf(Tree)));
    fs().SetHuntRadius(0);
    run(40, 1, zone);

    // The game wakes a sleeping prop by itself: the verification slice
    // notices and it is managed as awake from then on.
    long woke = 0;
    for (auto& i : world.instances) if (isScenery(i) && !i.active && woke < 200) { i.active = true; ++woke; }
    run(60 * 40, 1, zone);
    check("external/noticed", fs().StatsRef().externalWakes > 0 && misplaced().empty(),
        "external=" + std::to_string(fs().StatsRef().externalWakes) + " " + misplaced());

    // The prop made after the first scan was picked up by the top-up scan.
    g_NotYetKnown = -1;
    check("topup/late_prop_asleep", !byId(late)->active && fs().StatsRef().scans >= 2,
        "scans=" + std::to_string(fs().StatsRef().scans));

    // Off: everything it put to sleep wakes (bounded per frame), then no calls.
    fs().SetEnabled(false);
    maxCalls = 0;
    run(10, 1, zone);
    check("off/everything_awake", countInactive() == 0, "inactive=" + std::to_string(countInactive()));
    check("off/drain_budget", maxCalls <= FarSleep::kUrgentCallsPerFrame, "max=" + std::to_string(maxCalls));
    callsBefore = world.calls;
    run(100, 1, zone);
    check("off/quiet_after_drain", world.calls == callsBefore);

    // On again, then a room change: the old room's instances are gone and are
    // never addressed again; the new room is scanned on its own.
    fs().SetEnabled(true);
    run(900, 1, zone);
    check("again/asleep", countInactive() > 0);
    for (auto& i : world.instances) if (i.object != Player) i.exists = false;
    addGrid(Bush, 200, 30000, 0, 400);
    addGrid(Tree, 200, 30200, 200, 400);
    addMinimap();
    player().x = 30000; player().y = 0;
    long missingBefore = world.callsOnMissing;
    run(900, 2, zone);
    check("room/old_ids_never_addressed", world.callsOnMissing == missingBefore, "missing=" + std::to_string(world.callsOnMissing - missingBefore));
    check("room/new_room_managed", misplaced().empty() && countInactive() > 0, misplaced() + " inactive=" + std::to_string(countInactive()));

    // Rooms far sleep leaves alone.
    struct { const char* label; FarSleep::RoomInfo (*probe)(); } skipped[] = {
        { "town", town }, { "menu", menu }, { "dev", dev }, { "persistent", persistent } };
    int64_t key = 10;
    for (const auto& s : skipped) {
        const long deact = world.deactivates;
        const uint64_t skippedBefore = fs().StatsRef().zonesSkipped;
        run(900, key++, s.probe);
        check(std::string("skip/") + s.label, world.deactivates == deact && fs().StatsRef().zonesSkipped == skippedBefore + 1,
            "deactivates=" + std::to_string(world.deactivates - deact));
    }

    // A restart under the same room key: every instance is replaced, the
    // minimap holder too; the zone token notices and the new room is scanned.
    run(900, 2, zone);
    const uint64_t restartsBefore = fs().StatsRef().restarts;
    for (auto& i : world.instances) if (i.object != Player) i.exists = false;
    addGrid(Bush, 200, 30000, 0, 400);
    addGrid(Hay, 200, 30100, 100, 400);
    addMinimap();
    missingBefore = world.callsOnMissing;
    run(1200, 2, zone);
    check("restart/rescanned", fs().StatsRef().restarts > restartsBefore && misplaced().empty() && countInactive() > 0,
        "restarts=" + std::to_string(fs().StatsRef().restarts - restartsBefore) + " " + misplaced());
    check("restart/old_ids_barely_addressed", world.callsOnMissing - missingBefore < 200,
        "missing=" + std::to_string(world.callsOnMissing - missingBefore));

    // A runner that refuses deactivation: the zone is given up after a bounded
    // number of errors, and nothing stays asleep.
    fs().SetEnabled(false); run(10, 2, zone);
    for (auto& i : world.instances) if (i.object != Player) i.exists = false;
    addGrid(Bush, 300, 40000, 0, 400);
    addMinimap();
    player().x = 40000; player().y = 0;
    world.throwOnDeactivate = true;
    fs().SetEnabled(true);
    run(1200, 3, zone);
    check("errors/zone_given_up", std::string(fs().ZoneStateName()) == "skipped" && countInactive() == 0,
        std::string(fs().ZoneStateName()) + " inactive=" + std::to_string(countInactive()));
    world.throwOnDeactivate = false;

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
