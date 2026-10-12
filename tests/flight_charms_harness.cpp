// Behavioral regression harness for the flight charms' decision core
// (FlightCharms.hpp, `flightcharm`, ForgePact #158 phase 2).
//
// The Python runner injects the REAL ForgePact::FlightCharms header below. No
// game is touched. The controlled world is an object table with parents
// (object_is_ancestor answers from it the way the runtime does - an object is
// not its own ancestor), the charms the local player wears, whether the
// player overlaps the Collision_Parent_obj family, a room size, each room's
// two game-layer ends and its own layers (with the flight layer once placed),
// a millisecond clock, and a small inventory grid for the size guard. Frames
// are plain numbers.
//
// seed_codec: the item table, every roll's seed and back, every range edge
// and one past it, and t, b, c or j off by one (not a flight charm).
// exclusion: with no charm worn every query keeps its real answer (baseline);
// with one worn a blocked family query is answered, while a wall, a lock, a
// re-run that a wall blocks, a monster and a point outside the room keep the
// real answer, a free answer stands and another self is never touched
// (target); InstancePlaceTallest's family result becomes noone, a wall result
// stays. residual: a charm taken off inside a prop keeps flight on until the
// first free read; outside a prop it goes straight to off. size_guard: outside
// a load scope or for another item nothing changes (baseline); a dressed 2x2
// or 1x5 charm waits for the end of its grid's load, a vanilla item takes its
// cells first, a refused full footprint becomes 1x1 at its own cell (kept) and
// a refused 1x1 counts failed; nested scopes flush their own lists (target).
// front_layer: the flight layer one in front of the lowest of the game layers'
// front end and every drawn room layer, a census once per room visit and a
// re-choice at most once a second, validated where it is written, no write for
// off, residual or another self; front_layer_filter: never past a filter, the
// rearmost filter deciding (the room stacks are the data file's). tooltip_rows:
// a forged item's gold rows anchor at its first present format-2/3 stat call
// (baseline), or at its first format-2/3 call when it has no stats at all
// (target, Pocket Elf), at most once a pass and never with no rows
// (ForgedTooltipRows.hpp). stand_in: the even-count rule, its order, its refusal, the
// coin. stat_line: the fresh session's line, byte for byte. install and
// command: the install states and the `flightcharm` forms.
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// The research build's charmprobe header shares the namespace. Including it
// first proves the two compile together, as ModuleMain.cpp includes both.
#include <ForgePact/FlightCharmProbe.hpp>
// Where a forged item's gold rows anchor in the tooltip pass. It is not a
// flight header, but Pocket Elf is the item with no stats that needs it.
#include <ForgePact/ForgedTooltipRows.hpp>

// PRODUCTION_FLIGHTCHARMS

using namespace ForgePact::FlightCharms;

// ---- the controlled world -------------------------------------------------
// Parents as the data file gives them (RDM § 19.8): Wall_Parent_obj and
// Lock_obj are children of Collision_Parent_obj, Gate_Parent_obj a child of
// Wall_Parent_obj, Block_Dynamic_Sprite_obj (the ravine's blocker) a child of
// Block_obj, which is not a wall.
enum Obj : int {
    AvoidableParent = 1, CollisionParent = 957, CollisionProp = 959, Fence = 5001, Cart = 5002,
    WallParent = 5701, InvisibleWall = 2269, GateParent = 1881, GateChild = 5003, LockObj = 2509,
    LockChild = 5004, BlockObj = 609, BlockDynamic = 608, EnemyAggroable = 300, EnemyParent = 301,
    Skeleton = 5005,
};
static const std::map<int, int> kParent = {
    { CollisionParent, AvoidableParent }, { CollisionProp, CollisionParent }, { Fence, CollisionProp },
    { Cart, CollisionProp }, { WallParent, CollisionParent }, { InvisibleWall, WallParent },
    { GateParent, WallParent }, { GateChild, GateParent }, { LockObj, CollisionParent }, { LockChild, LockObj },
    { BlockObj, CollisionParent }, { BlockDynamic, BlockObj }, { EnemyParent, EnemyAggroable },
    { Skeleton, EnemyParent },
};
static long g_AncestorCalls = 0;
static bool objectIsAncestor(int object, int ancestor)
{
    ++g_AncestorCalls;
    auto it = kParent.find(object);
    for (int d = 0; it != kParent.end() && d < 64; ++d) {
        if (it->second == ancestor) return true;
        it = kParent.find(it->second);
    }
    return false;
}

// One room layer as the runtime holds it: what layer_get_depth,
// layer_get_name, layer_get_visible, the length of layer_get_all_elements and
// layer_get_fx (a struct, or not) would answer.
struct WorldLayer {
    int64_t     id = -1;
    double      depth = 0.0;
    std::string name;
    bool        visible = true;
    int64_t     elements = 0;
    bool        effect = false;
};

struct FlightWorld {
    unsigned equipped = 0;          // CharmBit()s of the charms the player wears
    bool equippedReadable = true;
    bool overlapping = false;       // the player stands inside the family
    bool overlapReadable = true;
    double roomW = 2000.0, roomH = 1500.0;
    bool roomReadable = true;
    std::map<int64_t, std::pair<LayerEnd, LayerEnd>> layers;   // room -> gameLayer[room]'s two ends
    long equippedReads = 0, overlapReads = 0, roomReads = 0, layerReads = 0;
    // The current room's own layers, which the census reads; the flight layer
    // joins them once placed, and dies with them when a room is unloaded.
    int64_t room = 7;                                   // the room the layer builtins see
    std::map<int64_t, std::vector<WorldLayer>> rooms;   // room -> its layers
    bool listReadable = true;                           // layer_get_all
    std::set<int64_t> effectFails;                      // layers whose layer_get_fx read fails
    double nowMs = 0.0;
    int64_t nextId = 900;
    long listReads = 0, detailReads = 0, effectReads = 0, countReads = 0;
    long places = 0, creates = 0, moves = 0, valids = 0;
};

static WorldLayer* worldLayer(FlightWorld& w, int64_t id)
{
    for (WorldLayer& l : w.rooms[w.room])
        if (l.id == id) return &l;
    return nullptr;
}

// The current room's flight layer, or nullptr.
static WorldLayer* worldFlightLayer(FlightWorld& w)
{
    for (WorldLayer& l : w.rooms[w.room])
        if (l.name == kFlightLayerName) return &l;
    return nullptr;
}

// gameLayer[room]'s ends as Live 1 read them: index 0 at 0, the last at -5500.
static const std::pair<LayerEnd, LayerEnd> kGameEnds = { LayerEnd{ 11, 0.0 }, LayerEnd{ 19, -5500.0 } };

static Flight makeFlight(FlightWorld& w)
{
    Flight f;
    f.SetIsAncestor(objectIsAncestor);
    f.SetFamily(CollisionParent, { WallParent, LockObj });
    f.SetEquipped([&w](unsigned& mask) {
        ++w.equippedReads;
        if (!w.equippedReadable) return false;
        mask = w.equipped;
        return true;
    });
    f.SetOverlap([&w](bool& inside) {
        ++w.overlapReads;
        if (!w.overlapReadable) return false;
        inside = w.overlapping;
        return true;
    });
    f.SetRoomSize([&w](double& rw, double& rh) {
        ++w.roomReads;
        if (!w.roomReadable) return false;
        rw = w.roomW;
        rh = w.roomH;
        return true;
    });
    f.SetLayerEnds([&w](int64_t room, LayerEnd& first, LayerEnd& last) {
        ++w.layerReads;
        const auto it = w.layers.find(room);
        if (it == w.layers.end()) return false;
        first = it->second.first;
        last = it->second.second;
        return true;
    });
    Flight::LayerOps ops;
    ops.all = [&w](std::vector<LayerEnd>& all) {
        ++w.listReads;
        if (!w.listReadable) return false;
        for (const WorldLayer& l : w.rooms[w.room]) all.push_back(LayerEnd{ l.id, l.depth });
        return true;
    };
    ops.detail = [&w](int64_t id, RoomLayer& l) {
        ++w.detailReads;
        const WorldLayer* p = worldLayer(w, id);
        if (!p) return false;
        l.name = p->name;
        l.visible = p->visible;
        l.elements = p->elements;
        return true;
    };
    ops.effect = [&w](int64_t id, bool& effect) {
        ++w.effectReads;
        const WorldLayer* p = worldLayer(w, id);
        if (!p || w.effectFails.count(id)) return false;
        effect = p->effect;
        return true;
    };
    ops.count = [&w](int64_t id, int64_t& elements) {
        ++w.countReads;
        const WorldLayer* p = worldLayer(w, id);
        if (!p) return false;
        elements = p->elements;
        return true;
    };
    // layer_get_id by name first, moved to the chosen depth; layer_create only
    // when the room holds none.
    ops.place = [&w](double depth, int64_t& id) {
        ++w.places;
        if (WorldLayer* p = worldFlightLayer(w)) {
            p->depth = depth;
            id = p->id;
            return true;
        }
        ++w.creates;
        w.rooms[w.room].push_back(WorldLayer{ w.nextId++, depth, std::string(kFlightLayerName), true, 0, false });
        id = w.rooms[w.room].back().id;
        return true;
    };
    ops.move = [&w](int64_t id, double depth) {
        ++w.moves;
        WorldLayer* p = worldLayer(w, id);
        if (!p) return false;
        p->depth = depth;
        return true;
    };
    ops.valid = [&w](int64_t id) {
        ++w.valids;
        const WorldLayer* p = worldLayer(w, id);
        return p && p->name == kFlightLayerName;
    };
    ops.clock = [&w]() { return w.nowMs; };
    f.SetLayerOps(std::move(ops));
    return f;
}

// One UpdateDepth of the local player in `room` at `ms`; the player then sits
// on the layer written, so the flight layer holds an element as in the game.
static DepthWrite depthAt(Flight& f, FlightWorld& w, int64_t frame, int64_t room, double ms)
{
    w.room = room;
    w.nowMs = ms;
    const DepthWrite d = f.OnUpdateDepth(frame, true, room);
    if (d.write)
        if (WorldLayer* p = worldLayer(w, d.layer)) p->elements = 1;
    return d;
}

static double flightDepthIn(FlightWorld& w, int64_t room)
{
    w.room = room;
    const WorldLayer* p = worldFlightLayer(w);
    return p ? p->depth : std::numeric_limits<double>::quiet_NaN();
}

static std::string counts(const FlightCounters& c);

static std::string visitText(const Flight& f, FlightWorld& w, int64_t room)
{
    const FlightLayerVisit& v = f.Visit();
    return "have=" + std::to_string(v.have) + " depth=" + std::to_string(v.depth)
        + " world-depth=" + std::to_string(flightDepthIn(w, room)) + " by=" + std::to_string(v.by)
        + " lists=" + std::to_string(w.listReads) + " counts=" + std::to_string(w.countReads)
        + " effects=" + std::to_string(w.effectReads) + " creates=" + std::to_string(w.creates)
        + " moves=" + std::to_string(w.moves) + " | " + counts(f.Stats());
}

// The room stacks the data file gives (context § "Drawing in front"), each a
// list of { id, depth, name, visible, elements, effect }; the ids are the
// harness's own.
static std::vector<WorldLayer> town01()
{
    return {
        { 301, -10300.0, "Effect_1", true, 0, true },               // _filter_lut_colour
        { 302, -10200.0, "Wind_Leaves", false, 0, true },
        { 303, -10100.0, "Xmas_Event_Assets_1", false, 6, false },
        { 304, -10000.0, "Invisible_walls", true, 77, false },
        { 305, -9900.0, "Windmill_Propels", true, 4, false },
        { 306, -9800.0, "Raven_Creators", true, 3, false },
        { 307, -9700.0, "Tree_Leaves", true, 34, true },              // _filter_underwater, the sway
        { 308, 0.0, "Flames", true, 5, false },
        { 309, 5.0, "Objects", true, 430, false },
        { 310, 608.0, "Background_Tiles", true, 1, false },
    };
}

static std::vector<WorldLayer> act01Pregame()
{
    return {
        { 401, -10400.0, "Instances_2", true, 2, false },
        { 402, -10300.0, "Effect_1", true, 0, true },
        { 403, -10150.0, "Wind_Leaves", true, 0, true },
        { 404, -10100.0, "Raven_Creators", true, 3, false },
        { 405, 5.0, "Objects", true, 200, false },
    };
}

static std::vector<WorldLayer> act0905()
{
    return {
        { 501, -11300.0, "Sparks", true, 0, true },
        { 502, -11200.0, "Smoke", true, 6, false },
        { 503, -11100.0, "Effect_2", true, 0, true },
        { 504, -11000.0, "Effect_1", true, 0, true },
        { 505, 5.0, "Objects", true, 300, false },
    };
}

static std::vector<WorldLayer> act0101()
{
    return {
        { 601, -10100.0, "Effect_1", true, 0, true },               // _filter_lut_colour
        { 602, -10000.0, "Tree_Leaves", true, 0, true },            // empty until the canopies exist
        { 603, 5.0, "Objects", true, 250, false },
    };
}

// A world with one room (7) holding `stack` behind gameLayer's measured ends,
// and a Flight wearing Wings in it.
static Flight flightIn(FlightWorld& w, std::vector<WorldLayer> stack)
{
    w.layers[7] = kGameEnds;
    w.rooms[7] = std::move(stack);
    Flight f = makeFlight(w);
    w.equipped = CharmBit(Charm::Wings);
    return f;
}

static bool sumsHold(const FlightCounters& c)
{
    return c.depthCalls == c.depthOther + c.depthSeen && c.depthSeen == c.depthIdle + c.noFront + c.depth;
}

// One hooked call after its original ran. `wallBlocks` is what re-running the
// same original against Wall_Parent_obj and Lock_obj answers.
static long g_WallAsks = 0;
static Answer ask(Flight& f, Hooked row, int64_t frame, bool player, int object, bool blocked,
    bool wallBlocks = false, double x0 = 100.0, double y0 = 100.0, double x1 = 100.0, double y1 = 100.0)
{
    Query q{ row, frame, player, object, blocked, x0, y0, x1, y1 };
    return f.OnQuery(q, [wallBlocks]() { ++g_WallAsks; return wallBlocks; });
}

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static bool has(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }

// ---- one tooltip pass, the way Hook_DrawInventoryStatsNew walks it ---------
// The pass calls DrawInventoryStatsNew once per stat line it knows, whether
// or not the item has that stat (RDM § 16.6). Each call carries a format and
// whether its stat is present on the item. The rows are armed once a pass,
// with the item's no-stats answer; an anchor draws them and ends the pending
// state, as the adapter does. The stat is read only when the rule asks.
struct TipCall { double format; bool present; };
struct TipPass { int anchor = -1; int anchors = 0; int statReads = 0; };
static TipPass tipPass(size_t rows, bool noStats, const std::vector<TipCall>& calls)
{
    namespace R = ForgePact::ForgedTooltipRows;
    TipPass p;
    bool pending = R::Pending(rows);
    for (size_t i = 0; i < calls.size(); ++i) {
        bool present = false;
        if (R::NeedsStatRead(pending, calls[i].format, noStats)) { ++p.statReads; present = calls[i].present; }
        if (R::AnchorsHere(pending, calls[i].format, present, noStats)) {
            if (p.anchor < 0) p.anchor = static_cast<int>(i);
            ++p.anchors;
            pending = false;
        }
    }
    return p;
}
static std::string tipText(const TipPass& p)
{
    return "anchor=" + std::to_string(p.anchor) + " anchors=" + std::to_string(p.anchors) + " reads=" + std::to_string(p.statReads);
}

static std::string counts(const FlightCounters& c)
{
    return "answered=" + std::to_string(c.answered) + " excluded=" + std::to_string(c.excluded)
        + " outside-room=" + std::to_string(c.outsideRoom) + " depth=" + std::to_string(c.depth);
}

static bool same(const FlightCounters& c, uint64_t answered, uint64_t excluded, uint64_t outside, uint64_t depth)
{
    return c.answered == answered && c.excluded == excluded && c.outsideRoom == outside && c.depth == depth;
}

// What each row answers when flight lets the query through.
static Answer expected(Hooked row)
{
    return (row == Hooked::PositionMeeting || row == Hooked::PlaceMeeting) ? Answer::False : Answer::Noone;
}

// ---- the size guard's inventory -------------------------------------------
struct Pending { int id = 0; int x = 0; int y = 0; };
struct Saved { int id; int x; int y; int w; int h; bool charm; };

// A grid as InventoryGridAddItemPos fills it (RDM § 22.6, static reading): the
// anchor and every covered cell must be inside the grid and free, or nothing
// is written.
struct Grid {
    int cols = 6, rows = 4;
    std::map<std::pair<int, int>, int> cells;   // cell -> item id; ids start at 1
    bool place(int id, int x, int y, int w, int h)
    {
        if (x < 0 || y < 0 || x + w > cols || y + h > rows) return false;
        for (int dx = 0; dx < w; ++dx)
            for (int dy = 0; dy < h; ++dy)
                if (cells.count({ x + dx, y + dy })) return false;
        for (int dx = 0; dx < w; ++dx)
            for (int dy = 0; dy < h; ++dy) cells[{ x + dx, y + dy }] = id;
        return true;
    }
    int at(int x, int y) const
    {
        const auto it = cells.find({ x, y });
        return it == cells.end() ? 0 : it->second;
    }
    int covered(int id) const
    {
        int n = 0;
        for (const auto& c : cells) if (c.second == id) ++n;
        return n;
    }
};

// One load of a saved grid, as § 22.6 reads ParseItemToGrid: each entry is
// placed at its saved cell and a refusal is ignored. Guarded, the adapter's
// two hooks run around it: the scope opens, a charm's placement asks the
// guard first, and the scope closes once the last entry is placed.
//
// `transposed` models a game that reads the axes the other way round from the
// guard's reading of the arguments: every item then covers h columns by w
// rows. `unknownCell` names the entries whose cell the adapter could not read:
// the guard is told -1,-1, while the game still places them at their own cell.
struct Load {
    Grid grid;
    SizeGuard<Pending> guard;
    std::map<int, Saved> saved;
    std::vector<std::pair<Pending, GuardOutcome>> outcomes;
    bool transposed = false;
    std::set<int> unknownCell;

    bool put(int id, int w, int h)
    {
        const Saved& e = saved.at(id);
        return transposed ? grid.place(id, e.x, e.y, h, w) : grid.place(id, e.x, e.y, w, h);
    }
    void run(const std::vector<Saved>& entries, bool guarded)
    {
        for (const Saved& e : entries) saved[e.id] = e;
        if (guarded) guard.Open();
        for (const Saved& e : entries) {
            const bool known = unknownCell.count(e.id) == 0;
            if (guarded && guard.Defer(Pending{ e.id, known ? e.x : -1, known ? e.y : -1 }, e.charm, e.w, e.h)) continue;
            put(e.id, e.w, e.h);
        }
        if (guarded) close();
    }
    void close()
    {
        guard.Close(
            [this](const Pending& p) { const Saved& e = saved.at(p.id); return put(p.id, e.w, e.h); },
            [this](const Pending& p) { return put(p.id, 1, 1); },
            [this](const Pending& p, GuardOutcome o) { outcomes.emplace_back(p, o); });
    }
    int outcome(int id, GuardOutcome o) const
    {
        int n = 0;
        for (const auto& r : outcomes) n += (r.first.id == id && r.second == o) ? 1 : 0;
        return n;
    }
};

static std::string guardText(const GuardCounters& c)
{
    return "deferred=" + std::to_string(c.deferred) + " full=" + std::to_string(c.full)
        + " kept=" + std::to_string(c.kept) + " yielded=" + std::to_string(c.yielded)
        + " failed=" + std::to_string(c.failed);
}

int main()
{
    // ======================= seed_codec ====================================
    {
        const CharmRow& e = RowOf(Charm::Elf);
        const CharmRow& wg = RowOf(Charm::Wings);
        const CharmRow& br = RowOf(Charm::Broom);
        bool order = true;
        for (int i = 0; i < kCharmCount; ++i) order = order && static_cast<int>(kCharms[i].charm) == i;
        const bool ok = kCharmCount == 3 && order && kCharmTypeT == 10 && kCharmBaseB == 0
            && e.key == "elf" && e.name == "Pocket Elf" && e.rarity == 6 && e.tier == 5 && e.w == 1 && e.h == 1
            && e.seedLo == 777100 && e.seedHi == 777100 && e.statCount == 0
            && wg.key == "wings" && wg.name == "Gabriel Wings" && wg.rarity == 7 && wg.tier == 5 && wg.w == 2 && wg.h == 2
            && wg.seedLo == 777200 && wg.seedHi == 777343 && wg.statCount == 2
            && wg.stats[0].id == 159 && wg.stats[0].lo == 1 && wg.stats[0].hi == 12
            && wg.stats[1].id == 160 && wg.stats[1].lo == 1 && wg.stats[1].hi == 12
            && br.key == "broom" && br.name == "Witch's Broomstick" && br.rarity == 10 && br.tier == 5 && br.w == 1 && br.h == 5
            && br.seedLo == 777400 && br.seedHi == 777490 && br.statCount == 1
            && br.stats[0].id == 25 && br.stats[0].lo == 10 && br.stats[0].hi == 100
            && e.bagW == 32 && e.bagH == 32 && e.groundW == 16 && e.groundH == 16
            && wg.bagW == 64 && wg.bagH == 64 && wg.groundW == 24 && wg.groundH == 24
            && br.bagW == 32 && br.bagH == 160 && br.groundW == 34 && br.groundH == 17
            && e.lore == "A tiny helper who knows every shortcut." && wg.lore == "Lighter than any wall is tall."
            && br.lore == "Swept clean of every obstacle."
            && kAffixRow == "Flight" && kMechanic == "flight" && kGuardTag == "fp_flight_guarded";
        check("seed_codec/the_table", ok);
    }
    {
        bool ok = SeedFor(Charm::Elf) == 777100 && SeedFor(Charm::Elf, 5, 5) == 777100;
        Rolled r;
        ok = ok && DecodeSeed(777100, r) && r.charm == Charm::Elf && r.count == 0;
        std::set<long long> seen;
        seen.insert(777100);
        for (int p = 1; p <= 12; ++p) {
            for (int m = 1; m <= 12; ++m) {
                const long long a = SeedFor(Charm::Wings, p, m);
                Rolled d;
                ok = ok && a == 777200 + (p - 1) * 12 + (m - 1) && DecodeSeed(a, d) && d.charm == Charm::Wings
                    && d.count == 2 && d.stat[0] == 159 && d.value[0] == p && d.stat[1] == 160 && d.value[1] == m;
                seen.insert(a);
            }
        }
        for (int s = 10; s <= 100; ++s) {
            const long long a = SeedFor(Charm::Broom, s);
            Rolled d;
            ok = ok && a == 777400 + (s - 10) && DecodeSeed(a, d) && d.charm == Charm::Broom && d.count == 1
                && d.stat[0] == 25 && d.value[0] == s;
            seen.insert(a);
        }
        check("seed_codec/round_trips_every_roll", ok && seen.size() == 1u + 144u + 91u,
            "distinct=" + std::to_string(seen.size()));
    }
    {
        struct Edge { long long a; bool charm; };
        const Edge edges[] = {
            { 777099, false }, { 777100, true }, { 777101, false }, { 777199, false }, { 777200, true },
            { 777343, true }, { 777344, false }, { 777350, false }, { 777399, false }, { 777400, true },
            { 777490, true }, { 777491, false },
        };
        bool ok = true;
        std::string bad;
        for (const Edge& g : edges) {
            Rolled d;
            if (DecodeSeed(g.a, d) != g.charm) { ok = false; bad += " " + std::to_string(g.a); }
        }
        Rolled lo, hi, row, next, blo, bhi;
        ok = ok && DecodeSeed(777200, lo) && lo.value[0] == 1 && lo.value[1] == 1
            && DecodeSeed(777343, hi) && hi.value[0] == 12 && hi.value[1] == 12
            && DecodeSeed(777211, row) && row.value[0] == 1 && row.value[1] == 12
            && DecodeSeed(777212, next) && next.value[0] == 2 && next.value[1] == 1
            && DecodeSeed(777400, blo) && blo.value[0] == 10 && DecodeSeed(777490, bhi) && bhi.value[0] == 100;
        check("seed_codec/every_range_edge_and_one_past_it", ok, bad);
    }
    {
        const bool ok = SeedFor(Charm::Wings, 0, 1) == -1 && SeedFor(Charm::Wings, 13, 1) == -1
            && SeedFor(Charm::Wings, 1, 0) == -1 && SeedFor(Charm::Wings, 1, 13) == -1
            && SeedFor(Charm::Broom, 9) == -1 && SeedFor(Charm::Broom, 101) == -1
            && SeedFor(static_cast<Charm>(7), 1, 1) == -1;
        check("seed_codec/an_out_of_range_roll_has_no_seed", ok);
    }
    {
        Rolled r;
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        const bool yes = IsFlightCharm(10, 777100, 0, 0, 0, r) && r.charm == Charm::Elf
            && IsFlightCharm(10, 777250, 0, 0, 0, r) && r.charm == Charm::Wings
            && IsFlightCharm(10, 777455, 0, 0, 0, r) && r.charm == Charm::Broom && r.value[0] == 65;
        const bool no = !IsFlightCharm(9, 777100, 0, 0, 0, r) && !IsFlightCharm(11, 777100, 0, 0, 0, r)
            && !IsFlightCharm(10, 777100, 1, 0, 0, r) && !IsFlightCharm(10, 777100, -1, 0, 0, r)
            && !IsFlightCharm(10, 777100, 0, 1, 0, r) && !IsFlightCharm(10, 777100, 0, -1, 0, r)
            && !IsFlightCharm(10, 777100, 0, 0, 1, r) && !IsFlightCharm(10, 777100, 0, 0, -1, r)
            && !IsFlightCharm(10, 777250, 1, 0, 0, r) && !IsFlightCharm(10, 777455, 0, 0, 1, r)
            && !IsFlightCharm(10, 777100.5, 0, 0, 0, r) && !IsFlightCharm(10, nan, 0, 0, 0, r)
            && !IsFlightCharm(10, inf, 0, 0, 0, r) && !IsFlightCharm(nan, 777100, 0, 0, 0, r)
            && !IsFlightCharm(10, 777100, 0.5, 0, 0, r);
        check("seed_codec/t_b_c_or_j_off_by_one_is_not_a_flight_charm", yes && no);
    }
    {
        bool ok = SeedFromUniform(Charm::Wings, 0.0) == 777200 && SeedFromUniform(Charm::Wings, 0.999999999) == 777343
            && SeedFromUniform(Charm::Wings, 1.0) == 777343 && SeedFromUniform(Charm::Wings, -0.5) == 777200
            && SeedFromUniform(Charm::Wings, std::numeric_limits<double>::quiet_NaN()) == 777200
            && SeedFromUniform(Charm::Elf, 0.7) == 777100 && SeedFromUniform(Charm::Broom, 0.0) == 777400
            && SeedFromUniform(Charm::Broom, 0.9999999) == 777490 && TopSeed(Charm::Wings) == 777343
            && TopSeed(Charm::Broom) == 777490 && TopSeed(Charm::Elf) == 777100;
        std::set<long long> wings, broom;
        for (int k = 0; k < 144; ++k) wings.insert(SeedFromUniform(Charm::Wings, (k + 0.5) / 144.0));
        for (int k = 0; k < 91; ++k) broom.insert(SeedFromUniform(Charm::Broom, (k + 0.5) / 91.0));
        ok = ok && wings.size() == 144u && *wings.begin() == 777200 && *wings.rbegin() == 777343
            && broom.size() == 91u && *broom.begin() == 777400 && *broom.rbegin() == 777490;
        check("seed_codec/a_uniform_draw_covers_each_roll_once", ok);
    }
    {
        Rolled r;
        bool ok = true;
        for (long long a : { 777001LL, 777002LL, 777003LL, 779000LL, kSeedBase, SpawnSeed(1), SpawnSeed(400) })
            ok = ok && !DecodeSeed(a, r);
        check("seed_codec/no_collision_with_seeds_in_use", ok);
    }
    {
        Charm c = Charm::Elf;
        bool ok = CharmFromKey("wings", c) && c == Charm::Wings && CharmFromKey("broom", c) && c == Charm::Broom
            && CharmFromKey("elf", c) && c == Charm::Elf && !CharmFromKey("Elf", c) && !CharmFromKey("", c)
            && !CharmFromKey("wing", c) && CharmBit(Charm::Elf) == 1u && CharmBit(Charm::Wings) == 2u
            && CharmBit(Charm::Broom) == 4u && kAllCharmBits == 7u;
        Rolled w, b, e;
        DecodeSeed(SeedFor(Charm::Wings, 3, 7), w);
        DecodeSeed(SeedFor(Charm::Broom, 55), b);
        DecodeSeed(SeedFor(Charm::Elf), e);
        const auto ws = StatsFor(w);
        const auto bs = StatsFor(b);
        ok = ok && ws.size() == 2u && ws[0] == std::make_pair(159, 3) && ws[1] == std::make_pair(160, 7)
            && bs.size() == 1u && bs[0] == std::make_pair(25, 55) && StatsFor(e).empty();
        check("seed_codec/keys_and_stats", ok);
    }

    // ======================= exclusion =====================================
    {
        bool ok = true;
        const char* names[kHookedCount] = { "position_meeting", "place_meeting", "instance_position",
                                            "collision_line", "collision_circle", "InstancePlaceTallest" };
        const int objectArgs[kHookedCount] = { 2, 2, 2, 4, 3, 2 };
        const int points[kHookedCount] = { 1, 1, 1, 2, 1, 1 };
        for (int i = 0; i < kHookedCount; ++i) {
            const HookedRow& h = kHooked[i];
            ok = ok && h.name == names[i] && h.objectArg == objectArgs[i] && h.points == points[i]
                && h.answer == expected(static_cast<Hooked>(i)) && h.rerun == (i != 5);
        }
        ok = ok && kNoone == -4.0 && AnswerName(Answer::Real) == "real" && AnswerName(Answer::Noone) == "noone"
            && AnswerName(Answer::False) == "false";
        check("exclusion/the_six_rows", ok);
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        g_WallAsks = 0;
        bool ok = true;
        for (int i = 0; i < kHookedCount; ++i) {
            const Hooked row = static_cast<Hooked>(i);
            for (int frame = 10; frame < 13; ++frame) {
                ok = ok && ask(f, row, frame, true, Fence, true) == Answer::Real
                    && ask(f, row, frame, true, BlockDynamic, true) == Answer::Real
                    && ask(f, row, frame, true, WallParent, true) == Answer::Real
                    && ask(f, row, frame, true, Fence, false) == Answer::Real;
            }
        }
        check("exclusion/baseline_no_charm_every_query_real",
            ok && f.CurrentState() == State::Off && g_WallAsks == 0 && w.roomReads == 0 && w.overlapReads == 0
                && same(f.Stats(), 0, 0, 0, 0),
            counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = true;
        std::string got;
        for (int i = 0; i < kHookedCount; ++i) {
            const Hooked row = static_cast<Hooked>(i);
            const Answer a = ask(f, row, 10, true, Fence, true, false, 100.0, 100.0, 300.0, 400.0);
            got += " " + std::string(AnswerName(a));
            ok = ok && a == expected(row);
        }
        check("exclusion/target_a_blocked_family_query_is_answered",
            ok && f.CurrentState() == State::On && g_WallAsks == 5 && same(f.Stats(), 6, 0, 0, 0),
            got + " " + counts(f.Stats()) + " wall-asks=" + std::to_string(g_WallAsks));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Elf);
        g_WallAsks = 0;
        bool walls = true, locks = true;
        for (int i = 0; i < kHookedCount; ++i) {
            const Hooked row = static_cast<Hooked>(i);
            for (int o : { WallParent, InvisibleWall, GateParent, GateChild }) walls = walls && ask(f, row, 10, true, o, true) == Answer::Real;
            for (int o : { LockObj, LockChild }) locks = locks && ask(f, row, 10, true, o, true) == Answer::Real;
        }
        check("exclusion/target_a_wall_or_lock_named_query_keeps_its_answer",
            walls && locks && g_WallAsks == 0 && same(f.Stats(), 0, 6 * kHookedCount, 0, 0), counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Broom);
        g_WallAsks = 0;
        bool ok = true;
        for (int i = 0; i < 5; ++i) ok = ok && ask(f, static_cast<Hooked>(i), 10, true, Fence, true, true) == Answer::Real;
        check("exclusion/target_a_query_whose_rerun_a_wall_blocks_keeps_its_answer",
            ok && g_WallAsks == 5 && same(f.Stats(), 0, 5, 0, 0), counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = true;
        for (int i = 0; i < kHookedCount; ++i) {
            for (int o : { Skeleton, EnemyParent, EnemyAggroable, AvoidableParent })
                ok = ok && ask(f, static_cast<Hooked>(i), 10, true, o, true) == Answer::Real;
        }
        check("exclusion/target_a_monster_named_query_keeps_its_answer",
            ok && g_WallAsks == 0 && w.roomReads == 0 && same(f.Stats(), 0, 0, 0, 0), counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const bool outside = ask(f, Hooked::PlaceMeeting, 10, true, Fence, true, false, -1.0, 100.0) == Answer::Real
            && ask(f, Hooked::PlaceMeeting, 10, true, Fence, true, false, 2000.0, 100.0) == Answer::Real
            && ask(f, Hooked::PositionMeeting, 10, true, Fence, true, false, 100.0, 1500.0) == Answer::Real
            && ask(f, Hooked::InstancePosition, 10, true, Fence, true, false, 100.0, -0.5) == Answer::Real
            && ask(f, Hooked::PlaceMeeting, 10, true, Fence, true, false, nan, 100.0) == Answer::Real
            && ask(f, Hooked::CollisionLine, 10, true, Fence, true, false, 100.0, 100.0, 2100.0, 100.0) == Answer::Real
            && ask(f, Hooked::CollisionCircle, 10, true, Fence, true, false, 2500.0, 100.0) == Answer::Real
            && ask(f, Hooked::InstancePlaceTallest, 10, true, Fence, true, false, 100.0, 1600.0) == Answer::Real;
        const FlightCounters mid = f.Stats();
        const bool inside = ask(f, Hooked::PlaceMeeting, 10, true, Fence, true, false, 0.0, 0.0) == Answer::False
            && ask(f, Hooked::PlaceMeeting, 10, true, Fence, true, false, 1999.5, 1499.5) == Answer::False
            && ask(f, Hooked::CollisionLine, 10, true, Fence, true, false, 100.0, 100.0, 1900.0, 1400.0) == Answer::Noone;
        w.roomReadable = false;
        const bool unreadable = ask(f, Hooked::PlaceMeeting, 10, true, Fence, true) == Answer::Real;
        check("exclusion/target_a_point_outside_the_room_keeps_its_answer",
            outside && same(mid, 0, 0, 8, 0) && inside && unreadable && same(f.Stats(), 3, 0, 9, 0), counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = true;
        for (int i = 0; i < kHookedCount; ++i) ok = ok && ask(f, static_cast<Hooked>(i), 10, true, Fence, false) == Answer::Real;
        check("exclusion/target_a_free_answer_stands",
            ok && g_WallAsks == 0 && w.roomReads == 0 && same(f.Stats(), 0, 0, 0, 0), counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = true;
        for (int frame = 1; frame <= 20; ++frame)
            for (int i = 0; i < kHookedCount; ++i) ok = ok && ask(f, static_cast<Hooked>(i), frame, false, Fence, true) == Answer::Real;
        const DepthWrite d = f.OnUpdateDepth(5, false, 7);
        check("exclusion/target_another_self_is_never_touched",
            ok && !d.write && g_WallAsks == 0 && w.equippedReads == 0 && w.roomReads == 0 && w.layerReads == 0
                && f.CurrentState() == State::Off && same(f.Stats(), 0, 0, 0, 0),
            "equipped-reads=" + std::to_string(w.equippedReads));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Elf);
        g_WallAsks = 0;
        const bool ok = ask(f, Hooked::InstancePlaceTallest, 10, true, Fence, true, true) == Answer::Noone
            && ask(f, Hooked::InstancePlaceTallest, 10, true, CollisionProp, true) == Answer::Noone
            && ask(f, Hooked::InstancePlaceTallest, 10, true, WallParent, true) == Answer::Real
            && ask(f, Hooked::InstancePlaceTallest, 10, true, GateChild, true) == Answer::Real
            && ask(f, Hooked::InstancePlaceTallest, 10, true, LockChild, true) == Answer::Real
            && ask(f, Hooked::InstancePlaceTallest, 10, true, Skeleton, true) == Answer::Real
            && ask(f, Hooked::InstancePlaceTallest, 10, true, -1, false) == Answer::Real
            && ask(f, Hooked::InstancePlaceTallest, 10, true, Fence, true, false, -5.0, 100.0) == Answer::Real;
        check("exclusion/instance_place_tallest_family_result_noone_wall_result_stays",
            ok && g_WallAsks == 0 && same(f.Stats(), 2, 3, 1, 0), counts(f.Stats()));
    }
    {
        // The ravine edge's blocker (Live 1) is Block_obj's child: family, not a wall.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Broom);
        const bool ok = ask(f, Hooked::PlaceMeeting, 10, true, BlockDynamic, true) == Answer::False
            && ask(f, Hooked::CollisionCircle, 10, true, Cart, true) == Answer::Noone
            && ask(f, Hooked::InstancePosition, 10, true, CollisionParent, true) == Answer::Noone;
        check("exclusion/target_block_dynamic_sprite_is_family_not_a_wall", ok && same(f.Stats(), 3, 0, 0, 0), counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        for (int k = 0; k < 10; ++k) ask(f, Hooked::PlaceMeeting, 5, true, Fence, true);
        for (int k = 0; k < 10; ++k) ask(f, Hooked::PlaceMeeting, 5, false, Fence, true);
        const long inFive = w.equippedReads;
        ask(f, Hooked::PlaceMeeting, 6, true, Fence, false);
        ask(f, Hooked::PlaceMeeting, 6, true, Fence, true);
        f.OnUpdateDepth(6, true, 7);
        check("exclusion/the_worn_charms_are_read_once_a_frame", inFive == 1 && w.equippedReads == 2,
            "reads=" + std::to_string(w.equippedReads));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        const bool nonObject = ask(f, Hooked::PlaceMeeting, 1, true, -1, true) == Answer::Real;
        f.SetFamily(-1, { WallParent, LockObj });
        const bool noFamily = !f.Configured() && ask(f, Hooked::PlaceMeeting, 2, true, Fence, true) == Answer::Real;
        f.SetFamily(CollisionParent, { -1, LockObj });
        const bool noWall = !f.Configured() && ask(f, Hooked::PlaceMeeting, 3, true, Fence, true) == Answer::Real;
        f.SetFamily(CollisionParent, { WallParent, LockObj });
        const bool restored = f.Configured() && ask(f, Hooked::PlaceMeeting, 4, true, Fence, true) == Answer::False;
        check("exclusion/an_unresolved_family_or_exclusion_answers_for_nothing",
            nonObject && noFamily && noWall && restored && same(f.Stats(), 1, 0, 0, 0), counts(f.Stats()));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        ask(f, Hooked::PlaceMeeting, 1, true, GateChild, true);
        const long after = g_AncestorCalls;
        for (int k = 0; k < 100; ++k) {
            ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
            ask(f, Hooked::PlaceMeeting, 1, true, GateChild, true);
        }
        check("exclusion/each_object_is_classified_once", g_AncestorCalls == after,
            std::to_string(g_AncestorCalls - after) + " extra ancestry questions");
    }

    // ======================= residual ======================================
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Broom);
        const bool on = ask(f, Hooked::PlaceMeeting, 1, true, Fence, true) == Answer::False && f.CurrentState() == State::On
            && w.overlapReads == 0;
        w.equipped = 0;
        w.overlapping = true;
        const bool residual = ask(f, Hooked::PlaceMeeting, 2, true, Fence, true) == Answer::False
            && f.CurrentState() == State::Residual && w.overlapReads == 1;
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true);
        const bool oncePerFrame = w.overlapReads == 1;
        const bool stillInside = ask(f, Hooked::CollisionCircle, 3, true, Fence, true) == Answer::Noone
            && f.CurrentState() == State::Residual && w.overlapReads == 2;
        w.overlapping = false;
        const bool freed = ask(f, Hooked::PlaceMeeting, 4, true, Fence, true) == Answer::Real && f.CurrentState() == State::Off;
        w.overlapping = true;   // walked into a prop with no charm worn: vanilla again
        const bool staysOff = ask(f, Hooked::PlaceMeeting, 5, true, Fence, true) == Answer::Real
            && f.CurrentState() == State::Off && w.overlapReads == 3;
        check("residual/taken_off_inside_a_prop_until_the_first_free_read",
            on && residual && oncePerFrame && stillInside && freed && staysOff,
            "state=" + std::string(StateName(f.CurrentState())) + " overlap-reads=" + std::to_string(w.overlapReads));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        w.equipped = 0;
        w.overlapping = false;
        const bool ok = ask(f, Hooked::PlaceMeeting, 2, true, Fence, true) == Answer::Real && f.CurrentState() == State::Off
            && w.overlapReads == 1;
        check("residual/taken_off_outside_a_prop_goes_straight_to_off", ok);
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        w.equipped = 0;
        w.overlapping = true;
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true);
        const bool residual = f.CurrentState() == State::Residual;
        w.equipped = CharmBit(Charm::Elf);
        ask(f, Hooked::PlaceMeeting, 3, true, Fence, true);
        ask(f, Hooked::PlaceMeeting, 4, true, Fence, true);
        check("residual/worn_again_goes_back_on",
            residual && f.CurrentState() == State::On && w.overlapReads == 1 && f.EquippedMask() == CharmBit(Charm::Elf));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        w.equipped = 0;
        w.overlapping = true;
        w.overlapReadable = false;
        const bool unreadableOverlap = ask(f, Hooked::PlaceMeeting, 2, true, Fence, true) == Answer::Real
            && f.CurrentState() == State::Off;
        w.overlapReadable = true;
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 3, true, Fence, true);
        w.equippedReadable = false;
        const bool unreadableCharms = ask(f, Hooked::PlaceMeeting, 4, true, Fence, true) == Answer::False
            && f.CurrentState() == State::Residual && f.EquippedMask() == 0u;
        check("residual/an_unreadable_read_fails_closed", unreadableOverlap && unreadableCharms,
            "state=" + std::string(StateName(f.CurrentState())));
    }
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.overlapping = true;
        bool ok = true;
        for (int frame = 1; frame <= 5; ++frame) ok = ok && ask(f, Hooked::PlaceMeeting, frame, true, Fence, true) == Answer::Real;
        check("residual/baseline_no_charm_is_never_residual",
            ok && f.CurrentState() == State::Off && w.overlapReads == 0 && w.equippedReads == 5);
    }

    // ======================= front_layer ===================================
    {
        // The game layers' front end is the lower of gameLayer[room]'s two end
        // depths, whichever index holds it; a non-finite end names none.
        double d = 0.0;
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const bool ok = GameFrontDepth(LayerEnd{ 1, 0.0 }, LayerEnd{ 2, -5500.0 }, d) && d == -5500.0
            && GameFrontDepth(LayerEnd{ 1, -50.0 }, LayerEnd{ 2, 300.0 }, d) && d == -50.0
            && GameFrontDepth(LayerEnd{ 1, 4.0 }, LayerEnd{ 2, 4.0 }, d) && d == 4.0
            && !GameFrontDepth(LayerEnd{ 1, nan }, LayerEnd{ 2, 4.0 }, d)
            && !GameFrontDepth(LayerEnd{ 1, 4.0 }, LayerEnd{ 2, std::numeric_limits<double>::infinity() }, d);
        check("front_layer/the_game_front_is_the_lower_end", ok);
    }
    {
        // Town_01_rm's stack: in front of Invisible_walls (-10000), behind the
        // room's only visible filter, Effect_1 (-10300).
        FlightWorld w;
        Flight f = flightIn(w, town01());
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        const bool town = d.write && d.layer == v.layer && v.have && v.depth == -10001.0 && flightDepthIn(w, 7) == -10001.0
            && v.by >= 0 && v.layers[(size_t)v.by].name == "Invisible_walls" && v.gameFrontRead && v.gameFront == -5500.0
            && v.listed == 10 && w.creates == 1;
        // No room layer in front of the game layers: one in front of -5500.
        FlightWorld e;
        Flight g = flightIn(e, { { 309, 5.0, "Objects", true, 430, false }, { 310, 608.0, "Background_Tiles", true, 1, false } });
        const DepthWrite de = depthAt(g, e, 1, 7, 0.0);
        const bool empty = de.write && g.Visit().depth == -5501.0 && g.Visit().by == -1 && flightDepthIn(e, 7) == -5501.0;
        check("front_layer/one_in_front_of_the_lowest_drawn_layer",
            town && empty && f.Stats().depth == 1 && g.Stats().depth == 1, visitText(f, w, 7) + " || " + visitText(g, e, 7));
    }
    {
        // A hidden layer, an empty layer with no effect, and the flight layer
        // itself (found by name, holding the player at -20000) never count.
        FlightWorld w;
        std::vector<WorldLayer> stack = town01();
        stack.push_back({ 320, -12000.0, "Hidden_Rocks", false, 9, false });
        stack.push_back({ 321, -11000.0, "Empty_Instances", true, 0, false });
        stack.push_back({ 322, -20000.0, std::string(kFlightLayerName), true, 1, false });
        Flight f = flightIn(w, stack);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        check("front_layer/hidden_empty_and_the_flight_layer_itself_never_count",
            d.write && d.layer == 322 && f.Visit().depth == -10001.0 && flightDepthIn(w, 7) == -10001.0 && w.creates == 0,
            visitText(f, w, 7));
    }
    {
        // The game layers' front end unreadable (no ends) or non-finite: no
        // flight layer, nothing written, counted in no-front. Another layer
        // whose depth reads non-finite is skipped.
        FlightWorld w;
        w.rooms[7] = town01();
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        const bool unreadable = !depthAt(f, w, 1, 7, 0.0).write && w.creates == 0;
        FlightWorld n;
        n.layers[7] = { LayerEnd{ 11, 0.0 }, LayerEnd{ 19, std::numeric_limits<double>::quiet_NaN() } };
        n.rooms[7] = town01();
        Flight g = makeFlight(n);
        n.equipped = CharmBit(Charm::Wings);
        const bool nonFinite = !depthAt(g, n, 1, 7, 0.0).write && n.creates == 0;
        FlightWorld s;
        std::vector<WorldLayer> stack = town01();
        stack.push_back({ 330, std::numeric_limits<double>::quiet_NaN(), "Odd", true, 5, false });
        Flight h = flightIn(s, stack);
        const bool skipped = depthAt(h, s, 1, 7, 0.0).write && h.Visit().depth == -10001.0;
        check("front_layer/an_unreadable_game_front_writes_nothing",
            unreadable && nonFinite && skipped && f.Stats().noFront == 1 && f.Stats().depth == 0 && g.Stats().noFront == 1
                && sumsHold(f.Stats()) && sumsHold(g.Stats()),
            counts(f.Stats()) + " || " + counts(g.Stats()) + " || " + visitText(h, s, 7));
    }
    {
        // The census once per room visit; the re-choice at most once a second,
        // only while on, recounting the remembered layers alone; a new visit
        // (the layer gone with its room) runs the census again.
        FlightWorld w;
        Flight f = flightIn(w, town01());
        bool ok = true;
        for (int frame = 1; frame <= 60; ++frame) ok = ok && depthAt(f, w, frame, 7, frame * 16.0).write;   // 16..960 ms
        const long listsInVisit = w.listReads, countsBefore = w.countReads;
        ok = ok && depthAt(f, w, 61, 7, 1000.0).write && depthAt(f, w, 62, 7, 1016.0).write;
        const long countsAfterOne = w.countReads;   // one recount: the 7 remembered layers in front of -5500
        // Off for three seconds: no write, no recount.
        w.equipped = 0;
        for (int frame = 63; frame <= 80; ++frame) ok = ok && !depthAt(f, w, frame, 7, 1016.0 + (frame - 62) * 200.0).write;
        const long countsOff = w.countReads;
        w.equipped = CharmBit(Charm::Wings);
        ok = ok && depthAt(f, w, 81, 7, 5000.0).write;
        const long countsBack = w.countReads;
        // The room unloads with its layers; the same room's next visit.
        w.rooms[7] = town01();
        ok = ok && depthAt(f, w, 82, 7, 5016.0).write;
        check("front_layer/the_census_once_per_visit_the_rechoice_once_a_second",
            ok && listsInVisit == 1 && countsBefore == 0 && countsAfterOne == 7 && countsOff == 7 && countsBack == 14
                && w.listReads == 2 && w.creates == 2 && w.moves == 0 && f.Stats().depth == 64 && sumsHold(f.Stats()),
            visitText(f, w, 7));
    }
    {
        // A census that yields nothing is retried at most once a second, never
        // on every write.
        FlightWorld w;
        Flight f = flightIn(w, town01());
        w.listReadable = false;
        const bool first = !depthAt(f, w, 1, 7, 0.0).write;
        const bool soon = !depthAt(f, w, 2, 7, 500.0).write && !depthAt(f, w, 3, 7, 999.0).write;
        const long listsSoon = w.listReads;
        w.listReadable = true;
        const bool later = depthAt(f, w, 4, 7, 1000.0).write;
        check("front_layer/a_census_that_yields_nothing_retries_once_a_second",
            first && soon && later && listsSoon == 1 && w.listReads == 2 && f.Stats().noFront == 3 && f.Stats().depth == 1
                && sumsHold(f.Stats()),
            visitText(f, w, 7));
    }
    {
        // Validated where it is written: before each write the layer must
        // exist and carry our name. A layer id a new room reuses under another
        // name starts a new visit, whose census finds no layer by name and
        // creates one. A room change is a new visit too.
        FlightWorld w;
        Flight f = flightIn(w, town01());
        const DepthWrite a = depthAt(f, w, 1, 7, 0.0);
        const DepthWrite b = depthAt(f, w, 2, 7, 16.0);
        worldFlightLayer(w)->name = "Instances_Reused";
        const DepthWrite c = depthAt(f, w, 3, 7, 32.0);
        w.layers[8] = kGameEnds;
        w.rooms[8] = act01Pregame();
        const DepthWrite d = depthAt(f, w, 4, 8, 48.0);
        check("front_layer/validated_where_it_is_written_and_found_by_name",
            a.write && b.write && a.layer == b.layer && c.write && c.layer != a.layer && d.write && d.layer != c.layer
                && w.valids == 2 && w.listReads == 3 && w.creates == 3 && f.Visit().depth == -10101.0,
            visitText(f, w, 8));
    }
    // ---- never past a filter (the owner, 2026-10-13) ----
    {
        FlightWorld w;
        Flight f = flightIn(w, { { 701, -10000.0, "Invisible_walls", true, 40, false }, { 702, -9700.0, "Tree_Leaves", true, 34, true },
                                 { 703, 5.0, "Objects", true, 100, false } });
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        check("front_layer_filter/no_filter", d.write && f.Visit().depth == -10001.0 && flightDepthIn(w, 7) == -10001.0,
            visitText(f, w, 7));
    }
    {
        FlightWorld w;
        Flight f = flightIn(w, town01());
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        check("front_layer_filter/a_filter_in_front_of_every_drawn_layer",
            d.write && f.Visit().depth == -10001.0 && flightDepthIn(w, 7) > -10300.0, visitText(f, w, 7));
    }
    {
        // Act_01_Pregame: Instances_2 (-10400) is in front of the filters and
        // stays in front of him; -10101, not -10401.
        FlightWorld w;
        Flight f = flightIn(w, act01Pregame());
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/a_filter_behind_the_frontmost_drawn_layer",
            d.write && v.depth == -10101.0 && v.by >= 0 && v.layers[(size_t)v.by].name == "Raven_Creators"
                && flightDepthIn(w, 7) == -10101.0,
            visitText(f, w, 7));
    }
    {
        // Act_09_05: Smoke (-11200) sits between the filters; the rearmost
        // filter (-11000) decides, so -5501, not -11201.
        FlightWorld w;
        Flight f = flightIn(w, act0905());
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        check("front_layer_filter/several_filters_the_rearmost_decides",
            d.write && f.Visit().depth == -5501.0 && f.Visit().by == -1 && flightDepthIn(w, 7) == -5501.0, visitText(f, w, 7));
    }
    {
        // A hidden filter (which, counted, would put -10000 out of reach and
        // give -9701), and a filter behind the game layers' front end (600, as
        // Act_09_Cult_Dungeon_rm's; counted, it would give -5501): neither
        // changes the answer, and neither's effect is read.
        FlightWorld w;
        Flight f = flightIn(w, { { 801, -10000.0, "Invisible_walls", true, 40, false }, { 802, -9800.0, "Hidden_Filter", false, 0, true },
                                 { 803, -9700.0, "Tree_Leaves", true, 34, true }, { 804, 600.0, "Effect_1", true, 0, true } });
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        check("front_layer_filter/a_hidden_filter_and_one_behind_the_game_front_change_nothing",
            d.write && f.Visit().depth == -10001.0 && w.effectReads == 0 && w.detailReads == 3, visitText(f, w, 7));
    }
    {
        // Act_01_01: Tree_Leaves (-10000) carries an effect and is empty at the
        // census, so it reads as a filter: -5501. Once it holds canopies, the
        // next re-choice (not before a second) counts it as drawn and moves the
        // layer to -10001, behind the room's filter (-10100).
        FlightWorld w;
        Flight f = flightIn(w, act0101());
        const bool before = depthAt(f, w, 1, 7, 0.0).write && f.Visit().depth == -5501.0 && flightDepthIn(w, 7) == -5501.0;
        worldLayer(w, 602)->elements = 40;
        const bool notYet = depthAt(f, w, 2, 7, 500.0).write && f.Visit().depth == -5501.0 && w.moves == 0;
        const DepthWrite d = depthAt(f, w, 3, 7, 1000.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/an_empty_effect_layer_that_fills",
            before && notYet && d.write && v.depth == -10001.0 && flightDepthIn(w, 7) == -10001.0 && v.moves == 1 && w.moves == 1
                && w.listReads == 1 && v.by >= 0 && v.layers[(size_t)v.by].name == "Tree_Leaves",
            visitText(f, w, 7));
    }
    {
        // A failed layer_get_fx read is no effect: that layer is not a filter.
        // Read, the filter at -9800 would leave only -9700 behind it (-9701);
        // failed, the real filter at -10300 still decides: -10001.
        FlightWorld w;
        Flight f = flightIn(w, { { 901, -10300.0, "Effect_1", true, 0, true }, { 902, -10000.0, "Invisible_walls", true, 40, false },
                                 { 903, -9800.0, "Effect_2", true, 0, true }, { 904, -9700.0, "Tree_Leaves", true, 34, true } });
        w.effectFails.insert(903);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        FlightWorld r;
        Flight g = flightIn(r, { { 901, -10300.0, "Effect_1", true, 0, true }, { 902, -10000.0, "Invisible_walls", true, 40, false },
                                 { 903, -9800.0, "Effect_2", true, 0, true }, { 904, -9700.0, "Tree_Leaves", true, 34, true } });
        const DepthWrite e = depthAt(g, r, 1, 7, 0.0);
        check("front_layer_filter/a_failed_effect_read_is_no_filter",
            d.write && f.Visit().depth == -10001.0 && w.effectReads == 2 && e.write && g.Visit().depth == -9701.0,
            visitText(f, w, 7) + " || control " + visitText(g, r, 7));
    }
    {
        FlightWorld w;
        w.layers[7] = { LayerEnd{ 101, 100.0 }, LayerEnd{ 109, -100.0 } };
        Flight f = makeFlight(w);
        const DepthWrite off = f.OnUpdateDepth(1, true, 7);
        w.equipped = CharmBit(Charm::Wings);
        const DepthWrite on = f.OnUpdateDepth(2, true, 7);
        w.equipped = 0;
        w.overlapping = true;
        const DepthWrite residual = f.OnUpdateDepth(3, true, 7);
        const bool isResidual = f.CurrentState() == State::Residual;
        const DepthWrite other = f.OnUpdateDepth(3, false, 7);
        // Off, residual and another self run no census and create no layer:
        // only the one write while on did.
        check("front_layer/no_write_for_off_residual_or_another_self",
            !off.write && on.write && !residual.write && isResidual && !other.write && f.Stats().depth == 1
                && w.layerReads == 1 && w.listReads == 1 && w.creates == 1,
            counts(f.Stats()));
    }

    // ======================= tooltip_rows ==================================
    {
        // Baseline: an item with stats anchors at its first format-2/3 call
        // whose stat it has, never at an absent one before it, nor at a
        // present call of another format.
        const TipPass p = tipPass(2, false, { { 2.0, false }, { 1.0, true }, { 3.0, false }, { 3.0, true }, { 2.0, true } });
        check("tooltip_rows/baseline_an_item_with_stats_anchors_at_its_first_present_row_call",
            p.anchor == 3 && p.anchors == 1 && p.statReads == 3, tipText(p));
    }
    {
        // Target: an item with no stats (Pocket Elf) anchors at the first
        // format-2/3 call, reading no stat. The control is Live 1's bug: the
        // same pass read as an item with stats never anchors.
        const std::vector<TipCall> calls = { { 1.0, false }, { 0.0, false }, { 2.0, false }, { 3.0, false } };
        const TipPass p = tipPass(1, true, calls);
        const TipPass control = tipPass(1, false, calls);
        check("tooltip_rows/target_an_item_with_no_stats_anchors_at_the_first_row_call",
            p.anchor == 2 && p.anchors == 1 && p.statReads == 0 && control.anchor == -1 && control.statReads == 2,
            tipText(p) + " || control " + tipText(control));
    }
    {
        // Formats other than 2 and 3 never anchor, stat present or not.
        const std::vector<TipCall> calls = { { 0.0, true }, { 1.0, true }, { 4.0, true }, { -1.0, true }, { 2.5, true } };
        const TipPass stats = tipPass(3, false, calls);
        const TipPass none = tipPass(3, true, calls);
        bool formats = ForgePact::ForgedTooltipRows::IsAnchorFormat(2.0) && ForgePact::ForgedTooltipRows::IsAnchorFormat(3.0);
        for (const TipCall& c : calls) formats = formats && !ForgePact::ForgedTooltipRows::IsAnchorFormat(c.format);
        check("tooltip_rows/formats_other_than_2_and_3_never_anchor",
            formats && stats.anchor == -1 && stats.statReads == 0 && none.anchor == -1,
            tipText(stats) + " || no stats " + tipText(none));
    }
    {
        // A pass anchors at most once, with stats or without, and reads no
        // stat after its anchor.
        const std::vector<TipCall> calls = { { 2.0, true }, { 3.0, true }, { 2.0, true }, { 3.0, true } };
        const TipPass stats = tipPass(3, false, calls);
        const TipPass none = tipPass(3, true, calls);
        check("tooltip_rows/a_pass_anchors_at_most_once",
            stats.anchor == 0 && stats.anchors == 1 && stats.statReads == 1 && none.anchor == 0 && none.anchors == 1
                && none.statReads == 0,
            tipText(stats) + " || no stats " + tipText(none));
    }
    {
        // With no rows nothing is pending: nothing anchors and no stat is read.
        const std::vector<TipCall> calls = { { 2.0, true }, { 3.0, false }, { 3.0, true } };
        const TipPass stats = tipPass(0, false, calls);
        const TipPass none = tipPass(0, true, calls);
        check("tooltip_rows/with_no_rows_nothing_anchors",
            !ForgePact::ForgedTooltipRows::Pending(0) && ForgePact::ForgedTooltipRows::Pending(1) && stats.anchors == 0
                && stats.statReads == 0 && none.anchors == 0 && none.statReads == 0,
            tipText(stats) + " || no stats " + tipText(none));
    }

    // ======================= trace =========================================
    {
        // Every bucket the trace line prints, each driven at least once, and
        // the sums FlightCounters documents.
        FlightWorld w;
        w.layers[7] = { LayerEnd{ 101, 100.0 }, LayerEnd{ 109, -100.0 } };
        Flight f = makeFlight(w);
        // The adapter's own early returns: another self, a free answer.
        f.NoteCall(false);
        f.NoteCall(false);
        f.NoteCall(true);
        f.NoteDepthOther();
        // Off: the core still counts the player's blocked query and depth call.
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        ask(f, Hooked::PlaceMeeting, 1, false, Fence, true);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, false);
        f.OnUpdateDepth(1, true, 7);
        f.OnUpdateDepth(1, false, 7);
        // On.
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true);                              // answered
        ask(f, Hooked::PlaceMeeting, 2, true, WallParent, true);                         // excluded
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true, true);                        // excluded by the re-run
        ask(f, Hooked::PlaceMeeting, 2, true, Skeleton, true);                           // other
        ask(f, Hooked::PlaceMeeting, 2, true, -1, true);                                 // unreadable
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true, false, -1.0, 0.0);            // outside the room
        f.SetFamily(-1, { WallParent, LockObj });
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true);                              // unconfigured
        f.SetFamily(CollisionParent, { WallParent, LockObj });
        f.OnUpdateDepth(2, true, 7);                                                     // depth
        f.OnUpdateDepth(2, true, -1);                                                    // no front: room unreadable
        const FlightCounters& c = f.Stats();
        const bool every = c.notPlayer == 3 && c.unblocked == 2 && c.seen == 8 && c.idle == 1 && c.unconfigured == 1
            && c.unreadable == 1 && c.other == 1 && c.excluded == 2 && c.outsideRoom == 1 && c.answered == 1
            && c.depthOther == 2 && c.depthIdle == 1 && c.noFront == 1 && c.depth == 1;
        const bool sums = c.calls == c.notPlayer + c.unblocked + c.seen
            && c.seen == c.idle + c.unconfigured + c.unreadable + c.other + c.excluded + c.outsideRoom + c.answered
            && c.depthCalls == c.depthOther + c.depthSeen && c.depthSeen == c.depthIdle + c.noFront + c.depth;
        check("trace/the_counters_sum_as_documented", every && sums && c.calls == 13 && c.depthCalls == 5,
            TraceLine("match", c) + " | " + counts(c));
    }

    // ======================= size_guard ====================================
    // Grid 6x4. Ids: 1 a vanilla item, 2 Gabriel Wings (2x2), 3 Witch's
    // Broomstick (1x5), 4 Pocket Elf (1x1), 5 another vanilla item, 6 a second
    // Gabriel Wings (2x2).
    {
        // Baseline: unguarded, the entry order decides (Live 1's
        // `dress-added: item-missing`, read through § 22.6). The guard does
        // nothing outside a scope or for another item.
        Load first;
        first.run({ Saved{ 2, 0, 0, 2, 2, true }, Saved{ 1, 1, 1, 1, 1, false } }, false);
        Load second;
        second.run({ Saved{ 1, 1, 1, 1, 1, false }, Saved{ 2, 0, 0, 2, 2, true } }, false);
        SizeGuard<Pending> g;
        const bool outside = !g.Defer(Pending{ 2, 0, 0 }, true, 2, 2) && !g.Defer(Pending{ 3, 0, 0 }, true, 1, 5);
        g.Open();
        const bool other = !g.Defer(Pending{ 1, 0, 0 }, false, 2, 2) && !g.Defer(Pending{ 5, 0, 0 }, false, 1, 1);
        int calls = 0;
        g.Close([&calls](const Pending&) { ++calls; return true; }, [&calls](const Pending&) { ++calls; return true; });
        check("size_guard/baseline_outside_the_scope_or_for_another_item_nothing_changes",
            first.grid.at(1, 1) == 2 && first.grid.covered(1) == 0 && second.grid.at(1, 1) == 1
                && second.grid.covered(2) == 0 && outside && other && calls == 0 && g.Stats().deferred == 0,
            guardText(g.Stats()));
    }
    {
        Load first;
        first.run({ Saved{ 2, 0, 0, 2, 2, true }, Saved{ 1, 1, 1, 1, 1, false } }, true);
        Load second;
        second.run({ Saved{ 1, 1, 1, 1, 1, false }, Saved{ 2, 0, 0, 2, 2, true } }, true);
        const auto kept = [](const Load& l) {
            return l.grid.at(1, 1) == 1 && l.grid.at(0, 0) == 2 && l.grid.covered(2) == 1 && l.guard.Stats().kept == 1
                && l.guard.Stats().failed == 0 && l.guard.Stats().deferred == 1 && l.outcomes.size() == 1u
                && l.outcomes[0].first.id == 2 && l.outcomes[0].first.x == 0 && l.outcomes[0].first.y == 0
                && l.outcomes[0].second == GuardOutcome::Kept;
        };
        check("size_guard/target_deferred_to_the_end_of_the_load_whatever_the_order",
            kept(first) && kept(second), guardText(first.guard.Stats()) + " | " + guardText(second.guard.Stats()));
        check("size_guard/target_a_vanilla_item_places_first", first.grid.at(1, 1) == 1 && second.grid.at(1, 1) == 1);
        check("size_guard/target_a_refused_full_footprint_becomes_1x1_at_its_own_cell",
            first.grid.at(0, 0) == 2 && first.grid.at(1, 0) == 0 && first.grid.at(0, 1) == 0);
    }
    {
        Load l;
        l.run({ Saved{ 2, 0, 0, 2, 2, true }, Saved{ 3, 5, 0, 1, 4, true }, Saved{ 1, 3, 3, 1, 1, false } }, true);
        check("size_guard/target_a_full_footprint_that_fits_is_placed_whole",
            l.grid.covered(2) == 4 && l.grid.covered(3) == 4 && l.grid.at(3, 3) == 1 && l.guard.Stats().full == 2
                && l.guard.Stats().kept == 0 && l.guard.Stats().failed == 0,
            guardText(l.guard.Stats()));
    }
    {
        // The broom's 1x5 footprint no longer fits a 4-row grid at all.
        Load l;
        l.run({ Saved{ 3, 4, 0, 1, 5, true } }, true);
        check("size_guard/target_a_1x5_charm_is_deferred_and_kept",
            l.grid.covered(3) == 1 && l.grid.at(4, 0) == 3 && l.guard.Stats().kept == 1, guardText(l.guard.Stats()));
    }
    {
        Load l;
        l.run({ Saved{ 4, 2, 2, 1, 1, true }, Saved{ 1, 0, 0, 1, 1, false } }, true);
        check("size_guard/target_a_1x1_charm_is_never_deferred",
            l.grid.at(2, 2) == 4 && l.guard.Stats().deferred == 0 && l.outcomes.empty(), guardText(l.guard.Stats()));
    }
    {
        // A vanilla item took the charm's own cell: 1x1 is refused too.
        Load l;
        l.run({ Saved{ 1, 0, 0, 1, 1, false }, Saved{ 2, 0, 0, 2, 2, true } }, true);
        check("size_guard/target_a_refused_1x1_counts_failed",
            l.grid.at(0, 0) == 1 && l.grid.covered(2) == 0 && l.guard.Stats().failed == 1 && l.guard.Stats().kept == 0
                && l.outcomes.size() == 1u && l.outcomes[0].second == GuardOutcome::Failed,
            guardText(l.guard.Stats()));
    }
    {
        SizeGuard<Pending> g;
        std::vector<int> placed;
        const auto full = [&placed](const Pending& p) { placed.push_back(p.id); return true; };
        const auto one = [](const Pending&) { return true; };
        g.Open();
        g.Defer(Pending{ 2, 0, 0 }, true, 2, 2);
        g.Open();
        g.Defer(Pending{ 3, 0, 0 }, true, 1, 5);
        g.Defer(Pending{ 6, 0, 0 }, true, 1, 5);
        g.Close(full, one);
        const bool inner = placed == std::vector<int>{ 3, 6 } && g.Depth() == 1u;
        g.Close(full, one);
        check("size_guard/target_nested_scopes_flush_their_own_lists",
            inner && placed == std::vector<int>{ 3, 6, 2 } && g.Depth() == 0u && g.Stats().full == 3);
    }
    {
        SizeGuard<Pending> g;
        bool deferredInside = true;
        g.Open();
        g.Defer(Pending{ 2, 0, 0 }, true, 2, 2);
        g.Close([&g, &deferredInside](const Pending& p) { deferredInside = g.Defer(p, true, 2, 2); return true; },
            [](const Pending&) { return true; });
        g.Open();
        g.Defer(Pending{ 2, 0, 0 }, true, 2, 2);
        g.Defer(Pending{ 3, 0, 0 }, true, 1, 5);
        g.Close([](const Pending& p) -> bool { if (p.id == 2) throw std::runtime_error("x"); return false; },
            [](const Pending& p) -> bool { if (p.id == 3) throw std::runtime_error("y"); return true; });
        g.Close([](const Pending&) { return true; }, [](const Pending&) { return true; });   // no scope open
        check("size_guard/flushing_defers_nothing_and_a_throw_counts_failed",
            !deferredInside && g.Stats().kept == 1 && g.Stats().failed == 1 && g.Stats().full == 1 && g.Depth() == 0u,
            guardText(g.Stats()));
    }
    {
        const std::string kept = GuardLine("Gabriel Wings", 3, 2, GuardOutcome::Kept);
        const std::string failed = GuardLine("Witch's Broomstick", 0, 1, GuardOutcome::Failed);
        check("size_guard/the_log_line",
            kept == "flightcharm: guard kept Gabriel Wings at 1x1 in cell 3,2 (its full size did not fit)"
                && failed == "flightcharm: guard failed Witch's Broomstick in cell 0,1 (no room even at 1x1)"
                && GuardLine("Pocket Elf", 0, 0, GuardOutcome::Full).empty(),
            kept + " | " + failed);
    }

    // ---- two charms in one load ----
    {
        // Baseline: unguarded, the Wings saved first covers the cell of the
        // charm saved after it, which the game then refuses and drops.
        Load l;
        l.run({ Saved{ 2, 0, 0, 2, 2, true }, Saved{ 6, 1, 0, 2, 2, true } }, false);
        check("size_guard/baseline_two_charms_unguarded_the_first_covers_the_second",
            l.grid.covered(2) == 4 && l.grid.at(1, 0) == 2 && l.grid.covered(6) == 0);
    }
    {
        // Guarded, in either save order: the Wings whose full size would cover
        // the other's cell yields to 1x1, the other goes in whole.
        const auto both = [](const Load& l) {
            const GuardCounters& c = l.guard.Stats();
            return l.grid.at(0, 0) == 2 && l.grid.covered(2) == 1 && l.grid.at(1, 0) == 6 && l.grid.covered(6) == 4
                && c.deferred == 2 && c.full == 1 && c.kept == 1 && c.yielded == 1 && c.failed == 0
                && l.outcome(2, GuardOutcome::Yielded) == 1 && l.outcome(6, GuardOutcome::Full) == 1;
        };
        Load first;
        first.run({ Saved{ 2, 0, 0, 2, 2, true }, Saved{ 6, 1, 0, 2, 2, true } }, true);
        Load second;
        second.run({ Saved{ 6, 1, 0, 2, 2, true }, Saved{ 2, 0, 0, 2, 2, true } }, true);
        check("size_guard/target_two_charms_whose_footprint_covers_the_others_cell_keep_both",
            both(first) && both(second), guardText(first.guard.Stats()) + " | " + guardText(second.guard.Stats()));
    }
    {
        // The game reads the axes the other way round: the 1x5 Broomstick then
        // runs five columns along row 0, over the Wings' cell at (2,0). Its
        // transposed box is tested too, so it yields in either save order.
        const auto both = [](const Load& l) {
            const GuardCounters& c = l.guard.Stats();
            return l.grid.at(0, 0) == 3 && l.grid.covered(3) == 1 && l.grid.at(2, 0) == 6 && l.grid.covered(6) == 4
                && c.yielded == 1 && c.full == 1 && c.failed == 0;
        };
        Load first;
        first.transposed = true;
        first.run({ Saved{ 3, 0, 0, 1, 5, true }, Saved{ 6, 2, 0, 2, 2, true } }, true);
        Load second;
        second.transposed = true;
        second.run({ Saved{ 6, 2, 0, 2, 2, true }, Saved{ 3, 0, 0, 1, 5, true } }, true);
        check("size_guard/target_a_transposed_footprint_still_yields",
            both(first) && both(second), guardText(first.guard.Stats()) + " | " + guardText(second.guard.Stats()));
    }
    {
        // The adapter could not read the first Wings' cell, so the guard
        // cannot tell whether either covers the other: both go in at 1x1.
        Load l;
        l.unknownCell = { 2 };
        l.run({ Saved{ 2, 0, 0, 2, 2, true }, Saved{ 6, 1, 1, 2, 2, true } }, true);
        const GuardCounters& c = l.guard.Stats();
        check("size_guard/target_an_unknown_cell_yields_when_another_charm_is_held",
            l.grid.at(0, 0) == 2 && l.grid.covered(2) == 1 && l.grid.at(1, 1) == 6 && l.grid.covered(6) == 1
                && c.yielded == 2 && c.kept == 2 && c.full == 0 && c.failed == 0,
            guardText(c));
    }
    {
        // Alone in its load, a charm with an unknown cell is not shrunk.
        Load l;
        l.unknownCell = { 2 };
        l.run({ Saved{ 2, 0, 0, 2, 2, true }, Saved{ 1, 3, 3, 1, 1, false } }, true);
        const GuardCounters& c = l.guard.Stats();
        check("size_guard/target_one_charm_with_an_unknown_cell_is_not_shrunk",
            l.grid.covered(2) == 4 && l.grid.at(3, 3) == 1 && c.full == 1 && c.yielded == 0 && c.kept == 0,
            guardText(c));
    }

    // ---- how far a load got ----
    {
        SizeGuard<Pending> g;
        g.Saw(true);                         // outside a load: nothing
        const bool outside = g.Stats().seen == 0 && g.Stats().charms == 0 && g.Stats().opens == 0;
        g.Open();
        g.Saw(false);
        g.Saw(true);
        g.Defer(Pending{ 2, 0, 0 }, true, 2, 2);
        const bool inside = g.Stats().opens == 1 && g.Stats().seen == 2 && g.Stats().charms == 1;
        g.Close([&g](const Pending&) { g.Saw(true); return true; }, [&g](const Pending&) { g.Saw(true); return true; });
        const bool flush = g.Stats().seen == 2 && g.Stats().charms == 1;
        g.Open();
        g.Close([](const Pending&) { return true; }, [](const Pending&) { return true; });
        g.Saw(true);                         // closed again: nothing
        const GuardCounters& c = g.Stats();
        check("size_guard/saw_counts_only_inside_a_load_and_outside_a_flush",
            outside && inside && flush && c.opens == 2 && c.seen == 2 && c.charms == 1 && c.deferred == 1 && c.full == 1,
            "opens=" + std::to_string(c.opens) + " seen=" + std::to_string(c.seen) + " charms=" + std::to_string(c.charms));
    }
    {
        GuardCounters c;
        c.opens = 1;
        c.seen = 2;
        c.charms = 3;
        c.deferred = 4;
        c.full = 5;
        c.kept = 6;
        c.yielded = 7;
        c.failed = 8;
        const std::string line = GuardStatLine("on", c);
        const std::string fresh = GuardStatLine("not-installed", GuardCounters{});
        check("size_guard/the_guard_stat_line",
            line == "flightcharm guard: route=on opens=1 seen=2 charms=3 deferred=4 full=5 kept=6 yielded=7 failed=8"
                && fresh == "flightcharm guard: route=not-installed opens=0 seen=0 charms=0 deferred=0 full=0 kept=0 "
                            "yielded=0 failed=0",
            line + " | " + fresh);
    }

    // ======================= stand_in ======================================
    {
        const bool ok = kElfCandidateCount == 5 && kElfCandidates[0].name == "Aimbot.exe" && kElfCandidates[0].b == 76
            && kElfCandidates[1].name == "Witches Claw" && kElfCandidates[1].b == 41
            && kElfCandidates[2].name == "Traveler's Map Journal" && kElfCandidates[2].b == 24
            && kElfCandidates[3].name == "Crow's Feather" && kElfCandidates[3].b == 68
            && kElfCandidates[4].name == "Hello its me, Steve!" && kElfCandidates[4].b == 12
            && kWingsStandIn.name == "Water Melon" && kWingsStandIn.t == 10 && kWingsStandIn.sub == 0 && kWingsStandIn.b == 47
            && kBroomStandIn.name == "Soul Collector" && kBroomStandIn.t == 10 && kBroomStandIn.sub == 0 && kBroomStandIn.b == 91
            && StandInFor(Charm::Wings) == &kWingsStandIn && StandInFor(Charm::Broom) == &kBroomStandIn
            && StandInFor(Charm::Elf) == nullptr && kUniqueSubLists == 5 && kAngelicSubList == 5;
        bool all = true;
        for (const StandIn& s : kElfCandidates) all = all && s.t == 10 && s.sub == 0;
        check("stand_in/the_table_and_its_order", ok && all);
    }
    {
        std::array<CandidateCounts, kElfCandidateCount> c{};
        c[0] = { { 1, 1, 2, 1, 1 }, true };
        c[1] = { { 0, 1, 1, 1, 1 }, true };
        c[2] = { { 2, 2, 2, 2, 2 }, true };
        c[3] = { { 1, 1, 1, 1, 1 }, true };
        c[4] = { { 3, 3, 3, 3, 3 }, true };
        const int picked = PickElfStandIn(c);
        c[0].counts = { 4, 4, 4, 4, 4 };
        const int first = PickElfStandIn(c);
        c[0].passes = false;
        const int filtered = PickElfStandIn(c);
        check("stand_in/the_first_even_candidate_that_passes_the_filter",
            picked == 2 && first == 0 && filtered == 2 && EvenCount(ListCounts{ 2, 2, 2, 2, 2 })
                && !EvenCount(ListCounts{ 2, 2, 2, 2, 3 }) && !EvenCount(ListCounts{ -1, -1, -1, -1, -1 })
                && ElfStandInText(true, picked, c) == "map-journal",
            std::to_string(picked) + " " + std::to_string(first) + " " + std::to_string(filtered));
    }
    {
        std::array<CandidateCounts, kElfCandidateCount> c{};
        c[0] = { { 1, 1, 2, 1, 1 }, true };
        c[1] = { { 0, 1, 1, 1, 1 }, true };
        c[2] = { { 2, 2, 2, 2, 2 }, false };
        c[3] = { { 1, 2, 1, 1, 1 }, true };
        c[4] = { { 3, 3, 3, 3, 4 }, true };
        const int picked = PickElfStandIn(c);
        const std::string text = ElfStandInText(true, picked, c);
        check("stand_in/no_candidate_refuses_and_names_the_counts",
            picked == -1 && kElfNoStandIn == "elf: no stand-in with an even count across lootListUnique[0..4]"
                && text == "refused(aimbot:1/1/2/1/1,witches-claw:0/1/1/1/1,map-journal:2/2/2/2/2-filtered,crows-feather:1/2/1/1/1,steve:3/3/3/3/4)"
                && ElfStandInText(false, -1, c) == "none",
            text);
    }
    {
        const std::vector<UniqueEntry> list = { { 10, 0, 76 }, { 10, 0, 41 }, { 10, 0, 76 }, { 10, 1, 76 }, { 11, 0, 76 } };
        const bool ok = CountIn(list, kElfCandidates[0]) == 2 && CountIn(list, kElfCandidates[1]) == 1
            && CountIn(list, kElfCandidates[4]) == 0 && CountIn({}, kElfCandidates[0]) == 0
            && PassesUniqueFilter(6, false) && !PassesUniqueFilter(7, false) && !PassesUniqueFilter(6, true)
            && PassesUniqueFilter(0, false);
        check("stand_in/counting_and_the_unique_filter", ok);
    }
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const bool ok = CoinShare(1, 1) == 0.5 && CoinShare(3, 1) == 0.25 && CoinShare(0, 1) == 1.0
            && CoinShare(2, 6) == 0.75 && CoinShare(5, 0) == 0.0 && CoinShare(-1, 1) == 0.0
            && CoinOurs(3, 1, 0.24) && !CoinOurs(3, 1, 0.25) && CoinOurs(0, 1, 0.999) && !CoinOurs(5, 0, 0.0)
            && !CoinOurs(3, 1, nan) && !CoinOurs(3, 1, -0.1) && !CoinOurs(3, 1, 1.0);
        check("stand_in/the_coin_c_over_n_plus_c", ok);
    }

    // ======================= install =======================================
    {
        Flight f;
        const bool notArmed = f.InstallStateName() == "not-armed" && !f.ShouldInstall(true, true) && !f.Armed();
        f.Arm();
        const bool waiting = f.InstallStateName() == "waiting-for-character" && !f.ShouldInstall(false, true)
            && !f.ShouldInstall(true, false) && f.ShouldInstall(true, true);
        f.NoteInstall(true);
        f.Arm();
        const bool installed = f.InstallStateName() == "installed" && f.Installed() && !f.ShouldInstall(true, true);
        check("install/armed_by_a_charm_installed_once_a_character_exists", notArmed && waiting && installed);
    }
    {
        Flight f;
        f.Arm();
        f.NoteInstall(false);
        f.Arm();
        check("install/a_refusal_is_final_for_the_session",
            f.InstallStateName() == "refused" && !f.ShouldInstall(true, true) && !f.Installed()
                && RefusedLine("InstancePlaceTallest came back table-only")
                    == "flightcharm: refused - InstancePlaceTallest came back table-only");
    }
    {
        check("install/looks_for_the_player_every_60_frames",
            Flight::kInstallPollFrames == 60 && Flight::LooksForPlayer(0) && !Flight::LooksForPlayer(1)
                && !Flight::LooksForPlayer(59) && Flight::LooksForPlayer(60) && Flight::LooksForPlayer(120));
    }

    // ======================= command =======================================
    {
        Command c;
        std::string why;
        bool ok = ParseCommand({}, c, why) && c.form == CommandForm::Stat
            && ParseCommand({ "stat" }, c, why) && c.form == CommandForm::Stat
            && ParseCommand({ "elf", "1" }, c, why) && c.form == CommandForm::Switch && c.charm == Charm::Elf && c.on
            && ParseCommand({ "wings", "0" }, c, why) && c.charm == Charm::Wings && !c.on
            && ParseCommand({ "broom", "1" }, c, why) && c.charm == Charm::Broom && c.on;
        check("command/accepted_forms", ok);
    }
    {
        bool ok = true;
        std::string all;
        const std::vector<std::vector<std::string>> bad = {
            { "elf" }, { "elf", "2" }, { "dragon", "1" }, { "stat", "x" }, { "elf", "1", "x" }, { "ELF", "1" }, { "1" },
        };
        for (const auto& args : bad) {
            Command c;
            std::string why;
            const bool parsed = ParseCommand(args, c, why);
            ok = ok && !parsed && has(why, std::string(kUsage));
            all += " [" + why + "]";
        }
        check("command/refused_forms_name_the_usage",
            ok && kUsage == "usage: flightcharm elf|wings|broom 1|0 | flightcharm stat", all);
    }

    // ======================= stat_line =====================================
    {
        const std::string fresh = "flightcharm: switches elf=off wings=off broom=off | flight install=not-armed state=off "
                                  "equipped=none answered=0 excluded=0 outside-room=0 depth=0 | icons=not-loaded ground=0 | "
                                  "guard kept=0 failed=0 | drops gate=off elf=0/0 wings=0/0 broom=0/0 injected=0 refused=0 "
                                  "anomalies=0 standin-elf=none";
        Flight f;
        SizeGuard<Pending> g;
        StatFields s;
        f.FillStat(s);
        g.FillStat(s);
        const std::string built = StatLine(s);
        check("stat_line/the_fresh_session_line_byte_for_byte", StatLine(StatFields{}) == fresh && built == fresh, built);
    }
    {
        FlightWorld w;
        w.layers[7] = { LayerEnd{ 101, 100.0 }, LayerEnd{ 109, -100.0 } };
        Flight f = makeFlight(w);
        f.Arm();
        f.NoteInstall(true);
        w.equipped = CharmBit(Charm::Broom) | CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        ask(f, Hooked::PlaceMeeting, 1, true, GateChild, true);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true, false, -1.0, 0.0);
        f.OnUpdateDepth(1, true, 7);
        Load l;
        l.run({ Saved{ 1, 1, 1, 1, 1, false }, Saved{ 2, 0, 0, 2, 2, true } }, true);
        StatFields s;
        f.FillStat(s);
        l.guard.FillStat(s);
        s.switches[0] = true;
        s.switches[2] = true;
        s.icons = "loaded";
        s.ground = 3;
        s.gate = Gate::On;
        s.drops[0] = { 1, 2 };
        s.drops[2] = { 4, 5 };
        s.injected = 12;
        s.refused = 1;
        s.anomalies = 0;
        s.standinElf = "aimbot";
        const std::string line = StatLine(s);
        const bool ok = line == "flightcharm: switches elf=on wings=off broom=on | flight install=installed state=on "
                                "equipped=wings+broom answered=1 excluded=1 outside-room=1 depth=1 | icons=loaded ground=3 | "
                                "guard kept=1 failed=0 | drops gate=on elf=1/2 wings=0/0 broom=4/5 injected=12 refused=1 "
                                "anomalies=0 standin-elf=aimbot";
        const bool gates = GateName(Gate::Off) == "off" && GateName(Gate::On) == "on" && GateName(Gate::Refused) == "refused"
            && GateName(Gate::NotResolved) == "not-resolved" && GateName(Gate::NoDetection) == "no-detection";
        const bool equipped = EquippedText(0) == "none" && EquippedText(kAllCharmBits) == "elf+wings+broom"
            && EquippedText(CharmBit(Charm::Broom)) == "broom" && StateName(State::Residual) == "residual"
            && StateName(State::On) == "on" && StateName(State::Off) == "off";
        check("stat_line/fields_after_use_and_their_names", ok && gates && equipped, line);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
