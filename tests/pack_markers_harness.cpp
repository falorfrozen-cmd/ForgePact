// Behavioral regression harness for the pack markers (map reveal's monster
// half since 1.4.5).
//
// The Python runner injects the REAL ForgePact::PackMarkers class below. Only
// the game API is replaced; no game process is touched. The controlled world
// holds spawners with an `enemyCreatorTimer` that is undefined until the
// spawner initialises and undefined again once it has given birth, which is
// exactly what the live creator does (docs/population-performance-analysis.md).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
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
};

// ---- the controlled world -------------------------------------------------
struct Spawner { int64_t id; int kind; double x, y; bool exists; bool initialised; bool spawned; };
struct World {
    std::vector<Spawner> spawners;
    std::map<std::string, int> objects;   // creator family name -> object index
    double hudRes = 2.0;
    long finds = 0, reads = 0, numbers = 0, exists = 0, draws = 0, rings = 0, texts = 0, spriteAdds = 0, spriteDeletes = 0;
    long spriteAddResult = 500;           // sprite_add answers 500+n, or -1 when negative
    bool refuseRelative = false;          // the sandbox refuses the relative path: only the absolute one works
    std::vector<std::string> addedPaths;
    std::vector<std::string> drawn;       // "subimg,x,y,scale,colour,alpha" per sprite draw
    double drawAlpha = 1.0, drawColour = 16777215.0, drawFont = 0.0;   // the runner's global draw state
};
static World world;
static int familyIndex(const std::string& name) { auto it = world.objects.find(name); return it == world.objects.end() ? -1 : it->second; }
static Spawner* byId(int64_t id) { for (auto& s : world.spawners) if (s.id == id && s.exists) return &s; return nullptr; }

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
            const int obj = (int)args[0].number; int remaining = (int)args[1].number;
            for (const auto& s : world.spawners) if (s.exists && s.kind == obj && remaining-- == 0) { RValue v((double)s.id); v.m_Kind = VALUE_REF; return v; }
            return RValue(-4.0);
        }
        if (name == "instance_exists") { ++world.exists; return RValue(byId((int64_t)args[0].number) ? 1.0 : 0.0); }
        if (name == "variable_instance_get") {
            ++world.reads;
            Spawner* s = byId((int64_t)args[0].number);
            if (!s) throw std::runtime_error("no such instance");
            const std::string& field = args[1].text;
            if (field == "x") return RValue(s->x);
            if (field == "y") return RValue(s->y);
            if (field == "id") return RValue((double)s->id);
            if (field == "enemyCreatorTimer") { if (s->initialised && !s->spawned) return RValue(116.0); return RValue(); }
            throw std::runtime_error("unexpected field " + field);
        }
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
        if (name == "draw_get_font") return RValue(world.drawFont);
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

int main() {
    resetWorld();
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

    // Off again: the list is gone and nothing is drawn.
    pm().SetEnabled(false);
    world.draws = 0;
    pm().Draw(sx, sy, off, sprite);
    check("off/nothing_drawn", pm().Count() == 0 && world.draws == 0);

    std::cout << (failures ? "RESULT FAILED " + std::to_string(failures) : std::string("RESULT OK")) << "\n";
    return failures ? 1 : 0;
}
