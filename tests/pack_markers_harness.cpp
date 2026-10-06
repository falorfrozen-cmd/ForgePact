// Behavioral regression harness for the pack markers (map reveal's monster
// half since 1.4.5).
//
// The Python runner injects the REAL ForgePact::PackMarkers class below. Only
// the game API is replaced; no game process is touched. The controlled world
// holds spawners with an `enemyCreatorTimer` that is undefined until the
// spawner initialises and undefined again once it has given birth, which is
// exactly what the live creator does (docs/population-performance-analysis.md).
//
// It can also hold what the runtime may answer for the other six creator kinds
// (issue #181): a spawner with no `enemyCreatorTimer` variable at all, and an
// `enemyArray` per spawner that is undefined before the birth and an array
// after it (RUNTIME_DATA_MODELS 11.2), absent, or some other value.
// `variable_instance_exists` tells absent from undefined, as the runner does;
// `variable_instance_get` of an absent variable answers undefined.
//
// And the creators' protected pack state as the runtime holds it (replan 2's
// static reading, RUNTIME_DATA_MODELS 13.7): a spawner may carry `spawnPack`,
// holding a key (a whole number naming a record of a fake protected store) or
// anything else, or not carry it at all; the record is unset (undefined) or
// holds a value; the store is read only through the runner's game-script call,
// by the getter's script name. A key that is not a whole number in 0..262143
// faulted the live game, so the fake counts every such call and the scenarios
// fail on one. Pack members are instances of their own, alive or not.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_BOOL, VALUE_ARRAY };
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const { if (m_Kind == VALUE_STRING || m_Kind == VALUE_UNDEFINED) throw std::runtime_error("not a number"); return number; }
    bool ToBoolean() const { return number != 0; }
};

// ---- the controlled world -------------------------------------------------
// How a spawner carries a variable: as the live Enemy_Creator_obj does
// (Usual: the timer a number while armed, enemyArray an array once born), not
// at all (Absent), or as a value of another kind (Odd: a string timer, a
// numeric enemyArray).
enum VarMode { Usual = 0, Absent, Odd };
struct Spawner {
    int64_t id; int kind; double x, y; bool exists; bool initialised; bool spawned;
    int timerMode = Usual; int arrayMode = Usual;
    bool hasPack = false;   // carries a `spawnPack` variable
    RValue packKey;         // what it holds: a store key, or anything else
};
struct World {
    std::vector<Spawner> spawners;
    std::map<std::string, int> objects;   // creator family name -> object index
    double hudRes = 2.0;
    long finds = 0, reads = 0, numbers = 0, exists = 0, draws = 0, rings = 0, texts = 0, spriteAdds = 0, spriteDeletes = 0;
    long timerReads = 0, arrayReads = 0, existsChecks = 0;
    long spriteAddResult = 500;           // sprite_add answers 500+n, or -1 when negative
    bool refuseRelative = false;          // the sandbox refuses the relative path: only the absolute one works
    std::vector<std::string> addedPaths;
    std::vector<std::string> drawn;       // "subimg,x,y,scale,colour,alpha" per sprite draw
    double drawAlpha = 1.0, drawColour = 16777215.0, drawFont = 0.0;   // the runner's global draw state
    bool fontIsRef = false;               // draw_get_font answers with an asset reference, as sprites do here
    // Lookup caches, so a zone of 70000 spawners (the memory bound) lists in
    // linear time: id -> positions, and the last instance_find answered.
    std::unordered_map<int64_t, std::vector<size_t>> idIndex;
    size_t indexedSize = (size_t)-1; const Spawner* indexedData = nullptr;
    int findObj = -1, findI = -1; size_t findPos = 0, findSize = 0;
};
static World world;
// Session-long, like the game's own: the protected store, the getter's call
// counts and the pack members.
static std::map<int64_t, RValue> store;          // key -> record; a missing key is an unset record
static long getterCalls = 0, badKeyCalls = 0;    // badKeyCalls: a refused key reached the getter
static std::map<int64_t, bool> monsters;         // pack member id -> alive
static int familyIndex(const std::string& name) { auto it = world.objects.find(name); return it == world.objects.end() ? -1 : it->second; }
static Spawner* byId(int64_t id) {
    if (world.indexedSize != world.spawners.size() || world.indexedData != world.spawners.data()) {
        world.idIndex.clear();
        for (size_t i = 0; i < world.spawners.size(); ++i) world.idIndex[world.spawners[i].id].push_back(i);
        world.indexedSize = world.spawners.size(); world.indexedData = world.spawners.data();
    }
    auto it = world.idIndex.find(id);
    if (it == world.idIndex.end()) return nullptr;
    for (size_t i : it->second) {
        Spawner& s = world.spawners[i];
        if (s.id != id) { world.indexedSize = (size_t)-1; return byId(id); }   // stale: rebuild
        if (s.exists) return &s;
    }
    return nullptr;
}
namespace HeroSiege::Scripts {
inline constexpr std::string_view gml_Script_PC_GetVariableGMLWrapper = "gml_Script_PC_GetVariableGMLWrapper";
}

struct Runner {
    RValue CallBuiltin(const char* key, std::vector<RValue> args) {
        const std::string name(key);
        if (name == "asset_get_index") return RValue((double)familyIndex(args[0].text));
        if (name == "instance_number") {
            ++world.numbers;
            const int obj = (int)args[0].number; long n = 0;
            for (const auto& s : world.spawners) if (s.exists && s.kind == obj) ++n;
            return RValue((double)n);
        }
        if (name == "instance_find") {
            ++world.finds;
            const int obj = (int)args[0].number; const int i = (int)args[1].number; int remaining = i;
            // The next index of the same walk continues where the last stopped.
            size_t from = 0;
            if (obj == world.findObj && i == world.findI + 1 && world.findSize == world.spawners.size()) { from = world.findPos + 1; remaining = 0; }
            for (size_t p = from; p < world.spawners.size(); ++p) {
                const auto& s = world.spawners[p];
                if (s.exists && s.kind == obj && remaining-- == 0) {
                    world.findObj = obj; world.findI = i; world.findPos = p; world.findSize = world.spawners.size();
                    RValue v((double)s.id); v.m_Kind = VALUE_REF; return v;
                }
            }
            world.findObj = -1;
            return RValue(-4.0);
        }
        if (name == "instance_exists") {
            ++world.exists;
            const int64_t id = (int64_t)args[0].number;
            auto member = monsters.find(id);
            return RValue(byId(id) || (member != monsters.end() && member->second) ? 1.0 : 0.0);
        }
        if (name == "variable_instance_get") {
            ++world.reads;
            Spawner* s = byId((int64_t)args[0].number);
            if (!s) throw std::runtime_error("no such instance");
            const std::string& field = args[1].text;
            if (field == "x") return RValue(s->x);
            if (field == "y") return RValue(s->y);
            if (field == "id") return RValue((double)s->id);
            if (field == "enemyCreatorTimer") {
                ++world.timerReads;
                if (s->timerMode == Absent) return RValue();
                if (s->timerMode == Odd) return RValue("soon");
                if (s->initialised && !s->spawned) return RValue(116.0);
                return RValue();
            }
            if (field == "enemyArray") {
                ++world.arrayReads;
                if (s->arrayMode == Absent) return RValue();
                if (s->arrayMode == Odd) return RValue(-1.0);
                if (s->spawned) { RValue a; a.m_Kind = VALUE_ARRAY; return a; }
                return RValue();
            }
            if (field == "spawnPack") return s->hasPack ? s->packKey : RValue();
            throw std::runtime_error("unexpected field " + field);
        }
        if (name == "variable_instance_exists") {
            ++world.existsChecks;
            Spawner* s = byId((int64_t)args[0].number);
            if (!s) return RValue(0.0);
            const std::string& field = args[1].text;
            if (field == "enemyCreatorTimer") return RValue(s->timerMode == Absent ? 0.0 : 1.0);
            if (field == "enemyArray") return RValue(s->arrayMode == Absent ? 0.0 : 1.0);
            if (field == "spawnPack") return RValue(s->hasPack ? 1.0 : 0.0);
            return RValue(field == "x" || field == "y" || field == "id" ? 1.0 : 0.0);
        }
        if (name == "is_array") return RValue(args[0].m_Kind == VALUE_ARRAY ? 1.0 : 0.0);
        if (name == "variable_global_get") { if (args[0].text == "hud_res") return RValue(world.hudRes); if (args[0].text == "font_smallest") return RValue(3.0); return RValue(); }
        if (name == "draw_sprite_ext") {
            ++world.draws;
            world.drawn.push_back(std::to_string((int)args[1].number) + "," + std::to_string((long long)std::llround(args[2].number)) + "," + std::to_string((long long)std::llround(args[3].number)) + "," + std::to_string(args[4].number) + "," + std::to_string((long long)args[7].number) + "," + std::to_string(args[8].number));
            return RValue();
        }
        if (name == "draw_circle") { ++world.rings; return RValue(); }
        if (name == "draw_set_alpha") { world.drawAlpha = args[0].number; return RValue(); }
        if (name == "draw_set_colour") { world.drawColour = args[0].number; return RValue(); }
        if (name == "draw_get_alpha") return RValue(world.drawAlpha);
        if (name == "draw_get_colour") return RValue(world.drawColour);
        if (name == "draw_get_font") { RValue f(world.drawFont); if (world.fontIsRef) f.m_Kind = VALUE_REF; return f; }
        if (name == "draw_set_font") { world.drawFont = args[0].number; return RValue(); }
        if (name == "draw_text") { ++world.texts; return RValue(); }
        if (name == "sprite_add") {
            ++world.spriteAdds; world.addedPaths.push_back(args[0].text);
            // The live runner answers with an asset REFERENCE, not a real; a
            // relative path is refused (-1) when the fake sandbox says so.
            const bool relative = args[0].text.rfind("bp_ipc", 0) == 0;
            if (world.spriteAddResult < 0 || (relative && world.refuseRelative)) return RValue(-1.0);
            RValue r((double)(world.spriteAddResult + world.spriteAdds)); r.m_Kind = VALUE_REF; return r;
        }
        if (name == "sprite_get_width" || name == "sprite_get_height") return RValue(16.0);
        if (name == "sprite_set_offset") return RValue();
        if (name == "sprite_delete") { ++world.spriteDeletes; return RValue(); }
        throw std::runtime_error("unexpected builtin " + name);
    }
    // The protected store's getter, the only script the class may call. A key
    // the live game would fault on is counted and throws.
    RValue CallGameScript(std::string_view name, const std::vector<RValue>& args) {
        if (name != HeroSiege::Scripts::gml_Script_PC_GetVariableGMLWrapper) throw std::runtime_error("unexpected script " + std::string(name));
        ++getterCalls;
        const RValue key = args.empty() ? RValue() : args[0];
        const bool numberKind = key.m_Kind == VALUE_REAL || key.m_Kind == VALUE_INT32 || key.m_Kind == VALUE_INT64;
        if (!numberKind || !(key.number >= 0 && key.number < 262144.0) || std::floor(key.number) != key.number) {
            ++badKeyCalls;
            throw std::runtime_error("a key outside the store faults the game");
        }
        auto it = store.find((int64_t)key.number);
        return it == store.end() ? RValue() : it->second;
    }
} runner;
static Runner* g_Yytk = &runner;

// PRODUCTION_PACKMARKERS

using ForgePact::PackMarkers;
static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << "\n";
    if (!ok) ++failures;
}
static PackMarkers& pm() { return PackMarkers::Instance(); }
static bool readable() { return true; }
static bool unreadable() { return false; }
static bool iconProvider(int kind, std::string& path, std::string& absolute) {
    static const char* names[] = { "normal", "ambush", "ancient", "champion", "colossal_chest", "legion", "miniboss" };
    if (kind < 0 || kind > 6) return false;
    path = std::string("bp_ipc\\packmarks\\") + names[kind] + ".png";
    absolute = std::string("C:\\game\\bin\\bp_ipc\\packmarks\\") + names[kind] + ".png";
    return true;
}
static void resetWorld() {
    world = World();
    world.objects = { {"Enemy_Creator_obj", 10}, {"Enemy_Creator_Ambush_obj", 11}, {"Enemy_Creator_Ancient_obj", 12},
                      {"Enemy_Creator_Champion_obj", 13}, {"Enemy_Creator_Colossal_Chest_obj", 14}, {"Enemy_Creator_Legion_obj", 15},
                      {"Enemy_Creator_Miniboss_obj", 16} };
}
// The scenarios after the first two zones each start a zone of their own: a
// fresh world, a new zone generation, and frames run until it is listed.
static uint64_t clockFrame = 0;
static uint64_t zoneNow = 2;
// The room key the frame callback hands the markers (CurrentRoomKey()).
static int64_t roomNow = PackMarkers::kUnknownRoom;
static bool marked(int64_t id) { for (const auto& m : pm().Markers()) if (m.id == id) return true; return false; }
static void runFrames(uint64_t n) { for (uint64_t i = 0; i < n; ++i) pm().OnFrame(clockFrame++, zoneNow, readable, roomNow); }
static void enterZone(const std::vector<Spawner>& spawners) {
    resetWorld();
    world.spawners = spawners;
    ++zoneNow;
    const uint64_t before = pm().Enumerations();
    for (int i = 0; i < 1000 && pm().Enumerations() == before; ++i) runFrames(1);
}
// The same, in a room of a known key (a revisit is the same key again).
static void enterRoom(int64_t room, const std::vector<Spawner>& spawners) { roomNow = room; enterZone(spawners); }
static Spawner spawnerOf(int64_t id, int obj, double x, double y) { return Spawner{ id, obj, x, y, true, true, false }; }
// A spawner holding `key` in spawnPack, whatever that is.
static Spawner keyed(Spawner s, RValue key) { s.hasPack = true; s.packKey = key; return s; }
// A spawner whose spawnPack names a fresh store record holding `state`
// (RValue() = unset).
static int64_t nextKey = 100;
static Spawner packed(Spawner s, RValue state) { const int64_t key = nextKey++; store[key] = state; return keyed(s, RValue((double)key)); }
// A special creator as Live 1 found the ancient and miniboss ones: no
// enemyCreatorTimer and no enemyArray, its pack state behind spawnPack.
static Spawner special(int64_t id, int obj, double x, double y, RValue state) {
    Spawner s = spawnerOf(id, obj, x, y); s.timerMode = Absent; s.arrayMode = Absent;
    return packed(s, state);
}
static void setState(int64_t id, RValue state) {
    for (const auto& s : world.spawners) if (s.id == id && s.hasPack) store[(int64_t)s.packKey.number] = state;
}

int main() {
    resetWorld();
    // Issue #181, shipped: a fresh instance retires by `kind` in both builds
    // (Live 4 measured each kind's birth signal). The scenarios below up to
    // the policy section pin the old `timer` rules, so they select it
    // explicitly rather than relying on the default.
    check("retire/default_is_kind", PackMarkers::Instance().GetRetire() == PackMarkers::Retire::Kind
        && std::string(PackMarkers::RetireName(PackMarkers::Instance().GetRetire())) == "kind");
    PackMarkers::Instance().SetRetire(PackMarkers::Retire::Timer);
    // Zone one: 300 plain packs, 4 champion packs, 2 ancient packs, 1 mini boss,
    // 3 legion spawners. Everything initialises immediately except spawner 7.
    for (int i = 0; i < 300; ++i) world.spawners.push_back({ 1000 + i, 10, 100.0 * i, 50.0 * i, true, i != 7, false });
    for (int i = 0; i < 4; ++i) world.spawners.push_back({ 2000 + i, 13, 5000 + 10.0 * i, 700, true, true, false });
    for (int i = 0; i < 2; ++i) world.spawners.push_back({ 3000 + i, 12, 6000, 800 + 10.0 * i, true, true, false });
    world.spawners.push_back({ 4000, 16, 7000, 900, true, true, false });
    for (int i = 0; i < 3; ++i) world.spawners.push_back({ 5000 + i, 15, 8000.0 + i, 950, true, true, false });

    // Off: nothing is asked of the game at all.
    pm().OnFrame(1, 1, readable);
    check("off/no_calls", world.finds == 0 && world.numbers == 0 && world.reads == 0);

    // On, zone 1, map not readable yet: still nothing enumerated.
    pm().SetEnabled(true);
    pm().OnFrame(100, 1, unreadable);
    check("loading/no_enumeration", pm().Count() == 0 && world.finds == 0);

    // Map readable: one enumeration, every existing spawner marked.
    pm().OnFrame(101, 1, readable);
    check("ready/enumerated_all", pm().Count() == 310, "count=" + std::to_string(pm().Count()));
    check("ready/one_enumeration", pm().Enumerations() == 1);
    check("ready/reads_position_only", world.reads == 310 * 2, "reads=" + std::to_string(world.reads));
    long findsAfterEnumerate = world.finds;

    // Steady state: a rotating slice of 32 per frame, no re-enumeration while
    // the creator count is stable.
    world.reads = 0; world.exists = 0;
    for (uint64_t f = 102; f < 102 + 60; ++f) pm().OnFrame(f, 1, readable);
    check("steady/no_reenumeration", pm().Enumerations() == 1 && world.finds == findsAfterEnumerate);
    check("steady/bounded_checks_per_frame", world.exists <= 60 * 32 && world.reads <= 60 * 32, "exists=" + std::to_string(world.exists) + " reads=" + std::to_string(world.reads));
    check("steady/all_still_marked", pm().Count() == 310);

    // A birth reported by the create hook drops that marker immediately.
    world.spawners[0].spawned = true;
    pm().MarkSpawned(1000);
    check("birth/marker_dropped", pm().Count() == 309 && pm().Spawned() == 1);
    pm().MarkSpawned(1000);
    check("birth/idempotent", pm().Count() == 309 && pm().Spawned() == 1);
    pm().MarkSpawned(999999);
    check("birth/unknown_id_ignored", pm().Count() == 309);

    // A spawner that gave birth without the hook noticing (timer gone) is
    // dropped by the rotating check within one full rotation.
    world.spawners[1].spawned = true;
    for (uint64_t f = 200; f < 200 + 20; ++f) pm().OnFrame(f, 1, readable);
    check("rotation/spawned_dropped", pm().Count() == 308, "count=" + std::to_string(pm().Count()));
    // A destroyed spawner is dropped the same way.
    world.spawners[2].exists = false;
    for (uint64_t f = 220; f < 220 + 20; ++f) pm().OnFrame(f, 1, readable);
    check("rotation/destroyed_dropped", pm().Count() == 307, "count=" + std::to_string(pm().Count()));
    // The never-initialised spawner 7 is kept while young and dropped after
    // the give-up window, never resurrected by a later re-enumeration.
    bool seven = false; for (const auto& m : pm().Markers()) if (m.id == 1007) seven = true;
    check("unarmed/kept_while_young", seven);
    for (uint64_t f = 240; f < 101 + PackMarkers::kUnarmedGiveUpFrames + 40; ++f) pm().OnFrame(f, 1, readable);
    seven = false; for (const auto& m : pm().Markers()) if (m.id == 1007) seven = true;
    check("unarmed/dropped_after_give_up", !seven && pm().Count() == 306, "count=" + std::to_string(pm().Count()));

    // A legion spawner appearing later (creator count grows) is picked up by
    // the next count poll without resurrecting anything already dropped.
    world.spawners.push_back({ 6000, 15, 9000, 990, true, true, false });
    uint64_t f = 101 + PackMarkers::kUnarmedGiveUpFrames + 40;
    while ((f % PackMarkers::kCountPollFrames) != 0) pm().OnFrame(f++, 1, readable);
    pm().OnFrame(f++, 1, readable);
    check("growth/reenumerated", pm().Enumerations() == 2 && pm().Count() == 307, "count=" + std::to_string(pm().Count()) + " enumerations=" + std::to_string(pm().Enumerations()));
    bool resurrected = false; for (const auto& m : pm().Markers()) if (m.id == 1000 || m.id == 1001 || m.id == 1002 || m.id == 1007) resurrected = true;
    check("growth/no_resurrection", !resurrected);

    // The default look is primitives (MEASURED 2026-09-22: the sprite path drew
    // nothing visible in-game, the circles landed on the packs): one circle
    // per marker, drawn with the game's own layer arguments.
    RValue sx(0.0625), sy(0.0625), off(-64.0), sprite(13200.0); sprite.m_Kind = VALUE_REF;
    // Without an icon provider (this harness's default) the markers are
    // opaque dots on a dark outline disc: two circles per marker, one marker
    // per spawner while clustering is off.
    pm().StyleRef().clusterPx = 0; pm().StyleChanged();
    world.rings = 0; world.texts = 0;
    pm().Draw(sx, sy, off, sprite);
    check("draw/default_is_primitives", pm().StyleRef().ring && pm().StyleRef().outline && pm().StyleRef().alpha == 1.0
        && world.rings == 2 * (long)pm().Count() && world.texts == 0, "rings=" + std::to_string(world.rings));
    pm().StyleRef().outline = false; world.rings = 0;
    pm().Draw(sx, sy, off, sprite);
    check("draw/outline_off_one_circle", world.rings == (long)pm().Count(), "rings=" + std::to_string(world.rings));
    // Clustering: the four champion spawners 10 px apart and the two ancient
    // ones collapse into one marker each, the rarest kind wins the cell, and a
    // count badge (shadow + text) is drawn for every collapsed cluster.
    pm().StyleRef().clusterPx = 96; pm().StyleChanged();
    world.rings = 0; world.texts = 0;
    pm().Draw(sx, sy, off, sprite);
    long collapsed = 0, championCluster = 0, ancientCluster = 0;
    for (const auto& c : pm().ClusterList()) {
        if (c.count > 1) ++collapsed;
        if (c.kind == PackMarkers::Champion && c.count == 4) ++championCluster;
        if (c.kind == PackMarkers::Ancient && c.count == 2) ++ancientCluster;
    }
    check("cluster/nearby_spawners_collapse", championCluster == 1 && ancientCluster == 1 && pm().Clusters() < pm().Count(),
        "clusters=" + std::to_string(pm().Clusters()) + " of " + std::to_string(pm().Count()));
    check("cluster/one_marker_per_cluster", world.rings == (long)pm().Clusters(), "rings=" + std::to_string(world.rings));
    check("cluster/badge_per_collapsed_cluster", world.texts == 2 * collapsed, "texts=" + std::to_string(world.texts) + " collapsed=" + std::to_string(collapsed));
    pm().StyleRef().badge = false; world.texts = 0;
    pm().Draw(sx, sy, off, sprite);
    check("cluster/badge_off", world.texts == 0);
    pm().StyleRef().badge = true;
    // Icons: with a provider, every kind's PNG is added once (with the game's
    // relative path), each cluster is one sprite draw of its kind's icon, and
    // a failed sprite_add falls back to the dots for that kind.
    pm().SetIconProvider(&iconProvider);
    world.spriteAdds = 0; world.rings = 0; world.draws = 0; world.drawn.clear();
    pm().Draw(sx, sy, off, sprite);
    check("icons/loaded_once_per_kind", world.spriteAdds == 7 && pm().IconsLoaded() == 7 && world.addedPaths[0] == "bp_ipc\\packmarks\\normal.png",
        "adds=" + std::to_string(world.spriteAdds) + " loaded=" + std::to_string(pm().IconsLoaded()));
    check("icons/one_sprite_per_cluster", world.draws == (long)pm().Clusters() && world.rings == 0, "draws=" + std::to_string(world.draws) + " rings=" + std::to_string(world.rings));
    world.spriteAdds = 0;
    pm().Draw(sx, sy, off, sprite);
    check("icons/not_reloaded_every_draw", world.spriteAdds == 0);
    // A sandbox that refuses the relative path: the absolute one is tried next
    // and every icon still loads (two adds per kind).
    pm().ReloadIcons(); world.refuseRelative = true; world.spriteAdds = 0; world.draws = 0;
    pm().Draw(sx, sy, off, sprite);
    check("icons/absolute_path_fallback", pm().IconsLoaded() == 7 && pm().IconsViaAbsolute() == 7 && world.spriteAdds == 14 && world.draws == (long)pm().Clusters(),
        "loaded=" + std::to_string(pm().IconsLoaded()) + " viaAbsolute=" + std::to_string(pm().IconsViaAbsolute()) + " adds=" + std::to_string(world.spriteAdds));
    world.refuseRelative = false; world.spriteDeletes = 0;
    pm().ReloadIcons(); world.spriteAddResult = -1; world.spriteAdds = 0; world.rings = 0; world.draws = 0;
    pm().Draw(sx, sy, off, sprite);
    check("icons/failed_add_falls_back_to_dots", world.spriteDeletes == 7 && pm().IconsLoaded() == 0 && world.draws == 0 && world.rings == (long)pm().Clusters(),
        "deletes=" + std::to_string(world.spriteDeletes) + " rings=" + std::to_string(world.rings));
    world.spriteAddResult = 500; pm().ReloadIcons();
    pm().StyleRef().icons = false; pm().SetIconProvider(nullptr); pm().StyleRef().clusterPx = 0; pm().StyleChanged();
    // The sprite path: x' = 32 + x*sx, y' = 32 + (y+off)*sy.
    pm().StyleRef().ring = false;
    world.drawn.clear(); world.draws = 0;
    pm().Draw(sx, sy, off, sprite);
    check("draw/one_call_per_marker", world.draws == 307, "draws=" + std::to_string(world.draws));
    bool placed = false, championIcon = false;
    for (const auto& d : world.drawn) {
        // spawner 1003: x=300,y=150 -> 32+18.75=50.75 -> 51 ; 32+(150-64)*0.0625=37.375 -> 37
        if (d.rfind(std::to_string((int)PackMarkers::Style().subimage[PackMarkers::Normal]) + ",51,37,", 0) == 0) placed = true;
        if (d.rfind(std::to_string((int)PackMarkers::Style().subimage[PackMarkers::Champion]) + ",", 0) == 0) championIcon = true;
    }
    check("draw/placement_matches_game_formula", placed);
    check("draw/kind_selects_icon", championIcon);
    check("draw/scale_uses_hud_res", !world.drawn.empty() && world.drawn[0].find("," + std::to_string(PackMarkers::Style().scale * world.hudRes) + ",") != std::string::npos);
    // An unusable transform draws nothing and never throws; an unusable
    // sprite falls back to rings so the packs still show.
    world.draws = 0; world.rings = 0;
    pm().Draw(RValue("x"), sy, off, sprite);
    check("draw/bad_args_skip", world.draws == 0 && world.rings == 0, "draws=" + std::to_string(world.draws) + " rings=" + std::to_string(world.rings));
    pm().Draw(sx, sy, off, RValue("nope"));
    check("draw/bad_sprite_rings", world.draws == 0 && world.rings == (long)pm().Count(), "rings=" + std::to_string(world.rings));
    // Draw state is the game's: whatever colour, alpha and font the layer had
    // before the markers, it has again after them, on the sprite path (ring and
    // icons off, a usable sprite) as well as on the dot path, badges included.
    auto drawStateKept = [&](const char* label) {
        pm().StyleRef().clusterPx = 96; pm().StyleChanged();
        world.drawAlpha = 0.35; world.drawColour = 1193046.0; world.drawFont = 7.0; world.texts = 0;
        pm().Draw(sx, sy, off, sprite);
        check(label, world.texts > 0 && world.drawAlpha == 0.35 && world.drawColour == 1193046.0 && world.drawFont == 7.0,
            "texts=" + std::to_string(world.texts) + " alpha=" + std::to_string(world.drawAlpha)
            + " colour=" + std::to_string(world.drawColour) + " font=" + std::to_string(world.drawFont));
        world.drawAlpha = 1.0; world.drawColour = 16777215.0; world.drawFont = 0.0;
        pm().StyleRef().clusterPx = 0; pm().StyleChanged();
    };
    drawStateKept("draw/state_restored_on_sprite_path");
    pm().StyleRef().ring = true;
    drawStateKept("draw/state_restored_on_primitive_path");
    world.fontIsRef = true;
    drawStateKept("draw/font_restored_when_the_runner_answers_a_reference");
    world.fontIsRef = false;
    pm().StyleRef().ring = false;
    pm().StyleRef().ring = true; world.rings = 0;   // outline still off from above
    pm().Draw(sx, sy, RValue(), sprite);
    check("draw/ring_style", world.rings == (long)pm().Count());
    pm().StyleRef().outline = true;

    // Zone change: the list is cleared, dropped ids may mark again in the new
    // zone (they are new spawners there), and enumeration waits for the map.
    resetWorld();
    for (int i = 0; i < 5; ++i) world.spawners.push_back({ 1000 + i, 10, 10.0 * i, 0, true, true, false });
    pm().OnFrame(f + 1, 2, unreadable);
    check("zone/cleared_on_generation_change", pm().Count() == 0);
    pm().OnFrame(f + PackMarkers::kEnumerateMinGapFrames + 2, 2, readable);
    check("zone/fresh_enumeration", pm().Count() == 5, "count=" + std::to_string(pm().Count()));

    // A density copy made on its own as the player walks (rolling density
    // copies) is counted into its family: the count poll that follows does
    // not re-list the zone for it.
    uint64_t g = f + PackMarkers::kEnumerateMinGapFrames + 3;
    const uint64_t enumsBeforeCopy = pm().Enumerations();
    world.spawners.push_back({ 1100, 10, 60, 0, true, true, false });
    pm().NoteCopy(10);
    for (int k = 0; k < 4 * static_cast<int>(PackMarkers::kCountPollFrames); ++k) pm().OnFrame(g++, 2, readable);
    check("copy/no_relisting", pm().Enumerations() == enumsBeforeCopy && pm().CopiesNoted() == 1,
        "enumerations=" + std::to_string(pm().Enumerations() - enumsBeforeCopy));
    // A spawner the game makes itself still grows the family past its peak
    // and is listed.
    world.spawners.push_back({ 1101, 10, 70, 0, true, true, false });
    for (int k = 0; k < 4 * static_cast<int>(PackMarkers::kCountPollFrames) && pm().Enumerations() == enumsBeforeCopy; ++k)
        pm().OnFrame(g++, 2, readable);
    check("copy/real_growth_still_lists", pm().Enumerations() == enumsBeforeCopy + 1 && pm().Count() == 7,
        "enumerations=" + std::to_string(pm().Enumerations() - enumsBeforeCopy) + " count=" + std::to_string(pm().Count()));
    clockFrame = g;

    // ---- issue #181 baseline: how markers retire under today's rules -------
    // A special-kind spawner that carries no enemyCreatorTimer at all (as the
    // six other creator kinds may not) is listed and kept while young, then
    // given up as "spent before we looked" once kUnarmedGiveUpFrames pass,
    // although its pack was never born. Pinned as it is: the suspected cause
    // of the icons giving way a few seconds after arrival.
    {
        Spawner champion = spawnerOf(7000, 13, 100, 100); champion.timerMode = Absent;
        Spawner plain = spawnerOf(7001, 10, 900, 900);
        enterZone({ champion, plain });
        const uint64_t listedAt = clockFrame;
        check("timerless/listed", marked(7000) && marked(7001), "count=" + std::to_string(pm().Count()));
        runFrames(PackMarkers::kUnarmedGiveUpFrames / 2);
        check("timerless/kept_while_young", marked(7000));
        while (clockFrame < listedAt + PackMarkers::kUnarmedGiveUpFrames + 10) runFrames(1);
        check("timerless/dropped_after_give_up", !marked(7000) && marked(7001) && !world.spawners[0].spawned,
            "count=" + std::to_string(pm().Count()));
    }
    // A create the hooks attribute to a spawner retires its marker at once,
    // whatever the spawner's own state says.
    {
        enterZone({ spawnerOf(7100, 13, 100, 100), spawnerOf(7101, 13, 900, 900) });
        pm().MarkSpawned(7100);
        check("attributed/retires_at_once", !marked(7100) && marked(7101) && !world.spawners[0].spawned,
            "count=" + std::to_string(pm().Count()));
    }

    // ---- issue #181: the retirement policy -------------------------------
    using Retire = PackMarkers::Retire;
    auto st = [](int kind) -> const PackMarkers::KindStats& { return pm().Stats(kind); };
    // Rotations: the rotating check visits every marker within ceil(n/32) frames.
    auto rotation = []() { return (pm().Count() + PackMarkers::kValidatePerFrame - 1) / PackMarkers::kValidatePerFrame; };
    // Still `timer` here: selected at the top of main, not the default.
    check("retire/timer_selected_explicitly", pm().GetRetire() == Retire::Timer
        && std::string(PackMarkers::RetireName(Retire::Timer)) == "timer" && std::string(PackMarkers::RetireName(Retire::State)) == "state");
    // Switching policy at runtime keeps every marker held.
    {
        enterZone({ spawnerOf(7200, 13, 100, 100), spawnerOf(7201, 10, 900, 900) });
        pm().SetRetire(Retire::State);
        runFrames(rotation());
        const bool keptUnderState = pm().Count() == 2;
        pm().SetRetire(Retire::Timer);
        runFrames(rotation());
        check("retire/selectable_at_runtime", keptUnderState && pm().Count() == 2 && pm().GetRetire() == Retire::Timer,
            "count=" + std::to_string(pm().Count()));
    }

    // ---- issue #181 target: the state policy ------------------------------
    pm().SetRetire(Retire::State);
    // A special spawner without a timer keeps its marker past the give-up
    // window: an unborn pack is not "spent" for lacking a timer.
    {
        Spawner champion = spawnerOf(7300, 13, 100, 100); champion.timerMode = Absent;
        enterZone({ champion });
        const uint64_t listedAt = clockFrame;
        while (clockFrame < listedAt + 2 * PackMarkers::kUnarmedGiveUpFrames) runFrames(1);
        check("state/timerless_kept_past_give_up", marked(7300) && st(PackMarkers::Champion).retired[PackMarkers::ReasonGivenUp] == 0,
            "count=" + std::to_string(pm().Count()));
    }
    // A spawner whose enemyArray becomes an array (its pack is born) loses its
    // marker within one rotation; the 69 others keep theirs.
    {
        std::vector<Spawner> zone;
        for (int i = 0; i < 69; ++i) zone.push_back(spawnerOf(7400 + i, 13, 300.0 * i, 0));
        zone.push_back(spawnerOf(7499, 13, 0, 5000));
        enterZone(zone);
        runFrames(rotation());
        const bool keptUnborn = marked(7499);
        const size_t frames = rotation();
        world.spawners.back().spawned = true;
        runFrames(frames);
        check("state/born_retired_within_one_rotation", keptUnborn && !marked(7499) && pm().Count() == 69
            && st(PackMarkers::Champion).retired[PackMarkers::ReasonStateBorn] == 1,
            "frames=" + std::to_string(frames) + " count=" + std::to_string(pm().Count()));
    }
    // A destroyed spawner loses it.
    {
        enterZone({ spawnerOf(7500, 16, 100, 100), spawnerOf(7501, 16, 900, 900) });
        world.spawners[0].exists = false;
        runFrames(rotation());
        check("state/destroyed_retired", !marked(7500) && marked(7501) && st(PackMarkers::Miniboss).retired[PackMarkers::ReasonDestroyed] == 1,
            "count=" + std::to_string(pm().Count()));
    }
    // A create attributed to the spawner retires nothing while its enemyArray
    // is not an array (the create runs inside the birth, before the state
    // says born); the create is still counted.
    {
        enterZone({ spawnerOf(7600, 15, 100, 100) });
        const uint64_t spawnedBefore = pm().Spawned();
        pm().MarkSpawned(7600, 42, 15);
        runFrames(rotation());
        check("state/attributed_create_keeps_marker", marked(7600) && pm().Spawned() == spawnedBefore && st(PackMarkers::Legion).attributed == 1
            && st(PackMarkers::Legion).retired[PackMarkers::ReasonSpawned] == 0,
            "count=" + std::to_string(pm().Count()) + " spawned=" + std::to_string(pm().Spawned()));
        // ...and once the state does say born, the same spawner goes.
        world.spawners[0].spawned = true;
        runFrames(rotation());
        check("state/attributed_then_born_retired", !marked(7600));
    }
    // An absent enemyArray keeps the marker, born or not: only an array retires.
    {
        Spawner noArray = spawnerOf(7700, 11, 100, 100); noArray.arrayMode = Absent; noArray.spawned = true;
        Spawner odd = spawnerOf(7701, 11, 900, 900); odd.arrayMode = Odd; odd.spawned = true;
        enterZone({ noArray, odd });
        runFrames(rotation() + 2);
        check("state/absent_array_keeps_marker", marked(7700) && marked(7701), "count=" + std::to_string(pm().Count()));
    }
    // A revisited zone: its born spawners still exist with an array, and are
    // listed again by a fresh enumeration; one rotation retires them, the
    // unborn one stays, and nothing brings them back.
    {
        std::vector<Spawner> zone;
        for (int i = 0; i < 6; ++i) { Spawner s = spawnerOf(7800 + i, 12 + (i % 2), 200.0 * i, 0); s.spawned = true; zone.push_back(s); }
        zone.push_back(spawnerOf(7899, 12, 0, 4000));
        enterZone(zone);
        const size_t listed = pm().Count();
        runFrames(rotation());
        const bool retired = pm().Count() == 1 && marked(7899);
        const uint64_t enumerations = pm().Enumerations();
        runFrames(PackMarkers::kReenumerateFrames + PackMarkers::kCountPollFrames);   // past the periodic re-listing
        check("state/revisit_born_spawners_unmarked", listed == 7 && retired && pm().Enumerations() > enumerations
            && pm().Count() == 1 && marked(7899),
            "listed=" + std::to_string(listed) + " count=" + std::to_string(pm().Count()));
    }
    // Control: a normal spawner behaves the same way - an unborn one without a
    // usable timer is kept past the give-up window, a born one goes in one rotation.
    {
        Spawner unarmed = spawnerOf(7900, 10, 100, 100); unarmed.initialised = false;
        Spawner born = spawnerOf(7901, 10, 900, 900);
        enterZone({ unarmed, born });
        const uint64_t listedAt = clockFrame;
        world.spawners[1].spawned = true;
        runFrames(rotation());
        const bool bornGone = !marked(7901);
        while (clockFrame < listedAt + 2 * PackMarkers::kUnarmedGiveUpFrames) runFrames(1);
        check("state/normal_control", bornGone && marked(7900) && st(PackMarkers::Normal).retired[PackMarkers::ReasonStateBorn] == 1
            && st(PackMarkers::Normal).retired[PackMarkers::ReasonGivenUp] == 0, "count=" + std::to_string(pm().Count()));
    }
    pm().SetRetire(Retire::Timer);

    // ---- issue #181: the census --------------------------------------------
    // All seven kinds, every way a creator can carry its timer and its
    // enemyArray: number/array, undefined, absent, other.
    {
        std::vector<Spawner> zone;
        zone.push_back(spawnerOf(8000, 10, 0, 0));                                                    // normal: timer number, array undefined
        { Spawner s = spawnerOf(8001, 10, 100, 0); s.initialised = false; zone.push_back(s); }        // normal: timer undefined, array undefined
        { Spawner s = spawnerOf(8100, 11, 200, 0); s.timerMode = Absent; s.arrayMode = Absent; zone.push_back(s); }   // ambush: absent, absent
        { Spawner s = spawnerOf(8200, 12, 300, 0); s.timerMode = Odd; s.arrayMode = Odd; zone.push_back(s); }         // ancient: other, other
        { Spawner s = spawnerOf(8300, 13, 400, 0); s.spawned = true; zone.push_back(s); }             // champion: born - timer undefined, array an array
        { Spawner s = spawnerOf(8400, 14, 500, 0); s.initialised = false; zone.push_back(s); }        // colossal chest: undefined, undefined
        { Spawner s = spawnerOf(8500, 15, 600, 0); s.timerMode = Absent; zone.push_back(s); }         // legion: absent, undefined
        { Spawner s = spawnerOf(8501, 15, 700, 0); s.spawned = true; zone.push_back(s); }             // legion: born
        { Spawner s = spawnerOf(8600, 16, 800, 0); s.timerMode = Absent; zone.push_back(s); }         // miniboss: absent, undefined
        enterZone(zone);
        // Retire two markers by an attributed create (timer policy): 8001's
        // pack is not born (lost), 8501's is (neither lost nor stale).
        pm().MarkSpawned(8001, 42, 10);
        pm().MarkSpawned(8501, 42, 15);
        const long existsBefore = world.existsChecks;
        const auto rows = pm().Census();
        using T = PackMarkers::Tally;
        auto tallies = [](const unsigned* t) { return std::to_string(t[0]) + "/" + std::to_string(t[1]) + "/" + std::to_string(t[2]) + "/" + std::to_string(t[3]); };
        auto row = [&](int kind) {
            const auto& r = rows[kind];
            return std::string(PackMarkers::kKindNames[kind]) + ": creators=" + std::to_string(r.creators) + " marked=" + std::to_string(r.marked)
                + " timer=" + tallies(r.timer) + " enemyArray=" + tallies(r.enemyArray) + " lost=" + std::to_string(r.lost) + " stale=" + std::to_string(r.stale);
        };
        std::string all; for (int k = 0; k < PackMarkers::KindCount; ++k) all += " [" + row(k) + "]";
        check("census/creators_and_marked", rows[PackMarkers::Normal].creators == 2 && rows[PackMarkers::Normal].marked == 1
            && rows[PackMarkers::Legion].creators == 2 && rows[PackMarkers::Legion].marked == 1
            && rows[PackMarkers::Ambush].creators == 1 && rows[PackMarkers::Ambush].marked == 1 && rows[PackMarkers::Miniboss].creators == 1, all);
        check("census/timer_number", rows[PackMarkers::Normal].timer[T::TallyValue] == 1, all);
        check("census/timer_undefined", rows[PackMarkers::Normal].timer[T::TallyUndefined] == 1 && rows[PackMarkers::Champion].timer[T::TallyUndefined] == 1
            && rows[PackMarkers::ColossalChest].timer[T::TallyUndefined] == 1, all);
        check("census/timer_absent", rows[PackMarkers::Ambush].timer[T::TallyAbsent] == 1 && rows[PackMarkers::Legion].timer[T::TallyAbsent] == 1
            && rows[PackMarkers::Miniboss].timer[T::TallyAbsent] == 1 && rows[PackMarkers::Normal].timer[T::TallyAbsent] == 0, all);
        check("census/timer_other", rows[PackMarkers::Ancient].timer[T::TallyOther] == 1 && rows[PackMarkers::Ancient].timer[T::TallyValue] == 0, all);
        check("census/array_array", rows[PackMarkers::Champion].enemyArray[T::TallyValue] == 1 && rows[PackMarkers::Legion].enemyArray[T::TallyValue] == 1
            && rows[PackMarkers::Normal].enemyArray[T::TallyValue] == 0, all);
        check("census/array_undefined", rows[PackMarkers::Normal].enemyArray[T::TallyUndefined] == 2 && rows[PackMarkers::ColossalChest].enemyArray[T::TallyUndefined] == 1
            && rows[PackMarkers::Legion].enemyArray[T::TallyUndefined] == 1 && rows[PackMarkers::Miniboss].enemyArray[T::TallyUndefined] == 1, all);
        check("census/array_absent", rows[PackMarkers::Ambush].enemyArray[T::TallyAbsent] == 1 && rows[PackMarkers::Ambush].enemyArray[T::TallyUndefined] == 0, all);
        check("census/array_other", rows[PackMarkers::Ancient].enemyArray[T::TallyOther] == 1 && rows[PackMarkers::Ancient].enemyArray[T::TallyValue] == 0, all);
        // lost: not marked and no array (8001); a retired marker whose pack IS
        // born (8501) is not lost, and a marked unborn one (8000) is neither.
        check("census/lost_both_ways", rows[PackMarkers::Normal].lost == 1 && rows[PackMarkers::Legion].lost == 0
            && rows[PackMarkers::ColossalChest].lost == 0 && rows[PackMarkers::Ambush].lost == 0, all);
        // stale: marked and an array (8300, listed already born); the retired
        // born one (8501) and the marked unborn ones are not stale.
        check("census/stale_both_ways", rows[PackMarkers::Champion].stale == 1 && rows[PackMarkers::Legion].stale == 0
            && rows[PackMarkers::Normal].stale == 0 && rows[PackMarkers::Miniboss].stale == 0, all);
        // One-shot: the census asked variable_instance_exists twice per
        // creator; frames never ask it at all.
        const long censusAsks = world.existsChecks - existsBefore;
        runFrames(3 * PackMarkers::kCountPollFrames);
        check("census/never_per_frame", censusAsks == 2 * 9 && world.existsChecks - existsBefore == censusAsks,
            "asks=" + std::to_string(censusAsks) + " after=" + std::to_string(world.existsChecks - existsBefore));
    }

    // ---- issue #181: retirement accounting ----------------------------------
    // Each rule counts against its own reason and its own kind only.
    {
        Spawner timerless = spawnerOf(9200, 12, 300, 0); timerless.timerMode = Absent;
        enterZone({ spawnerOf(9000, 10, 0, 0), spawnerOf(9100, 11, 100, 0), timerless,
                    spawnerOf(9300, 13, 600, 0), spawnerOf(9500, 15, 900, 0), spawnerOf(9600, 16, 1200, 0) });
        const uint64_t listedAt = clockFrame;
        check("accounting/listed", st(PackMarkers::Normal).listed == 1 && st(PackMarkers::Legion).listed == 1
            && st(PackMarkers::ColossalChest).listed == 0, "normal=" + std::to_string(st(PackMarkers::Normal).listed));
        runFrames(rotation());   // arms every spawner that has a timer
        // Each step must add exactly one retirement, under its own kind and reason.
        using Counters = std::vector<uint64_t>;
        auto snapshot = [&]() {
            Counters c;
            for (int k = 0; k < PackMarkers::KindCount; ++k)
                for (int r = 0; r < PackMarkers::ReasonCount; ++r) c.push_back(st(k).retired[r]);
            return c;
        };
        auto onlyAdded = [&](const Counters& before, int kind, int reason) {
            const Counters after = snapshot();
            for (size_t i = 0; i < after.size(); ++i) {
                const uint64_t want = before[i] + ((int)i == kind * PackMarkers::ReasonCount + reason ? 1 : 0);
                if (after[i] != want) return false;
            }
            return true;
        };
        auto counts = [&]() {
            std::string s;
            for (int k = 0; k < PackMarkers::KindCount; ++k) {
                s += std::string(" ") + PackMarkers::kKindNames[k] + ":";
                for (int r = 0; r < PackMarkers::ReasonCount; ++r) s += std::to_string(st(k).retired[r]);
            }
            return s;
        };
        Counters before = snapshot();
        pm().MarkSpawned(9500, 42, 15);
        check("accounting/spawned", onlyAdded(before, PackMarkers::Legion, PackMarkers::ReasonSpawned), counts());
        before = snapshot();
        world.spawners[3].exists = false;
        runFrames(rotation());
        check("accounting/destroyed", onlyAdded(before, PackMarkers::Champion, PackMarkers::ReasonDestroyed), counts());
        before = snapshot();
        world.spawners[0].spawned = true;
        runFrames(rotation());
        check("accounting/timergone", onlyAdded(before, PackMarkers::Normal, PackMarkers::ReasonTimerGone), counts());
        before = snapshot();
        while (clockFrame < listedAt + PackMarkers::kUnarmedGiveUpFrames + 10) runFrames(1);
        check("accounting/givenup", onlyAdded(before, PackMarkers::Ancient, PackMarkers::ReasonGivenUp), counts());
        before = snapshot();
        pm().SetRetire(Retire::State);
        world.spawners[1].spawned = true;
        runFrames(rotation());
        pm().SetRetire(Retire::Timer);
        check("accounting/stateborn", onlyAdded(before, PackMarkers::Ambush, PackMarkers::ReasonStateBorn)
            && pm().Count() == 1 && marked(9600), counts());
        // Ages: the create came before any rotation, the give-up after the window.
        check("accounting/age_range", st(PackMarkers::Legion).ageMin >= 0 && st(PackMarkers::Legion).ageMax < (int64_t)PackMarkers::kUnarmedGiveUpFrames
            && st(PackMarkers::Ancient).ageMin >= (int64_t)PackMarkers::kUnarmedGiveUpFrames && st(PackMarkers::Ancient).ageMin == st(PackMarkers::Ancient).ageMax
            && st(PackMarkers::Miniboss).ageMin == -1 && st(PackMarkers::Miniboss).ageMax == -1,
            "legion=" + std::to_string(st(PackMarkers::Legion).ageMin) + ".." + std::to_string(st(PackMarkers::Legion).ageMax)
            + " ancient=" + std::to_string(st(PackMarkers::Ancient).ageMin) + ".." + std::to_string(st(PackMarkers::Ancient).ageMax));
        // Every create is counted against its creator's kind with the created
        // object, retiring or not: a spent creator's by the kind it was listed
        // under, an unlisted one's by its object index, else unattributed.
        pm().MarkSpawned(9500, 42, -1);
        pm().MarkSpawned(9500, 43, -1);
        pm().MarkSpawned(9500, 43, -1);
        pm().MarkSpawned(9500, 44, -1);
        pm().MarkSpawned(9500, 45, -1);
        pm().MarkSpawned(9500, 46, -1);
        pm().MarkSpawned(424242, 50, 16);
        pm().MarkSpawned(434343, 50, -1);
        const auto top = pm().TopCreates(PackMarkers::Legion);
        check("accounting/attributed_creates", st(PackMarkers::Legion).attributed == 7 && st(PackMarkers::Legion).creates.at(42) == 2
            && st(PackMarkers::Legion).creates.at(43) == 2 && st(PackMarkers::Miniboss).attributed == 1 && st(PackMarkers::Miniboss).creates.at(50) == 1
            && pm().UnattributedCreates() == 1 && st(PackMarkers::Legion).retired[PackMarkers::ReasonSpawned] == 1,
            "legion=" + std::to_string(st(PackMarkers::Legion).attributed) + " unattributed=" + std::to_string(pm().UnattributedCreates()));
        check("accounting/top_creates", top.size() == 4 && top[0].first == 42 && top[0].second == 2 && top[1].first == 43
            && top[2].first == 44 && top[3].first == 45, "top=" + std::to_string(top.size()));
        // markers held now, per kind
        const auto held = pm().KindCounts();
        check("stat/kinds_held_now", held[PackMarkers::Miniboss] == 1 && held[PackMarkers::Normal] == 0 && held[PackMarkers::Legion] == 0
            && std::string(PackMarkers::kKindNames[PackMarkers::ColossalChest]) == "colossal_chest", "miniboss=" + std::to_string(held[PackMarkers::Miniboss]));
        // A zone generation change starts every count again.
        enterZone({ spawnerOf(9700, 10, 0, 0) });
        bool zero = pm().UnattributedCreates() == 0;
        for (int k = 0; k < PackMarkers::KindCount; ++k) {
            const auto& s = st(k);
            for (int r = 0; r < PackMarkers::ReasonCount; ++r) zero = zero && s.retired[r] == 0;
            zero = zero && s.attributed == 0 && s.creates.empty() && s.ageMin == -1 && s.ageMax == -1 && s.listed == (k == PackMarkers::Normal ? 1u : 0u);
        }
        check("accounting/reset_on_zone_change", zero);
    }

    // ---- issue #181 replan 2: the `kind` policy ----------------------------
    // Ids from 11000 up and rooms from 1000 up: the birth memory lasts the
    // whole session, so no scenario below may meet an earlier one's spawner.
    {
        // The fake itself: a refused key that did reach the getter is counted
        // (the negative control for every "never reaches the getter" below).
        try { runner.CallGameScript(HeroSiege::Scripts::gml_Script_PC_GetVariableGMLWrapper, { RValue(-1.0) }); } catch (...) {}
        check("getter/fake_counts_a_refused_key", badKeyCalls == 1, "bad=" + std::to_string(badKeyCalls));
        badKeyCalls = 0;
    }
    using Rule = PackMarkers::KindRule;
    auto kindborn = [&](int kind) { return st(kind).retired[PackMarkers::ReasonKindBorn]; };
    auto packgone = [&](int kind) { return st(kind).retired[PackMarkers::ReasonPackGone]; };
    auto heldNow = [](int kind) { return pm().HeldNow()[kind]; };
    {
        auto is = [](int kind, PackMarkers::Signal signal, double threshold, PackMarkers::Mode mode) {
            const Rule& r = PackMarkers::kKindRules[kind];
            return r.signal == signal && r.threshold == threshold && r.mode == mode;
        };
        using S = PackMarkers::Signal; using M = PackMarkers::Mode;
        check("kind/one_rule_per_kind", is(PackMarkers::Normal, S::EnemyArray, 0, M::Birth) && is(PackMarkers::Ambush, S::EnemyArray, 0, M::Birth)
            && is(PackMarkers::Ancient, S::PackState, 2, M::Birth) && is(PackMarkers::ColossalChest, S::PackState, 2, M::Birth)
            && is(PackMarkers::Miniboss, S::PackState, 2, M::PackGone) && is(PackMarkers::Legion, S::PackState, 2, M::PackGone)
            && is(PackMarkers::Champion, S::PackState, 2, M::PackGone));
        pm().SetRetire(Retire::Kind);
        check("kind/selectable_at_runtime", pm().GetRetire() == Retire::Kind && std::string(PackMarkers::RetireName(Retire::Kind)) == "kind"
            && std::string(PackMarkers::RetireName(Retire::State)) == "state" && std::string(PackMarkers::RetireName(Retire::Timer)) == "timer");
    }
    // A spawner with no timer, no enemyArray and state 1 keeps its marker
    // past the give-up window; state 2 retires it within one rotation.
    {
        std::vector<Spawner> zone;
        for (int i = 0; i < 40; ++i) zone.push_back(special(11000 + i, 12, 300.0 * i, 0, RValue(1.0)));
        zone.push_back(special(11099, 12, 0, 5000, RValue(1.0)));
        enterRoom(1000, zone);
        const uint64_t listedAt = clockFrame;
        while (clockFrame < listedAt + 2 * PackMarkers::kUnarmedGiveUpFrames) runFrames(1);
        check("kind/unborn_state_kept_past_give_up", marked(11099) && pm().Count() == 41 && st(PackMarkers::Ancient).retired[PackMarkers::ReasonGivenUp] == 0
            && kindborn(PackMarkers::Ancient) == 0, "count=" + std::to_string(pm().Count()));
        const size_t frames = rotation();
        setState(11099, RValue(2.0));
        runFrames(frames);
        check("kind/state_2_retires_within_one_rotation", !marked(11099) && pm().Count() == 40 && kindborn(PackMarkers::Ancient) == 1,
            "frames=" + std::to_string(frames) + " count=" + std::to_string(pm().Count()));
    }
    // ...and state 3 likewise (a colossal chest spawner).
    {
        std::vector<Spawner> zone;
        for (int i = 0; i < 40; ++i) zone.push_back(special(11100 + i, 14, 300.0 * i, 0, RValue(1.0)));
        zone.push_back(special(11199, 14, 0, 5000, RValue(1.0)));
        enterRoom(1001, zone);
        runFrames(rotation());
        const bool kept = marked(11199);
        const size_t frames = rotation();
        setState(11199, RValue(3.0));
        runFrames(frames);
        check("kind/state_3_retires_within_one_rotation", kept && !marked(11199) && kindborn(PackMarkers::ColossalChest) == 1,
            "count=" + std::to_string(pm().Count()));
    }
    // A spawner whose state reads 2 when it is listed gets no marker: listed,
    // and retired as kindborn at age 0.
    {
        enterRoom(1002, { special(11200, 12, 100, 100, RValue(2.0)), special(11201, 12, 900, 900, RValue(1.0)) });
        const auto& a = st(PackMarkers::Ancient);
        check("kind/born_when_listed_gets_no_marker", !marked(11200) && marked(11201) && a.listed == 2 && kindborn(PackMarkers::Ancient) == 1
            && a.ageMin == 0 && a.ageMax == 0, "listed=" + std::to_string(a.listed) + " kindborn=" + std::to_string(kindborn(PackMarkers::Ancient)));
    }
    // A key of -1, 262144 or 1.5, a string key and an absent spawnPack keep
    // the marker, count unread and never reach the getter; an integer-kind
    // key in range does reach it (positive control).
    {
        auto base = [](int64_t id, double x) { Spawner s = spawnerOf(id, 12, x, 0); s.timerMode = Absent; s.arrayMode = Absent; return s; };
        RValue intKey((double)nextKey); intKey.m_Kind = VALUE_INT64; store[nextKey++] = RValue(1.0);
        const long getterBefore = getterCalls;
        const uint64_t unreadBefore = pm().Unread();
        enterRoom(1003, { keyed(base(11300, 0), RValue(-1.0)), keyed(base(11301, 300), RValue(262144.0)), keyed(base(11302, 600), RValue(1.5)),
                          keyed(base(11303, 900), RValue("17")), base(11304, 1200), keyed(base(11305, 1500), intKey) });
        runFrames(3 * rotation());
        const uint64_t unread = pm().Unread() - unreadBefore;
        bool allMarked = true; for (int64_t id = 11300; id <= 11305; ++id) allMarked = allMarked && marked(id);
        // five refused reads at listing, five more per rotation
        check("kind/refused_keys_keep_the_marker_and_count_unread", allMarked && unread >= 5 * 4 && unread % 5 == 0,
            "unread=" + std::to_string(unread) + " count=" + std::to_string(pm().Count()));
        check("kind/refused_keys_never_reach_the_getter", badKeyCalls == 0 && getterCalls - getterBefore == 4,
            "bad=" + std::to_string(badKeyCalls) + " getter=" + std::to_string(getterCalls - getterBefore));
    }
    // An attributed create retires nothing; it is counted for its spawner.
    {
        enterRoom(1004, { special(11400, 12, 100, 100, RValue(1.0)) });
        const uint64_t spawnedBefore = pm().Spawned();
        pm().MarkSpawned(11400, 42, 12);
        runFrames(rotation());
        const auto mine = pm().CreatesOf(11400);
        check("kind/attributed_create_retires_nothing", marked(11400) && st(PackMarkers::Ancient).retired[PackMarkers::ReasonSpawned] == 0
            && st(PackMarkers::Ancient).attributed == 1 && mine.attributed == 1 && mine.objects.count(42) && mine.objects.at(42) == 1 && pm().Spawned() == spawnedBefore,
            "attributed=" + std::to_string(mine.attributed));
    }
    // Normal and ambush spawners retire on enemyArray exactly as under
    // `state`; an absent enemyArray keeps the marker and counts unread, a
    // numeric one keeps it as unborn.
    {
        Spawner absentArray = spawnerOf(11502, 11, 1800, 1800); absentArray.arrayMode = Absent; absentArray.spawned = true;
        Spawner oddArray = spawnerOf(11503, 10, 2700, 0); oddArray.arrayMode = Odd; oddArray.spawned = true;
        Spawner unarmed = spawnerOf(11504, 10, 3600, 0); unarmed.initialised = false;
        enterRoom(1005, { spawnerOf(11500, 10, 100, 100), spawnerOf(11501, 11, 900, 900), absentArray, oddArray, unarmed });
        const uint64_t listedAt = clockFrame;
        const uint64_t unreadBefore = pm().Unread();
        runFrames(rotation());
        const uint64_t unreadOneRotation = pm().Unread() - unreadBefore;
        const bool keptUnborn = marked(11500) && marked(11501);
        world.spawners[0].spawned = true; world.spawners[1].spawned = true;
        runFrames(rotation());
        while (clockFrame < listedAt + 2 * PackMarkers::kUnarmedGiveUpFrames) runFrames(1);
        check("kind/normal_and_ambush_retire_on_enemy_array", keptUnborn && !marked(11500) && !marked(11501) && kindborn(PackMarkers::Normal) == 1
            && kindborn(PackMarkers::Ambush) == 1 && marked(11503) && marked(11504) && st(PackMarkers::Normal).retired[PackMarkers::ReasonGivenUp] == 0,
            "count=" + std::to_string(pm().Count()));
        check("kind/absent_enemy_array_keeps_the_marker_and_counts_unread", marked(11502) && unreadOneRotation == 1,
            "unread=" + std::to_string(unreadOneRotation));
    }
    // Under `state`, an absent enemyArray is not unread: only a read that throws is.
    {
        pm().SetRetire(Retire::State);
        Spawner absentArray = spawnerOf(11510, 11, 100, 100); absentArray.arrayMode = Absent;
        enterRoom(1006, { absentArray });
        const uint64_t unreadBefore = pm().Unread();
        runFrames(3 * rotation());
        check("state/absent_enemy_array_is_not_unread", marked(11510) && pm().Unread() == unreadBefore);
        pm().SetRetire(Retire::Kind);
    }
    // A destroyed spawner retires.
    {
        enterRoom(1007, { special(11600, 12, 100, 100, RValue(1.0)), special(11601, 12, 900, 900, RValue(1.0)) });
        world.spawners[0].exists = false;
        runFrames(rotation());
        check("kind/destroyed_retires", !marked(11600) && marked(11601) && st(PackMarkers::Ancient).retired[PackMarkers::ReasonDestroyed] == 1);
    }

    // ---- the birth memory ----------------------------------------------------
    // After a zone change a remembered id gets no marker (here unborn again,
    // in another room and place, so only the id can match).
    {
        enterRoom(1100, { spawnerOf(11700, 10, 100, 100) });
        world.spawners[0].spawned = true;
        runFrames(rotation());
        const bool retired = !marked(11700) && kindborn(PackMarkers::Normal) == 1;
        enterRoom(1101, { spawnerOf(11700, 10, 5000, 5000), spawnerOf(11701, 10, 100, 100) });
        check("memory/remembered_id_gets_no_marker", retired && !marked(11700) && marked(11701) && st(PackMarkers::Normal).remembered == 1
            && st(PackMarkers::Normal).listed == 1, "remembered=" + std::to_string(st(PackMarkers::Normal).remembered));
    }
    // A new id at a remembered room, kind and position gets no marker; another
    // kind at that position does.
    {
        enterRoom(1102, { special(11800, 12, 500, 500, RValue(1.0)) });
        setState(11800, RValue(2.0));
        runFrames(rotation());
        const bool retired = !marked(11800);
        enterRoom(1102, { special(11801, 12, 500.4, 499.6, RValue(1.0)), special(11802, 14, 500, 500, RValue(1.0)) });
        check("memory/new_id_at_a_remembered_position_gets_no_marker", retired && !marked(11801) && marked(11802)
            && st(PackMarkers::Ancient).remembered == 1, "count=" + std::to_string(pm().Count()));
        // Negative control: the same position in another room gets one.
        enterRoom(1103, { special(11803, 12, 500, 500, RValue(1.0)) });
        check("memory/same_position_in_another_room_is_marked", marked(11803) && st(PackMarkers::Ancient).remembered == 0);
    }
    // An unknown room key records and matches nothing by position.
    {
        const size_t positionsBefore = pm().MemoryPositions();
        enterRoom(PackMarkers::kUnknownRoom, { special(11900, 12, 700, 700, RValue(2.0)) });   // born at listing, room unknown
        const bool recordedNothing = pm().MemoryPositions() == positionsBefore && kindborn(PackMarkers::Ancient) == 1;
        enterRoom(PackMarkers::kUnknownRoom, { special(11901, 12, 700, 700, RValue(1.0)) });
        const bool unknownMatchesUnknown = !marked(11901);
        enterRoom(1104, { special(11902, 12, 800, 800, RValue(2.0)) });                     // remembered in a known room
        const bool recordedKnown = pm().MemoryPositions() == positionsBefore + 1;
        enterRoom(PackMarkers::kUnknownRoom, { special(11903, 12, 800, 800, RValue(1.0)) });
        check("memory/unknown_room_matches_nothing_by_position", recordedNothing && !unknownMatchesUnknown && recordedKnown && marked(11903),
            "positions=" + std::to_string(pm().MemoryPositions() - positionsBefore));
    }
    // A spawner still unborn when the zone is left keeps its marker on the
    // return (positive control), and `sameid` counts the ids listed in an
    // earlier visit to the same room.
    {
        enterRoom(1105, { special(12000, 12, 100, 100, RValue(1.0)), special(12001, 12, 900, 900, RValue(1.0)) });
        setState(12001, RValue(2.0));
        runFrames(rotation());
        const bool firstVisit = marked(12000) && !marked(12001) && st(PackMarkers::Ancient).sameid == 0;
        enterRoom(1106, { special(12000, 12, 100, 100, RValue(1.0)), special(12002, 12, 900, 900, RValue(1.0)) });   // the same id elsewhere
        const bool elsewhere = st(PackMarkers::Ancient).sameid == 0;
        enterRoom(1105, { special(12000, 12, 100, 100, RValue(1.0)), special(12001, 12, 900, 900, RValue(1.0)), special(12003, 12, 1800, 0, RValue(1.0)) });
        check("memory/unborn_spawner_is_marked_again_on_return", firstVisit && marked(12000) && !marked(12001) && marked(12003),
            "count=" + std::to_string(pm().Count()));
        check("memory/sameid_counts_ids_from_an_earlier_visit", elsewhere && st(PackMarkers::Ancient).sameid == 2,
            "sameid=" + std::to_string(st(PackMarkers::Ancient).sameid));
    }
    // Under `timer` and `state` the memory is recorded but never withholds a
    // marker; under `kind` it then does.
    {
        pm().SetRetire(Retire::State);
        enterRoom(1107, { spawnerOf(12100, 10, 100, 100) });
        world.spawners[0].spawned = true;
        runFrames(rotation());
        const bool stateRetired = !marked(12100) && st(PackMarkers::Normal).retired[PackMarkers::ReasonStateBorn] == 1;
        enterRoom(1107, { spawnerOf(12100, 10, 100, 100) });
        const bool stateMarks = marked(12100);
        pm().SetRetire(Retire::Timer);
        enterRoom(1108, { spawnerOf(12101, 10, 100, 100) });
        pm().MarkSpawned(12101, 42, 10);
        const bool timerRetired = !marked(12101);
        enterRoom(1108, { spawnerOf(12101, 10, 100, 100) });
        const bool timerMarks = marked(12101);
        check("memory/recorded_but_never_withholding_under_timer_and_state", stateRetired && stateMarks && timerRetired && timerMarks);
        pm().SetRetire(Retire::Kind);
        enterRoom(1109, { spawnerOf(12100, 10, 100, 100), spawnerOf(12101, 10, 900, 900) });
        check("memory/recorded_under_timer_and_state_applies_under_kind", !marked(12100) && !marked(12101) && st(PackMarkers::Normal).remembered == 2,
            "remembered=" + std::to_string(st(PackMarkers::Normal).remembered));
    }

    // ---- `packgone` mode: miniboss, legion, champion ---------------------------
    // Born when listed: a marker, held; with no member recorded it stays,
    // counted unlinked.
    {
        enterRoom(1200, { special(12200, 16, 100, 100, RValue(2.0)) });
        const auto held = heldNow(PackMarkers::Miniboss);
        const bool listed = marked(12200) && held.held == 1 && kindborn(PackMarkers::Miniboss) == 0;
        check("packgone/born_when_listed_gets_a_marker_held", listed, "held=" + std::to_string(held.held));
        runFrames(4 * rotation() + 10);
        check("packgone/no_member_recorded_stays_unlinked", marked(12200) && heldNow(PackMarkers::Miniboss).unlinked == 1
            && packgone(PackMarkers::Miniboss) == 0);
    }
    // Its state turning 2 retires nothing.
    {
        enterRoom(1201, { special(12300, 16, 100, 100, RValue(1.0)) });
        runFrames(rotation());
        const bool unbornNotHeld = marked(12300) && heldNow(PackMarkers::Miniboss).held == 0;
        setState(12300, RValue(2.0));
        runFrames(3 * rotation());
        check("packgone/state_turning_born_retires_nothing", unbornNotHeld && marked(12300) && heldNow(PackMarkers::Miniboss).held == 1
            && kindborn(PackMarkers::Miniboss) == 0);
    }
    // With members recorded, the marker stays while one member exists and
    // retires as packgone within one rotation once none does.
    {
        std::vector<Spawner> zone;
        for (int i = 0; i < 40; ++i) zone.push_back(special(12400 + i, 16, 300.0 * i, 0, RValue(1.0)));
        zone.push_back(special(12499, 16, 0, 5000, RValue(2.0)));
        monsters[60000] = true; monsters[60001] = true;
        enterRoom(1202, zone);
        pm().NoteMember(12499, 60000, 77);
        pm().NoteMember(12499, 60001, 78);
        runFrames(rotation());
        const bool linked = marked(12499) && heldNow(PackMarkers::Miniboss).held == 1 && heldNow(PackMarkers::Miniboss).unlinked == 0;
        monsters[60000] = false;
        runFrames(2 * rotation());
        const bool oneAlive = marked(12499) && packgone(PackMarkers::Miniboss) == 0;
        monsters[60001] = false;
        const size_t frames = rotation();
        runFrames(frames);
        check("packgone/members_hold_the_marker_until_none_exists", linked && oneAlive && !marked(12499) && packgone(PackMarkers::Miniboss) == 1
            && pm().Count() == 40, "frames=" + std::to_string(frames) + " count=" + std::to_string(pm().Count()));
    }
    // Members recorded before a zone change (the per-zone reset), or before
    // the spawner was listed at all, still count after it.
    {
        monsters[60100] = true; monsters[60101] = true;
        enterRoom(1203, { special(12500, 16, 100, 100, RValue(2.0)) });
        pm().NoteMember(12500, 60100, 77);
        pm().NoteMember(12501, 60101, 77);   // a pack built before its spawner was listed
        enterRoom(1203, { special(12500, 16, 100, 100, RValue(2.0)), special(12501, 16, 900, 900, RValue(2.0)) });
        const bool kept = marked(12500) && marked(12501) && heldNow(PackMarkers::Miniboss).unlinked == 0;
        monsters[60100] = false; monsters[60101] = false;
        runFrames(rotation());
        check("packgone/members_recorded_before_a_zone_change_still_count", kept && !marked(12500) && !marked(12501)
            && packgone(PackMarkers::Miniboss) == 2, "packgone=" + std::to_string(packgone(PackMarkers::Miniboss)));
    }
    // A destroyed miniboss spawner retires as destroyed, its pack alive or not.
    {
        monsters[60200] = true;
        enterRoom(1204, { special(12600, 16, 100, 100, RValue(2.0)) });
        pm().NoteMember(12600, 60200, 77);
        world.spawners[0].exists = false;
        runFrames(rotation());
        check("packgone/destroyed_spawner_retires_as_destroyed", !marked(12600) && st(PackMarkers::Miniboss).retired[PackMarkers::ReasonDestroyed] == 1
            && packgone(PackMarkers::Miniboss) == 0);
    }
    // Legion and champion behave the same.
    {
        monsters[60300] = true; monsters[60301] = true;
        enterRoom(1205, { special(12700, 15, 100, 100, RValue(2.0)), special(12701, 13, 900, 900, RValue(2.0)), special(12702, 15, 1800, 0, RValue(2.0)) });
        pm().NoteMember(12700, 60300, 77);
        pm().NoteMember(12701, 60301, 78);
        runFrames(rotation());
        const bool held = marked(12700) && marked(12701) && heldNow(PackMarkers::Legion).held == 2 && heldNow(PackMarkers::Legion).unlinked == 1
            && heldNow(PackMarkers::Champion).held == 1 && kindborn(PackMarkers::Legion) == 0 && kindborn(PackMarkers::Champion) == 0;
        monsters[60300] = false; monsters[60301] = false;
        runFrames(rotation());
        check("packgone/legion_and_champion_behave_the_same", held && !marked(12700) && !marked(12701) && marked(12702)
            && packgone(PackMarkers::Legion) == 1 && packgone(PackMarkers::Champion) == 1);
    }
    // A packgone spawner is remembered only once retired: a living pack's
    // spawner is marked again after a zone change, a gone one is not.
    {
        monsters[60400] = true; monsters[60401] = false;
        enterRoom(1206, { special(12800, 16, 100, 100, RValue(2.0)), special(12801, 16, 900, 900, RValue(2.0)) });
        pm().NoteMember(12800, 60400, 77);
        pm().NoteMember(12801, 60401, 77);
        runFrames(rotation());
        const bool first = marked(12800) && !marked(12801) && packgone(PackMarkers::Miniboss) == 1;
        enterRoom(1206, { special(12800, 16, 100, 100, RValue(2.0)), special(12801, 16, 900, 900, RValue(2.0)) });
        check("packgone/remembered_only_once_its_pack_is_gone", first && marked(12800) && !marked(12801) && st(PackMarkers::Miniboss).remembered == 1);
    }
    // Baseline: members never hold a `birth`-mode marker - an ancient
    // spawner turning 2 retires as kindborn with its members alive.
    {
        monsters[60500] = true;
        enterRoom(1207, { special(12900, 12, 100, 100, RValue(1.0)) });
        pm().NoteMember(12900, 60500, 77);
        setState(12900, RValue(2.0));
        runFrames(rotation());
        check("packgone/birth_mode_ignores_living_members", !marked(12900) && kindborn(PackMarkers::Ancient) == 1 && packgone(PackMarkers::Ancient) == 0);
    }
    // Baseline: under `timer` and `state`, members change nothing.
    {
        pm().SetRetire(Retire::State);
        monsters[60600] = false;
        enterRoom(1208, { special(13000, 16, 100, 100, RValue(2.0)) });
        pm().NoteMember(13000, 60600, 77);
        runFrames(3 * rotation());
        const bool stateKeeps = marked(13000) && packgone(PackMarkers::Miniboss) == 0;
        pm().SetRetire(Retire::Timer);
        monsters[60601] = false;
        enterRoom(1209, { packed(spawnerOf(13001, 16, 100, 100), RValue(2.0)) });   // timer armed: the timer policy keeps it
        pm().NoteMember(13001, 60601, 77);
        runFrames(3 * rotation());
        check("packgone/members_change_nothing_under_timer_and_state", stateKeeps && marked(13001) && packgone(PackMarkers::Miniboss) == 0);
        pm().SetRetire(Retire::Kind);
    }

    // ---- the protected-state census -------------------------------------------
    {
        std::vector<Spawner> zone;
        zone.push_back(packed(spawnerOf(13100, 10, 0, 0), RValue(1.0)));                                     // normal: unborn
        { Spawner s = packed(spawnerOf(13101, 10, 100, 0), RValue(1.0)); s.spawned = true; zone.push_back(s); }   // normal: born (array)
        { Spawner s = spawnerOf(13110, 11, 200, 0); s.arrayMode = Absent; zone.push_back(s); }             // ambush: no spawnPack
        zone.push_back(special(13120, 12, 300, 0, RValue()));                                                 // ancient: unset record
        { Spawner s = spawnerOf(13121, 12, 400, 0); s.arrayMode = Absent; zone.push_back(keyed(s, RValue(-1.0))); }   // ancient: refused key
        zone.push_back(special(13122, 12, 500, 0, RValue(2.0)));                                              // ancient: born
        zone.push_back(special(13130, 13, 600, 0, RValue(3.0)));                                              // champion: born
        zone.push_back(special(13140, 14, 700, 0, RValue("open")));                                           // colossal chest: other
        zone.push_back(special(13141, 14, 800, 0, RValue(1.5)));                                              // colossal chest: other, unborn
        zone.push_back(special(13150, 15, 900, 0, RValue(2.0)));                                              // legion: born
        zone.push_back(special(13151, 15, 1000, 0, RValue(1.0)));                                             // legion: unborn
        { Spawner s = spawnerOf(13160, 16, 1100, 0); s.arrayMode = Absent; zone.push_back(keyed(s, RValue("k"))); }   // miniboss: string key
        zone.push_back(special(13161, 16, 1200, 0, RValue(1.0)));                                             // miniboss: unborn
        enterRoom(1300, zone);
        pm().MarkSpawned(13100, 42, 10);
        pm().MarkSpawned(13101, 42, 10);
        pm().MarkSpawned(13161, 43, 16);
        monsters[61000] = true; monsters[61001] = false;
        pm().NoteMember(13150, 61000, 77);
        pm().NoteMember(13150, 61001, 78);
        pm().NoteMember(13150, 61001, 78);   // recorded once
        const long getterBefore = getterCalls, badBefore = badKeyCalls;
        const auto plain = pm().Census();
        const bool plainAsksNoGetter = getterCalls == getterBefore && plain[PackMarkers::Normal].born == 0
            && PackMarkers::SpawnPackTally(plain[PackMarkers::Normal]) == "none";
        const auto rows = pm().Census(true);
        std::string all;
        for (int k = 0; k < PackMarkers::KindCount; ++k)
            all += std::string(" ") + PackMarkers::kKindNames[k] + " born=" + std::to_string(rows[k].born) + " attributedUnborn=" + std::to_string(rows[k].attributedUnborn)
                + " spawnPack=" + PackMarkers::SpawnPackTally(rows[k]);
        auto tally = [&](int k) { return PackMarkers::SpawnPackTally(rows[k]); };
        check("census2/spawnpack_tally_on_all_seven_kinds", tally(PackMarkers::Normal) == "1:2" && tally(PackMarkers::Ambush) == "absent:1"
            && tally(PackMarkers::Ancient) == "2:1,undefined:1,unreadable:1" && tally(PackMarkers::Champion) == "3:1"
            && tally(PackMarkers::ColossalChest) == "other:2" && tally(PackMarkers::Legion) == "1:1,2:1" && tally(PackMarkers::Miniboss) == "1:1,unreadable:1"
            && PackMarkers::SpawnPackTally(PackMarkers::CensusRow{}) == "none" && badKeyCalls == badBefore, all);
        check("census2/born_both_ways", rows[PackMarkers::Normal].born == 1 && rows[PackMarkers::Ambush].born == 0 && rows[PackMarkers::Ancient].born == 1
            && rows[PackMarkers::Champion].born == 1 && rows[PackMarkers::ColossalChest].born == 0 && rows[PackMarkers::Legion].born == 1
            && rows[PackMarkers::Miniboss].born == 0, all);
        check("census2/attributed_unborn_both_ways", rows[PackMarkers::Normal].attributedUnborn == 1 && rows[PackMarkers::Miniboss].attributedUnborn == 1
            && rows[PackMarkers::Legion].attributedUnborn == 0 && rows[PackMarkers::Ancient].attributedUnborn == 0, all);
        check("census2/pack_state_read_only_when_asked", plainAsksNoGetter, "getter=" + std::to_string(getterCalls - getterBefore));
        // The per-spawner listing for one kind.
        const auto legion = pm().CensusList(PackMarkers::Legion);
        const auto normal = pm().CensusList(PackMarkers::Normal);
        const auto ancient = pm().CensusList(PackMarkers::Ancient);
        auto find = [](const std::vector<PackMarkers::CensusEntry>& list, int64_t id) -> const PackMarkers::CensusEntry* {
            for (const auto& e : list) if (e.id == id) return &e;
            return nullptr;
        };
        const auto* l0 = find(legion, 13150); const auto* l1 = find(legion, 13151);
        const auto* n0 = find(normal, 13100); const auto* n1 = find(normal, 13101);
        const auto* a0 = find(ancient, 13120); const auto* a1 = find(ancient, 13121);
        check("census2/members_both_ways", l0 && l1 && l0->membersAlive == 1 && l0->membersRecorded == 2 && l1->membersAlive == 0 && l1->membersRecorded == 0,
            l0 ? "members=" + std::to_string(l0->membersAlive) + "/" + std::to_string(l0->membersRecorded) : "missing");
        check("census2/per_spawner_listing", legion.size() == 2 && normal.size() == 2 && l0 && l1 && n0 && n1 && a0 && a1
            && l0->marked && l0->born && PackMarkers::PackText(l0->pack) == "2" && std::string(PackMarkers::kArrayTallyNames[l0->enemyArray]) == "absent"
            && l0->x == 900 && l0->y == 0 && !l1->born && PackMarkers::PackText(l1->pack) == "1"
            && !n1->marked && n1->born && n1->attributed == 1 && std::string(PackMarkers::kArrayTallyNames[n1->enemyArray]) == "array"
            && n0->marked && !n0->born && n0->attributed == 1 && std::string(PackMarkers::kArrayTallyNames[n0->enemyArray]) == "undefined"
            && PackMarkers::PackText(a0->pack) == "undefined" && PackMarkers::PackText(a1->pack) == "unreadable" && badKeyCalls == badBefore);
        // A creator's members by object, read now; a duplicate is recorded once.
        const auto members = pm().Members(13150);
        check("creator/members_by_object", members.alive == 1 && members.recorded == 2 && members.objects.size() == 2
            && members.objects[0].recorded == 1 && pm().Members(99999).recorded == 0,
            "alive=" + std::to_string(members.alive) + " recorded=" + std::to_string(members.recorded));
        // The non-enemy creates are counted per kind and retire nothing.
        pm().NoteOtherCreate(13100, 900, 10);
        pm().NoteOtherCreate(13100, 900, 10);
        pm().NoteOtherCreate(13100, 900, 10);
        pm().NoteOtherCreate(13100, 901, 10);
        pm().NoteOtherCreate(424242, 902, 16);
        pm().NoteOtherCreate(434343, 903, -1);
        const auto other = pm().TopOtherCreates(PackMarkers::Normal);
        check("other/non_enemy_creates_counted_per_kind", other.size() == 2 && other[0].first == 900 && other[0].second == 3 && other[1].first == 901
            && pm().TopOtherCreates(PackMarkers::Miniboss).size() == 1 && pm().UnattributedOther() == 1 && st(PackMarkers::Normal).attributed == 2
            && marked(13100), "other=" + std::to_string(other.size()));
        // The unread count lasts the game session; the zone's counts do not.
        const uint64_t unread = pm().Unread();
        enterRoom(1301, { spawnerOf(13200, 10, 0, 0) });
        check("stat/unread_survives_a_zone_change", unread > 0 && pm().Unread() >= unread && pm().TopOtherCreates(PackMarkers::Normal).empty()
            && pm().UnattributedOther() == 0 && pm().CreatesOf(13100).attributed == 0, "unread=" + std::to_string(unread));
    }
    // Frames under `timer` and `state` never call the getter.
    {
        const long getterBefore = getterCalls;
        pm().SetRetire(Retire::Timer);
        enterRoom(1302, { special(13300, 12, 0, 0, RValue(1.0)), special(13301, 16, 900, 0, RValue(2.0)) });
        runFrames(3 * rotation());
        pm().SetRetire(Retire::State);
        enterRoom(1303, { special(13302, 12, 0, 0, RValue(1.0)) });
        runFrames(3 * rotation());
        check("kind/getter_only_under_kind", getterCalls == getterBefore, "getter=" + std::to_string(getterCalls - getterBefore));
        pm().SetRetire(Retire::Kind);
    }
    // The memories stay bounded: 70000 packs born before listing, and as
    // many members' creators.
    {
        std::vector<Spawner> zone;
        zone.reserve(70000);
        for (int i = 0; i < 70000; ++i) { Spawner s = spawnerOf(200000 + i, 10, 3.0 * i, 7.0); s.spawned = true; zone.push_back(s); }
        enterRoom(1400, zone);
        for (int i = 0; i < 70000; ++i) pm().NoteMember(300000 + i, 400000 + i, 77);
        for (int i = 0; i < 100; ++i) pm().NoteMember(500000, 600000 + i, 77);
        check("memory/stays_bounded", pm().MemoryIds() <= PackMarkers::kMemoryCap && pm().MemoryPositions() <= PackMarkers::kMemoryCap
            && pm().MemoryIds() > 0 && pm().Count() == 0 && kindborn(PackMarkers::Normal) == 70000 && pm().MemberCreators() <= PackMarkers::kMemoryCap
            && pm().Members(500000).recorded == PackMarkers::kMembersPerCreator,
            "ids=" + std::to_string(pm().MemoryIds()) + " positions=" + std::to_string(pm().MemoryPositions()) + " creators=" + std::to_string(pm().MemberCreators()));
    }
    check("getter/no_refused_key_ever_reached_it", badKeyCalls == 0, "bad=" + std::to_string(badKeyCalls));
    pm().SetRetire(Retire::Timer);
    roomNow = PackMarkers::kUnknownRoom;

    // Off again: the list is gone and nothing is drawn.
    pm().SetEnabled(false);
    world.draws = 0;
    pm().Draw(sx, sy, off, sprite);
    check("off/nothing_drawn", pm().Count() == 0 && world.draws == 0);

    std::cout << (failures ? "RESULT FAILED " + std::to_string(failures) : std::string("RESULT OK")) << "\n";
    return failures ? 1 : 0;
}
