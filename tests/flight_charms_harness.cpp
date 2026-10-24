// Behavioral regression harness for the flight charms' decision core
// (FlightCharms.hpp, `flightcharm`, ForgePact #158 phase 2).
//
// The Python runner injects the REAL ForgePact::FlightCharms header below. No
// game is touched. The controlled world is an object table with parents
// (object_is_ancestor answers from it the way the runtime does - an object is
// not its own ancestor), the charms the local player wears, whether the
// player overlaps the Collision_Parent_obj family or a monster, a room size, each room's
// two game-layer ends and its own layers (with the flight layer once placed),
// a millisecond clock, and a small inventory grid for the size guard. Frames
// are plain numbers.
//
// seed_codec: the item table, every roll's seed and back, every range edge
// and one past it, and t, b, c or j off by one (not a flight charm).
// exclusion: with no charm worn every query keeps its real answer (baseline);
// with one worn a blocked family query is answered, while a wall, a lock, a
// re-run that a wall blocks, a monster on any row but place_meeting and a
// point outside the room keep the real answer, a free answer stands and
// another self is never touched (target); InstancePlaceTallest's family result
// becomes noone, a wall result stays. residual: a charm taken off inside a
// prop keeps flight on until the first free read; outside a prop it goes
// straight to off. monster (the owner's scope, 2026-10-13): with no charm worn
// a monster on place_meeting keeps its answer (baseline); worn, it is answered
// false unless a wall or lock re-run or the room's edge holds it, every other
// row keeps it (monster-kept=), Enemy_Only_Passage_obj and Player_obj are not
// monsters, and residual holds while only a monster overlaps. size_guard: outside
// a load scope or for another item nothing changes (baseline); a dressed 2x2
// or 1x5 charm waits for the end of its grid's load, a vanilla item takes its
// cells first, a refused full footprint becomes 1x1 at its own cell (kept) and
// a refused 1x1 counts failed; nested scopes flush their own lists (target).
// front_layer: the flight layer one in front of the lowest of the game layers'
// front end and every drawn room layer, a census once per room visit and a
// re-choice at most once a second, validated where it is written, no write for
// off, residual or another self; front_layer_filter: never past a filter, the
// rearmost filter deciding (the room stacks are the data file's), the layer
// strictly behind it and never at its depth (no whole depth between them is
// no write, at the census and at a re-choice alike), and
// census_outcomes: every listed layer in exactly one of the census's outcomes,
// rendered by the core's own text. tooltip_rows:
// a forged item's gold rows anchor at its first present format-2/3 stat call
// (baseline), or at its first format-2/3 call when it has no stats at all
// (target, Pocket Elf), at most once a pass and never with no rows
// (ForgedTooltipRows.hpp). stand_in: the even-count rule, its order, its refusal, the
// coin. elf_draw (#166): Pocket Elf's kill route on DropItem's own draw - with
// the switch off, or inside a roll already in progress, a call pushes nothing
// (baseline); on, the SS-only stand-in over lootListUnique[0..5], the coin at
// the call's recorded tier, no tier refusing the hit, two hits settled apart
// and a nested roll setting the entries and the hit aside (target).
// stat_line: the fresh session's line, byte for byte. install and
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
// Block_obj, which is not a wall. The monsters (RDM § 19.9): Enemy_Parent_obj
// is a child of Avoidable_Parent_obj, the training dummy descends from it
// through Enemy_Child_Basic_obj and a boss part through
// Enemy_Child_Destructible_obj. Player_obj is a child of Enemy_Aggroable_obj,
// which has no parent, and so has Enemy_Only_Passage_obj: neither is a monster.
// Skeleton, Fence, Cart and the two *Child entries stand for any descendant.
enum Obj : int {
    AvoidableParent = 433, CollisionParent = 957, CollisionProp = 959, Fence = 5001, Cart = 5002,
    WallParent = 5701, InvisibleWall = 2269, GateParent = 1881, GateChild = 5003, LockObj = 2509,
    LockChild = 5004, BlockObj = 609, BlockDynamic = 608, EnemyAggroable = 1401, EnemyParent = 1429,
    EnemyChildBasic = 1406, EnemyChildDestructible = 1408, AttackDummy = 364, OdinPillar = 3288,
    EnemyOnlyPassage = 1428, PlayerObj = 3553, Skeleton = 5005,
};
static const std::map<int, int> kParent = {
    { CollisionParent, AvoidableParent }, { CollisionProp, CollisionParent }, { Fence, CollisionProp },
    { Cart, CollisionProp }, { WallParent, CollisionParent }, { InvisibleWall, WallParent },
    { GateParent, WallParent }, { GateChild, GateParent }, { LockObj, CollisionParent }, { LockChild, LockObj },
    { BlockObj, CollisionParent }, { BlockDynamic, BlockObj }, { EnemyParent, AvoidableParent },
    { EnemyChildBasic, EnemyParent }, { AttackDummy, EnemyChildBasic }, { Skeleton, EnemyChildBasic },
    { EnemyChildDestructible, EnemyParent }, { OdinPillar, EnemyChildDestructible }, { PlayerObj, EnemyAggroable },
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
    bool overlappingMonster = false;  // the player stands inside a monster (Enemy_Parent_obj's family)
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
    std::set<int64_t> detailFails;                      // layers whose name, visibility or elements read fails
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
    f.SetMonster(EnemyParent);
    f.SetEquipped([&w](unsigned& mask) {
        ++w.equippedReads;
        if (!w.equippedReadable) return false;
        mask = w.equipped;
        return true;
    });
    // The adapter's one overlap read: the family, then Enemy_Parent_obj.
    f.SetOverlap([&w](bool& inside) {
        ++w.overlapReads;
        if (!w.overlapReadable) return false;
        inside = w.overlapping || w.overlappingMonster;
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
        if (!p || w.detailFails.count(id)) return false;
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
    ops.count = [&w](int64_t id, bool& visible, int64_t& elements) {
        ++w.countReads;
        const WorldLayer* p = worldLayer(w, id);
        if (!p) return false;
        visible = p->visible;
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

// The census's nine outcomes summed, written out here rather than asked of the
// core: every listed layer lands in exactly one, so this equals `listed`.
static uint64_t tallySum(const CensusTally& t)
{
    return t.fxStruct + t.fxOther + t.fxFailed + t.behind + t.noDepth + t.noDetail + t.own + t.hidden + t.holds;
}

// The visit's rearmost filter as `name@depth`, or `none`, and its tally's text.
static std::string censusText(const Flight& f, FlightWorld& w)
{
    const FlightLayerVisit& v = f.Visit();
    const int at = RearmostFilter(v.gameFront, v.layers);
    const std::string filter = at >= 0 ? v.layers[(size_t)at].name + "@" + std::to_string((long long)v.layers[(size_t)at].depth)
                                       : std::string("none");
    return "filter=" + filter + " " + CensusTallyText(v.tally) + " listed=" + std::to_string(v.listed)
        + " sum=" + std::to_string(tallySum(v.tally)) + " | " + visitText(f, w, w.room);
}

static bool filterIs(const Flight& f, std::string_view name, double depth)
{
    const FlightLayerVisit& v = f.Visit();
    const int at = RearmostFilter(v.gameFront, v.layers);
    return at >= 0 && v.layers[(size_t)at].name == name && v.layers[(size_t)at].depth == depth;
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

// A grid as InventoryGridAddItemPos fills it (RDM § 23.6, static reading): the
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

// One load of a saved grid, as § 23.6 reads ParseItemToGrid: each entry is
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
        // The owner's scope (2026-10-13) answers a monster on place_meeting
        // (the monster/ scenarios); every other row keeps a monster's answer,
        // and an object that is not a monster keeps it on every row.
        for (int i = 0; i < kHookedCount; ++i) {
            const Hooked row = static_cast<Hooked>(i);
            if (row != Hooked::PlaceMeeting)
                for (int o : { Skeleton, EnemyParent }) ok = ok && ask(f, row, 10, true, o, true) == Answer::Real;
            for (int o : { EnemyAggroable, AvoidableParent }) ok = ok && ask(f, row, 10, true, o, true) == Answer::Real;
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

    // ======================= monster (the owner's scope, 2026-10-13) =======
    // A flying player walks through monsters: a blocked place_meeting of the
    // local player naming Enemy_Parent_obj or a descendant is answered "nothing
    // met", walls, locks and the room's edge still decide, and every other row
    // keeps a monster's answer.
    {
        // Baseline: no charm worn, a monster on place_meeting keeps its answer.
        FlightWorld w;
        Flight f = makeFlight(w);
        g_WallAsks = 0;
        bool ok = true;
        for (int frame = 1; frame <= 3; ++frame)
            for (int o : { EnemyParent, Skeleton, AttackDummy, OdinPillar })
                ok = ok && ask(f, Hooked::PlaceMeeting, frame, true, o, true) == Answer::Real;
        const FlightCounters& c = f.Stats();
        check("monster/baseline_flight_off_a_monster_on_place_meeting_keeps_its_answer",
            ok && f.CurrentState() == State::Off && g_WallAsks == 0 && w.roomReads == 0 && c.idle == 12
                && c.monster == 0 && c.monsterKept == 0 && same(c, 0, 0, 0, 0),
            TraceLine("match", c));
    }
    {
        // Target: worn, Enemy_Parent_obj and its descendants (a basic monster,
        // the training dummy, a boss part) are answered false on place_meeting,
        // each after the wall and lock re-run, and count in monster=.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = true;
        for (int o : { EnemyParent, Skeleton, AttackDummy, OdinPillar })
            ok = ok && ask(f, Hooked::PlaceMeeting, 10, true, o, true) == Answer::False;
        const FlightCounters& c = f.Stats();
        check("monster/target_flight_on_a_monster_on_place_meeting_is_answered_false",
            ok && g_WallAsks == 4 && c.monster == 4 && c.monsterKept == 0 && c.other == 0 && same(c, 4, 0, 0, 0),
            TraceLine("match", c));
    }
    {
        // Residual answers monsters too, as it answers the family.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Broom);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        w.equipped = 0;
        w.overlapping = true;
        const bool ok = ask(f, Hooked::PlaceMeeting, 2, true, AttackDummy, true) == Answer::False
            && f.CurrentState() == State::Residual && f.Stats().monster == 1;
        check("monster/target_residual_answers_a_monster_too", ok, TraceLine("match", f.Stats()));
    }
    {
        // A monster query whose re-run meets a wall or a lock keeps its answer.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Elf);
        g_WallAsks = 0;
        const bool ok = ask(f, Hooked::PlaceMeeting, 10, true, Skeleton, true, true) == Answer::Real
            && ask(f, Hooked::PlaceMeeting, 10, true, AttackDummy, true, true) == Answer::Real;
        const FlightCounters& c = f.Stats();
        check("monster/target_a_monster_query_whose_rerun_meets_a_wall_or_lock_keeps_its_answer",
            ok && g_WallAsks == 2 && c.monster == 0 && c.monsterKept == 0 && same(c, 0, 2, 0, 0), TraceLine("match", c));
    }
    {
        // A monster query whose point is outside the room, or in a room that
        // cannot be read, keeps its answer, and asks no re-run.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = ask(f, Hooked::PlaceMeeting, 10, true, Skeleton, true, false, -1.0, 100.0) == Answer::Real
            && ask(f, Hooked::PlaceMeeting, 10, true, Skeleton, true, false, 100.0, 1500.0) == Answer::Real
            && ask(f, Hooked::PlaceMeeting, 10, true, EnemyParent, true, false, std::numeric_limits<double>::quiet_NaN(), 100.0) == Answer::Real;
        w.roomReadable = false;
        ok = ok && ask(f, Hooked::PlaceMeeting, 10, true, AttackDummy, true) == Answer::Real;
        const FlightCounters& c = f.Stats();
        check("monster/target_a_monster_query_outside_the_room_keeps_its_answer",
            ok && g_WallAsks == 0 && c.monster == 0 && c.monsterKept == 0 && same(c, 0, 0, 4, 0), TraceLine("match", c));
    }
    {
        // Every other row keeps a monster's answer: position_meeting,
        // instance_position, collision_line, collision_circle, and an
        // InstancePlaceTallest answer whose instance is a monster. Each is a
        // monster kept (monster-kept=, inside other=), with nothing re-run or read.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = true;
        for (Hooked row : { Hooked::PositionMeeting, Hooked::InstancePosition, Hooked::CollisionLine,
                            Hooked::CollisionCircle, Hooked::InstancePlaceTallest })
            ok = ok && ask(f, row, 10, true, AttackDummy, true) == Answer::Real;
        const FlightCounters& c = f.Stats();
        check("monster/target_a_monster_on_another_row_keeps_its_answer_and_counts_monster_kept",
            ok && g_WallAsks == 0 && w.roomReads == 0 && c.monsterKept == 5 && c.other == 5 && c.monster == 0
                && same(c, 0, 0, 0, 0),
            TraceLine("match", c));
    }
    {
        // Not monsters: Enemy_Only_Passage_obj (no parent) and Player_obj (under
        // Enemy_Aggroable_obj) keep their answer and count as other, not as a
        // monster kept. Another self is never touched, monster or not.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        g_WallAsks = 0;
        bool ok = true;
        for (int o : { EnemyOnlyPassage, PlayerObj, EnemyAggroable })
            ok = ok && ask(f, Hooked::PlaceMeeting, 10, true, o, true) == Answer::Real;
        const FlightCounters mid = f.Stats();
        for (int o : { EnemyParent, Skeleton, AttackDummy })
            ok = ok && ask(f, Hooked::PlaceMeeting, 11, false, o, true) == Answer::Real;
        const FlightCounters& c = f.Stats();
        check("monster/target_enemy_only_passage_the_player_and_another_self_keep_theirs",
            ok && g_WallAsks == 0 && mid.other == 3 && mid.monsterKept == 0 && c.notPlayer == 3 && c.monster == 0
                && c.monsterKept == 0 && same(c, 0, 0, 0, 0),
            TraceLine("match", c));
    }
    {
        // A monster index that did not resolve (-1) answers for nothing: the
        // monster is then just another object, while the family still answers.
        FlightWorld w;
        Flight f = makeFlight(w);
        f.SetMonster(-1);
        w.equipped = CharmBit(Charm::Wings);
        const bool off = ask(f, Hooked::PlaceMeeting, 1, true, Skeleton, true) == Answer::Real
            && ask(f, Hooked::PlaceMeeting, 1, true, Fence, true) == Answer::False;
        f.SetMonster(EnemyParent);
        const bool back = ask(f, Hooked::PlaceMeeting, 2, true, Skeleton, true) == Answer::False;
        const FlightCounters& c = f.Stats();
        check("monster/an_unresolved_monster_answers_for_nothing",
            off && back && c.other == 1 && c.monster == 1 && c.monsterKept == 0 && c.answered == 2, TraceLine("match", c));
    }
    {
        // Residual: the charm taken off while the player stands only inside a
        // monster (a training dummy never moves away) keeps flight answering
        // until both overlap reads, the family's and the monster's, are free.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 1, true, AttackDummy, true);
        w.equipped = 0;
        w.overlapping = false;
        w.overlappingMonster = true;
        const bool held = ask(f, Hooked::PlaceMeeting, 2, true, AttackDummy, true) == Answer::False
            && f.CurrentState() == State::Residual;
        const bool stillHeld = ask(f, Hooked::PlaceMeeting, 3, true, Fence, true) == Answer::False
            && f.CurrentState() == State::Residual;
        w.overlapping = true;
        w.overlappingMonster = false;
        const bool familyOnly = ask(f, Hooked::PlaceMeeting, 4, true, AttackDummy, true) == Answer::False
            && f.CurrentState() == State::Residual;
        w.overlapping = false;
        const bool freed = ask(f, Hooked::PlaceMeeting, 5, true, AttackDummy, true) == Answer::Real
            && f.CurrentState() == State::Off;
        // The control: taken off outside every monster and prop goes straight to off.
        FlightWorld v;
        Flight g = makeFlight(v);
        v.equipped = CharmBit(Charm::Wings);
        ask(g, Hooked::PlaceMeeting, 1, true, AttackDummy, true);
        v.equipped = 0;
        const bool control = ask(g, Hooked::PlaceMeeting, 2, true, AttackDummy, true) == Answer::Real
            && g.CurrentState() == State::Off;
        check("monster/residual_holds_while_only_a_monster_overlaps_and_ends_when_both_reads_are_free",
            held && stillHeld && familyOnly && freed && control && w.overlapReads == 4 && f.Stats().monster == 3,
            "state=" + std::string(StateName(f.CurrentState())) + " overlap-reads=" + std::to_string(w.overlapReads)
                + " " + TraceLine("match", f.Stats()));
    }
    {
        // The trace line ends with monster= and monster-kept=; the stat line's
        // answered= carries the monster answers, its fresh text unchanged.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        ask(f, Hooked::PlaceMeeting, 1, true, Skeleton, true);
        ask(f, Hooked::PlaceMeeting, 1, true, Fence, true);
        ask(f, Hooked::CollisionLine, 1, true, Skeleton, true);
        const FlightCounters& c = f.Stats();
        const std::string line = TraceLine("match", c);
        const std::string tail = " fx-failed=0 monster=1 monster-kept=1";
        const bool ends = line.size() >= tail.size() && line.compare(line.size() - tail.size(), tail.size(), tail) == 0;
        StatFields s;
        f.FillStat(s);
        check("monster/the_trace_line_ends_with_monster_and_monster_kept",
            ends && has(StatLine(s), " answered=2 excluded=0 outside-room=0 depth=0 ") && c.other == 1,
            line + " | " + StatLine(s));
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
    // ---- strictly behind the rearmost filter, never at its depth ----
    {
        // A filter at the game layers' front end minus one (-5501) with nothing
        // drawn behind it: one below the front end is the filter's own depth,
        // which would draw in an order that ignores depth. No whole depth lies
        // between them, so no layer: nothing placed or written, no-front=.
        FlightWorld w;
        Flight f = flightIn(w, { { 1001, -10000.0, "Invisible_walls", true, 40, false }, { 1002, -5501.0, "Effect_1", true, 0, true } });
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        check("front_layer_filter/a_filter_at_the_game_front_minus_one_writes_no_layer",
            !d.write && !f.Visit().have && w.places == 0 && w.creates == 0 && f.Stats().noFront == 1 && f.Stats().depth == 0
                && sumsHold(f.Stats()),
            visitText(f, w, 7));
    }
    {
        // A drawn layer one behind the filter (-10099 behind -10100): one below
        // it is the filter's depth again. No layer.
        FlightWorld w;
        Flight f = flightIn(w, { { 1101, -10100.0, "Effect_1", true, 0, true }, { 1102, -10099.0, "Raven_Creators", true, 3, false } });
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        check("front_layer_filter/a_drawn_layer_one_behind_the_filter_writes_no_layer",
            !d.write && !f.Visit().have && w.places == 0 && f.Stats().noFront == 1 && f.Stats().depth == 0 && sumsHold(f.Stats()),
            visitText(f, w, 7));
    }
    {
        // A gap of two, both ways: a drawn layer two behind the filter
        // (-10098 behind -10100) gives -10099, and a filter two in front of the
        // game layers' front end (-5502) with nothing behind it gives -5501.
        FlightWorld w;
        Flight f = flightIn(w, { { 1201, -10100.0, "Effect_1", true, 0, true }, { 1202, -10098.0, "Raven_Creators", true, 3, false } });
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        const bool drawn = d.write && v.depth == -10099.0 && v.by >= 0 && v.layers[(size_t)v.by].name == "Raven_Creators"
            && flightDepthIn(w, 7) == -10099.0;
        const std::string drawnText = visitText(f, w, 7);
        FlightWorld e;
        Flight g = flightIn(e, { { 1301, -10000.0, "Invisible_walls", true, 40, false }, { 1302, -5502.0, "Effect_1", true, 0, true } });
        const DepthWrite h = depthAt(g, e, 1, 7, 0.0);
        check("front_layer_filter/a_gap_of_two_puts_the_layer_strictly_between",
            drawn && h.write && g.Visit().depth == -5501.0 && g.Visit().by == -1 && flightDepthIn(e, 7) == -5501.0,
            drawnText + " || " + visitText(g, e, 7));
    }
    {
        // A re-choice that yields no layer writes nothing either, and leaves the
        // layer where it was: Tree_Leaves (-10001) is drawn at the census, so
        // -10002 behind Effect_1 (-10100); once it empties it is the rearmost
        // filter, Smoke (-10000) one behind it, and no whole depth lies between.
        // No write until a re-choice finds one again (Smoke empties: -5501).
        FlightWorld w;
        Flight f = flightIn(w, { { 1401, -10100.0, "Effect_1", true, 0, true }, { 1402, -10001.0, "Tree_Leaves", true, 34, true },
                                 { 1403, -10000.0, "Smoke", true, 6, false } });
        const bool before = depthAt(f, w, 1, 7, 0.0).write && f.Visit().depth == -10002.0;
        worldLayer(w, 1402)->elements = 0;
        const DepthWrite none = depthAt(f, w, 2, 7, 1000.0);
        const DepthWrite still = depthAt(f, w, 3, 7, 1500.0);
        const bool held = !none.write && !still.write && f.Visit().have && f.Visit().depth == -10002.0
            && flightDepthIn(w, 7) == -10002.0 && w.moves == 0 && f.Stats().noFront == 2;
        worldLayer(w, 1403)->elements = 0;
        const DepthWrite again = depthAt(f, w, 4, 7, 2000.0);
        check("front_layer_filter/a_rechoice_that_yields_no_layer_writes_nothing",
            before && held && again.write && f.Visit().depth == -5501.0 && flightDepthIn(w, 7) == -5501.0 && w.moves == 1
                && f.Stats().depth == 2 && sumsHold(f.Stats()),
            visitText(f, w, 7));
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
        // Each effect read lands in exactly one of three counts: a struct (an
        // effect), another answer (no effect), a read that failed (no effect).
        // Only a visible empty layer is read.
        FlightWorld w;
        Flight f = flightIn(w, { { 951, -10300.0, "Effect_1", true, 0, true }, { 952, -10200.0, "Empty_Instances", true, 0, false },
                                 { 953, -10100.0, "Effect_2", true, 0, true }, { 954, -10000.0, "Invisible_walls", true, 40, false } });
        w.effectFails.insert(953);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightCounters& c = f.Stats();
        check("front_layer_filter/each_effect_read_counted_struct_other_or_failed",
            d.write && w.effectReads == 3 && c.fxStruct == 1 && c.fxOther == 1 && c.fxFailed == 1
                && c.fxStruct + c.fxOther + c.fxFailed == (uint64_t)w.effectReads && f.Visit().depth == -10001.0,
            visitText(f, w, 7) + " fx=" + std::to_string(c.fxStruct) + "/" + std::to_string(c.fxOther) + "/" + std::to_string(c.fxFailed));
    }
    {
        // Every read failing in a room that has filters (Act_09_05): the census
        // reports no filter, so Smoke counts (-11201), and `failed` equals the
        // reads, never a silent zero. Read, the same room gives -5501.
        FlightWorld w;
        Flight f = flightIn(w, act0905());
        for (int64_t id : { 501, 503, 504 }) w.effectFails.insert(id);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        const FlightCounters& c = f.Stats();
        check("front_layer_filter/every_read_failing_reports_no_filter_and_counts_failed",
            d.write && RearmostFilter(v.gameFront, v.layers) == -1 && v.depth == -11201.0 && w.effectReads == 3
                && c.fxFailed == 3 && c.fxStruct == 0 && c.fxOther == 0,
            visitText(f, w, 7) + " fx=" + std::to_string(c.fxStruct) + "/" + std::to_string(c.fxOther) + "/" + std::to_string(c.fxFailed));
    }
    {
        // The rearmost filter in force is reported, after any re-choice:
        // Act_01_01 names Tree_Leaves@-10000 at the census, then Effect_1@-10100
        // once Tree_Leaves holds canopies. The effect is not read again.
        FlightWorld w;
        Flight f = flightIn(w, act0101());
        depthAt(f, w, 1, 7, 0.0);
        const int before = RearmostFilter(f.Visit().gameFront, f.Visit().layers);
        const bool first = before >= 0 && f.Visit().layers[(size_t)before].name == "Tree_Leaves"
            && f.Visit().layers[(size_t)before].depth == -10000.0;
        worldLayer(w, 602)->elements = 40;
        depthAt(f, w, 2, 7, 1000.0);
        const FlightLayerVisit& v = f.Visit();
        const int after = RearmostFilter(v.gameFront, v.layers);
        check("front_layer_filter/the_rearmost_filter_is_reported",
            first && after >= 0 && v.layers[(size_t)after].name == "Effect_1" && v.layers[(size_t)after].depth == -10100.0
                && w.effectReads == 2 && f.Stats().fxStruct == 2,
            visitText(f, w, 7) + " before=" + std::to_string(before) + " after=" + std::to_string(after));
    }
    {
        // PR #214 review: an effect-carrying layer that holds instances at the
        // census (Tree_Leaves) is drawn, and its effect is not read. When it
        // empties it is a filter, so the next re-choice reads its effect once
        // and moves the flight layer behind it: -5501, not -10001 in front of
        // it. Read once only: later re-choices do not read it again.
        FlightWorld w;
        Flight f = flightIn(w, { { 1101, -10000.0, "Invisible_walls", true, 40, false }, { 1102, -9700.0, "Tree_Leaves", true, 34, true },
                                 { 1103, 5.0, "Objects", true, 100, false } });
        const bool before = depthAt(f, w, 1, 7, 0.0).write && f.Visit().depth == -10001.0 && w.effectReads == 0;
        worldLayer(w, 1102)->elements = 0;
        const DepthWrite d = depthAt(f, w, 2, 7, 1000.0);
        const FlightLayerVisit& v = f.Visit();
        const int filter = RearmostFilter(v.gameFront, v.layers);
        const bool behind = d.write && v.depth == -5501.0 && flightDepthIn(w, 7) == -5501.0 && v.moves == 1 && filter >= 0
            && v.layers[(size_t)filter].name == "Tree_Leaves" && w.effectReads == 1 && f.Stats().fxStruct == 1;
        depthAt(f, w, 3, 7, 2000.0);
        check("front_layer_filter/an_effect_layer_that_empties_is_a_filter",
            before && behind && w.effectReads == 1 && f.Visit().depth == -5501.0, visitText(f, w, 7));
    }
    {
        // A filter hidden at the census and shown later: the re-choice reads
        // each remembered layer's visibility with its count, so the shown
        // filter (-9800) is counted and the flight layer goes behind it.
        FlightWorld w;
        Flight f = flightIn(w, { { 1201, -10000.0, "Invisible_walls", true, 40, false }, { 1202, -9800.0, "Effect_2", false, 0, true },
                                 { 1203, 5.0, "Objects", true, 100, false } });
        const bool before = depthAt(f, w, 1, 7, 0.0).write && f.Visit().depth == -10001.0 && w.effectReads == 0;
        worldLayer(w, 1202)->visible = true;
        const DepthWrite d = depthAt(f, w, 2, 7, 1000.0);
        const double shown = f.Visit().depth, shownWorld = flightDepthIn(w, 7);
        // And hidden again: no longer a filter, so the layer goes back in front.
        worldLayer(w, 1202)->visible = false;
        const DepthWrite e = depthAt(f, w, 3, 7, 2000.0);
        check("front_layer_filter/a_filter_shown_after_the_census_is_counted",
            before && d.write && shown == -5501.0 && shownWorld == -5501.0 && e.write && w.effectReads == 1
                && f.Stats().fxStruct == 1 && f.Visit().depth == -10001.0 && f.Visit().moves == 2,
            visitText(f, w, 7) + " shown=" + std::to_string(shown));
    }
    // ---- census outcomes: every listed layer in exactly one -------------------
    // `flightprobe depth` prints the core's rendering of the visit's tally, so a
    // `filter=none` names its cause: an effect read that answered something other
    // than a struct, or failed, or a layer whose effect was never read, and why.
    const std::string kEmptyTally = "fx=0/0/0 unread=behind:0,no-depth:0,no-detail:0,own:0,hidden:0,holds:0";
    {
        // Town_01_rm: Effect_1 (-10300) is the one layer read, and it answers a
        // struct. Wind_Leaves and Xmas_Event_Assets_1 are hidden, four hold
        // elements, and three lie behind the game layers' front end.
        FlightWorld w;
        Flight f = flightIn(w, town01());
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/census_outcomes_town_effect_1_is_read_as_a_struct",
            d.write && filterIs(f, "Effect_1", -10300.0)
                && CensusTallyText(v.tally) == "fx=1/0/0 unread=behind:3,no-depth:0,no-detail:0,own:0,hidden:2,holds:4"
                && v.listed == 10 && tallySum(v.tally) == v.listed && w.effectReads == 1,
            censusText(f, w));
    }
    {
        // One layer of each of the nine outcomes, tested in the core's order.
        FlightWorld w;
        Flight f = flightIn(w, {
            { 1001, std::numeric_limits<double>::quiet_NaN(), "Odd", true, 0, true },   // no-depth
            { 1002, 600.0, "Effect_Behind", true, 0, true },                           // behind
            { 1003, -10400.0, "Unreadable", true, 0, true },                           // no-detail
            { 1004, -12000.0, std::string(kFlightLayerName), true, 1, false },          // own
            { 1005, -10300.0, "Hidden_Filter", false, 0, true },                       // hidden
            { 1006, -10000.0, "Invisible_walls", true, 40, false },                    // holds
            { 1007, -10200.0, "Effect_1", true, 0, true },                             // struct
            { 1008, -10100.0, "Empty_Instances", true, 0, false },                     // other
            { 1009, -9900.0, "Effect_2", true, 0, true },                              // failed
        });
        w.detailFails.insert(1003);
        w.effectFails.insert(1009);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/census_outcomes_every_listed_layer_lands_in_one",
            d.write && CensusTallyText(v.tally) == "fx=1/1/1 unread=behind:1,no-depth:1,no-detail:1,own:1,hidden:1,holds:1"
                && v.listed == 9 && tallySum(v.tally) == v.listed && w.effectReads == 3 && filterIs(f, "Effect_1", -10200.0)
                && v.depth == -10001.0 && w.creates == 0,
            censusText(f, w));
    }
    {
        // A hidden filter is never read: `filter=none` with fx=0/0/0, which is
        // not an answer of another kind (`other`), and not a failed read.
        FlightWorld w;
        std::vector<WorldLayer> stack = town01();
        stack[0].visible = false;
        Flight f = flightIn(w, stack);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        const FlightCounters& c = f.Stats();
        check("front_layer_filter/census_outcomes_a_hidden_filter_is_never_read",
            d.write && RearmostFilter(v.gameFront, v.layers) == -1
                && CensusTallyText(v.tally) == "fx=0/0/0 unread=behind:3,no-depth:0,no-detail:0,own:0,hidden:3,holds:4"
                && v.listed == 10 && tallySum(v.tally) == v.listed && w.effectReads == 0
                && c.fxStruct + c.fxOther + c.fxFailed == 0,
            censusText(f, w));
    }
    {
        // A filter whose name, visibility or elements do not read is never read
        // either, and is counted as no-detail.
        FlightWorld w;
        Flight f = flightIn(w, town01());
        w.detailFails.insert(301);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/census_outcomes_a_filter_whose_detail_fails_is_never_read",
            d.write && RearmostFilter(v.gameFront, v.layers) == -1
                && CensusTallyText(v.tally) == "fx=0/0/0 unread=behind:3,no-depth:0,no-detail:1,own:0,hidden:2,holds:4"
                && v.listed == 10 && tallySum(v.tally) == v.listed && w.effectReads == 0,
            censusText(f, w));
    }
    {
        // Effect_1 with a depth that does not read (the adapter hands such an
        // entry over with a non-finite depth): counted, not dropped.
        FlightWorld w;
        std::vector<WorldLayer> stack = town01();
        stack[0].depth = std::numeric_limits<double>::quiet_NaN();
        Flight f = flightIn(w, stack);
        const DepthWrite d = depthAt(f, w, 1, 7, 0.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/census_outcomes_an_unreadable_depth_is_counted_not_dropped",
            d.write && RearmostFilter(v.gameFront, v.layers) == -1
                && CensusTallyText(v.tally) == "fx=0/0/0 unread=behind:3,no-depth:1,no-detail:0,own:0,hidden:2,holds:4"
                && v.listed == 10 && tallySum(v.tally) == v.listed && w.effectReads == 0 && w.detailReads == 6,
            censusText(f, w));
    }
    {
        // A new visit starts its tally afresh; the session's totals sum both.
        FlightWorld w;
        w.layers[7] = kGameEnds;
        w.layers[8] = kGameEnds;
        w.rooms[7] = town01();
        w.rooms[8] = act0905();
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Wings);
        depthAt(f, w, 1, 7, 0.0);
        const std::string town = CensusTallyText(f.Visit().tally);
        depthAt(f, w, 2, 8, 100.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/census_outcomes_a_new_visit_starts_afresh",
            town == "fx=1/0/0 unread=behind:3,no-depth:0,no-detail:0,own:0,hidden:2,holds:4" && v.room == 8
                && CensusTallyText(v.tally) == "fx=3/0/0 unread=behind:1,no-depth:0,no-detail:0,own:0,hidden:0,holds:1"
                && v.listed == 5 && tallySum(v.tally) == v.listed && f.Stats().fxStruct == 4,
            "town " + town + " | " + censusText(f, w));
    }
    {
        // Act_01_01: the re-choice moves `filter=` to Effect_1@-10100 once
        // Tree_Leaves fills, and the tally stays the census's: nothing is read
        // again.
        FlightWorld w;
        Flight f = flightIn(w, act0101());
        depthAt(f, w, 1, 7, 0.0);
        const std::string atCensus = CensusTallyText(f.Visit().tally);
        const bool treeFirst = filterIs(f, "Tree_Leaves", -10000.0);
        worldLayer(w, 602)->elements = 40;
        depthAt(f, w, 2, 7, 1000.0);
        const FlightLayerVisit& v = f.Visit();
        check("front_layer_filter/census_outcomes_a_rechoice_keeps_the_census_tally",
            treeFirst && filterIs(f, "Effect_1", -10100.0) && v.moves == 1
                && atCensus == "fx=2/0/0 unread=behind:1,no-depth:0,no-detail:0,own:0,hidden:0,holds:0"
                && CensusTallyText(v.tally) == atCensus && v.listed == 3 && w.effectReads == 2,
            "census " + atCensus + " | " + censusText(f, w));
    }
    {
        // Before a census the visit's tally is empty.
        FlightWorld w;
        Flight f = makeFlight(w);
        check("front_layer_filter/census_outcomes_zero_before_a_census",
            CensusTallyText(f.Visit().tally) == kEmptyTally && CensusTallyText(CensusTally{}) == kEmptyTally
                && f.Visit().listed == 0 && tallySum(f.Visit().tally) == 0,
            CensusTallyText(f.Visit().tally));
    }
    {
        // The session's totals follow no-front= and end the trace line's
        // effect part, before the two monster counts the owner's scope
        // (2026-10-13) added at its end, and sum across visits: town (one
        // read, a struct) and then Act_09_05 with one read failing (two
        // structs, one failed).
        FlightWorld w;
        w.layers[7] = kGameEnds;
        w.layers[8] = kGameEnds;
        w.rooms[7] = town01();
        w.rooms[8] = act0905();
        w.effectFails.insert(503);
        Flight f = makeFlight(w);
        const std::string fresh = TraceLine("match", f.Stats());
        w.equipped = CharmBit(Charm::Wings);
        depthAt(f, w, 1, 7, 0.0);
        depthAt(f, w, 2, 8, 100.0);
        const FlightCounters& c = f.Stats();
        const std::string line = TraceLine("match", c);
        const auto endsWith = [](const std::string& s, std::string_view tail) {
            return s.size() >= tail.size() && s.compare(s.size() - tail.size(), tail.size(), tail) == 0;
        };
        check("trace/the_effect_totals_end_the_trace_line",
            endsWith(fresh, " no-front=0 fx-struct=0 fx-other=0 fx-failed=0 monster=0 monster-kept=0")
                && endsWith(line, " no-front=0 fx-struct=3 fx-other=0 fx-failed=1 monster=0 monster-kept=0")
                && c.fxStruct + c.fxOther + c.fxFailed == (uint64_t)w.effectReads && c.depth == 2 && sumsHold(c),
            fresh + " | " + line);
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
        ask(f, Hooked::PlaceMeeting, 2, true, EnemyOnlyPassage, true);                   // other
        ask(f, Hooked::PlaceMeeting, 2, true, Skeleton, true);                           // answered: a monster
        ask(f, Hooked::PositionMeeting, 2, true, Skeleton, true);                        // other: a monster kept
        ask(f, Hooked::PlaceMeeting, 2, true, -1, true);                                 // unreadable
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true, false, -1.0, 0.0);            // outside the room
        f.SetFamily(-1, { WallParent, LockObj });
        ask(f, Hooked::PlaceMeeting, 2, true, Fence, true);                              // unconfigured
        f.SetFamily(CollisionParent, { WallParent, LockObj });
        f.OnUpdateDepth(2, true, 7);                                                     // depth
        f.OnUpdateDepth(2, true, -1);                                                    // no front: room unreadable
        const FlightCounters& c = f.Stats();
        const bool every = c.notPlayer == 3 && c.unblocked == 2 && c.seen == 10 && c.idle == 1 && c.unconfigured == 1
            && c.unreadable == 1 && c.other == 2 && c.excluded == 2 && c.outsideRoom == 1 && c.answered == 2
            && c.monster == 1 && c.monsterKept == 1
            && c.depthOther == 2 && c.depthIdle == 1 && c.noFront == 1 && c.depth == 1;
        // monster= is a part of answered=, monster-kept= a part of other=.
        const bool sums = c.calls == c.notPlayer + c.unblocked + c.seen
            && c.seen == c.idle + c.unconfigured + c.unreadable + c.other + c.excluded + c.outsideRoom + c.answered
            && c.monster <= c.answered && c.monsterKept <= c.other
            && c.depthCalls == c.depthOther + c.depthSeen && c.depthSeen == c.depthIdle + c.noFront + c.depth;
        check("trace/the_counters_sum_as_documented", every && sums && c.calls == 15 && c.depthCalls == 5,
            TraceLine("match", c) + " | " + counts(c));
    }

    // ======================= size_guard ====================================
    // Grid 6x4. Ids: 1 a vanilla item, 2 Gabriel Wings (2x2), 3 Witch's
    // Broomstick (1x5), 4 Pocket Elf (1x1), 5 another vanilla item, 6 a second
    // Gabriel Wings (2x2).
    {
        // Baseline: unguarded, the entry order decides (Live 1's
        // `dress-added: item-missing`, read through § 23.6). The guard does
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

    // ======================= elf_draw ======================================
    // Pocket Elf's kill route on DropItem's own draw (#166): the stand-in over
    // lootListUnique[0..5], SS only; the coin from the call's recorded tier; one
    // DropItem call's push, its hits, and a nested roll setting them aside.
    {
        // Baseline: the switch off. Every call is counted, nothing is pushed,
        // nothing comes off, and a hit on Pocket Elf's triple is not ours.
        ElfDraw d;
        const DrawAction a = d.BeginCall(false, 0);
        const DrawHit h = d.Hit(10, 0, 20, 0.0);
        const bool off = d.EndCall();
        StatFields s;
        d.FillStat(s, false);
        check("elf_draw/baseline_switch_off_pushes_nothing",
            a == DrawAction::PassOff && h == DrawHit::NotPushed && !off && d.Calls() == 1 && d.PushedCalls() == 0
                && d.Ours() == 0 && d.TierUnread() == 0 && s.elfDrawCalls == 1 && s.elfDrawPushed == 0
                && s.elfDrawStandIn == "none" && ElfDrawCopies(DrawStandIn{}, 1) == 0,
            DrawActionName(a));
    }
    {
        // Baseline: a call that starts inside a roll already in progress (the
        // shared DropUniqueItems detour, #74's Angelic roll or the Heroic hook)
        // or inside another DropItem call pushes nothing; a roll nested in a
        // call that pushed nothing has nothing to set aside.
        ElfDraw d;
        const DrawAction inRoll = d.BeginCall(true, 1);
        const bool inRollOff = d.EndCall();
        const DrawAction outer = d.BeginCall(false, 0);
        const DrawAction inner = d.BeginCall(true, 0);
        const bool innerOff = d.EndCall();
        const bool outerOff = d.EndCall();
        const DrawAction plain = d.BeginCall(true, 0);
        const bool aside = d.EnterRoll();
        const bool back = d.LeaveRoll();
        const bool plainOff = d.EndCall();
        check("elf_draw/baseline_a_nested_roll_passes_untouched",
            inRoll == DrawAction::PassNested && !inRollOff && outer == DrawAction::PassOff && inner == DrawAction::PassNested
                && !innerOff && !outerOff && plain == DrawAction::Push && !aside && !back && !plainOff && d.Calls() == 4
                && d.Nested() == 2 && d.PushedCalls() == 0,
            DrawActionName(inRoll) + " " + DrawActionName(inner) + " " + DrawActionName(plain));
    }
    // The six lists as the adapter reads them: class-10 triples with a verdict
    // each, plus an entry of another class that is never a candidate.
    const auto drawFixture = [](bool withSs) {
        DrawLists l{};
        const auto put = [&](int k, int sub, int b, int times) {
            for (int i = 0; i < times; ++i) l[static_cast<size_t>(k)].push_back(UniqueEntry{ 10, double(sub), double(b) });
        };
        if (withSs) {
            put(0, 0, 41, 1); put(1, 0, 41, 1); put(4, 0, 41, 2);   // SS, largest count 2
            put(1, 0, 24, 1); put(5, 0, 24, 1);                     // SS, largest 1
            put(2, 0, 20, 1); put(3, 0, 20, 1);                     // SS, largest 1, the lower (sub, b)
            put(0, 0, 68, 1);                                       // SS, field 40 set
            put(2, 0, 91, 1);                                       // rarity 7
            put(3, 0, 55, 1);                                       // verdict unreadable
        }
        put(0, 0, 12, 1);                                           // tier 4: below SS
        put(5, 1, 3, 1);                                            // tier 3: below SS
        l[0].push_back(UniqueEntry{ 11, 0, 5 });                    // another class
        l[1].push_back(UniqueEntry{ 10, 0.5, 20 });                 // not a whole number
        return l;
    };
    const DrawVerdictFn drawVerdict = [](const DrawTriple& t, DrawVerdict& v) {
        if (t.t != 10) return false;
        v = DrawVerdict{ 6, 5, false, false };
        if (t.b == 68) v.f40 = true;
        if (t.b == 91) v.rarity = 7;
        if (t.b == 12) v.tier = 4;
        if (t.sub == 1 && t.b == 3) v.tier = 3;
        return t.b != 55;
    };
    {
        const ElfDrawResolution r = ResolveElfDrawStandIn(drawFixture(true), drawVerdict);
        const DrawStandIn& s = r.standIn;
        check("elf_draw/target_the_stand_in_is_ss_and_the_smallest_largest_count",
            s.resolved && s.triple.t == 10 && s.triple.sub == 0 && s.triple.b == 20 && s.rarity == 6 && s.tier == 5
                && s.counts == DrawCounts{ 0, 0, 1, 1, 0, 0 } && r.candidates == 5 && r.ss == 3 && r.unread == 1
                && ElfDrawStandInText(s) == "0:20@6/5x0,0,1,1,0,0" && kElfDrawTier == kDrawSsTier && kDrawSsTier == 5
                && kDrawListCount == 6 && kDrawRarity == 6,
            ElfDrawStandInText(s) + " candidates=" + std::to_string(r.candidates) + " ss=" + std::to_string(r.ss)
                + " unread=" + std::to_string(r.unread));
    }
    {
        // Aimbot.exe's rarity and tier, with no fallback below SS (the owner,
        // 2026-10-21): only below-SS candidates leave Pocket Elf with none.
        const ElfDrawResolution r = ResolveElfDrawStandIn(drawFixture(false), drawVerdict);
        const ElfDrawResolution empty = ResolveElfDrawStandIn(DrawLists{}, drawVerdict);
        const ElfDrawResolution blind = ResolveElfDrawStandIn(drawFixture(true), DrawVerdictFn{});
        check("elf_draw/target_no_ss_candidate_is_none_with_no_fallback",
            !r.standIn.resolved && r.candidates == 2 && r.ss == 0 && ElfDrawStandInText(r.standIn) == "none"
                && ElfDrawCopies(r.standIn, 1) == 0 && !empty.standIn.resolved && empty.candidates == 0
                && !blind.standIn.resolved && blind.unread > 0,
            "candidates=" + std::to_string(r.candidates) + " ss=" + std::to_string(r.ss));
    }
    {
        DrawStandIn s;
        s.resolved = true;
        s.triple = DrawTriple{ 10, 0, 20 };
        s.rarity = 6;
        s.tier = 5;
        s.counts = DrawCounts{ 0, 1, 2, 3, 4, 0 };
        const bool ok = ElfDrawShare(s, 0, 1) == 1.0 && ElfDrawShare(s, 1, 1) == 0.5 && ElfDrawShare(s, 3, 1) == 0.25
            && ElfDrawShare(s, 4, 1) == 0.2 && ElfDrawShare(s, 5, 1) == 1.0 && std::fabs(ElfDrawShare(s, 2, 1) - 1.0 / 3.0) < 1e-12
            && std::fabs(ElfDrawShare(s, 1, 2) - 2.0 / 3.0) < 1e-12 && ElfDrawShare(s, -1, 1) == 0.0
            && ElfDrawShare(s, 6, 1) == 0.0 && ElfDrawShare(s, 1, 0) == 0.0 && ElfDrawShare(DrawStandIn{}, 0, 1) == 0.0
            && ElfDrawCopies(s, 1) == 1 && ElfDrawCopies(s, 0) == 0 && ElfDrawCopies(s, 1000) == kDrawMaxCopies;
        check("elf_draw/target_the_coin_per_k", ok);
    }
    // A stand-in for the call scenarios: n_k is 0 on lists 0 and 5.
    DrawStandIn elfStand;
    elfStand.resolved = true;
    elfStand.triple = DrawTriple{ 10, 0, 20 };
    elfStand.rarity = 6;
    elfStand.tier = 5;
    elfStand.counts = DrawCounts{ 0, 1, 2, 3, 4, 0 };
    {
        ElfDraw d;
        const DrawAction a = d.BeginCall(true, 0);
        d.NoteResolution(ResolveElfDrawStandIn(drawFixture(true), drawVerdict));
        d.NotePushed(elfStand, 1);
        const bool off = d.EndCall();
        const DrawAction again = d.BeginCall(true, 0);
        d.NotePushed(elfStand, 0);   // no copies: nothing went on
        const bool againOff = d.EndCall();
        check("elf_draw/target_a_call_pushes_and_takes_its_entries_off",
            a == DrawAction::Push && off && again == DrawAction::Push && !againOff && d.Calls() == 2 && d.PushedCalls() == 1
                && d.Injected() == 6,
            std::to_string(d.Injected()));
    }
    {
        ElfDraw d;
        d.BeginCall(true, 0);
        d.NotePushed(elfStand, 1);
        const DrawHit unread = d.Hit(10, 0, 20, 0.0);
        d.NoteTier(7);   // out of range: still no tier
        const DrawHit stillUnread = d.Hit(10, 0, 20, 0.0);
        d.EndCall();
        check("elf_draw/target_no_tier_recorded_refuses_the_hit",
            unread == DrawHit::TierUnread && stillUnread == DrawHit::TierUnread && d.TierUnread() == 2 && d.Ours() == 0
                && !d.HitPending(),
            DrawHitName(unread));
    }
    {
        ElfDraw d;
        d.BeginCall(true, 0);
        d.NotePushed(elfStand, 1);
        d.NoteTier(1);   // n_1 = 1, c = 1: half
        const DrawHit ours = d.Hit(10, 0, 20, 0.49);
        d.Settle(true);
        const DrawHit game = d.Hit(10, 0, 20, 0.5);
        const DrawHit other = d.Hit(10, 0, 41, 0.0);
        const DrawHit otherClass = d.Hit(11, 0, 20, 0.0);
        d.EndCall();
        d.NoteBuilt();
        d.NoteBuilt();   // only one was rewritten
        check("elf_draw/target_the_hit_takes_the_coin_at_the_calls_k",
            ours == DrawHit::Ours && game == DrawHit::Game && other == DrawHit::NotStandIn && otherClass == DrawHit::NotStandIn
                && d.Ours() == 1 && d.Built() == 1,
            DrawHitName(ours) + " " + DrawHitName(game) + " " + DrawHitName(other));
    }
    {
        // Two hits in one call, each settled before the next starts: the
        // second takes its own coin at the tier recorded for it, and a hit
        // left unsettled is closed by the next one, never carried into it.
        ElfDraw d;
        d.BeginCall(true, 0);
        d.NotePushed(elfStand, 1);
        d.NoteTier(0);
        const DrawHit first = d.Hit(10, 0, 20, 0.99);
        const bool firstPending = d.HitPending();
        d.Settle(true);
        const bool settled = !d.HitPending();
        d.NoteTier(5);
        const DrawHit second = d.Hit(10, 0, 20, 0.99);
        d.Settle(true);
        d.NoteTier(4);   // n_4 = 4: a fifth
        const DrawHit third = d.Hit(10, 0, 20, 0.0);
        const DrawHit fourth = d.Hit(10, 0, 20, 0.5);   // the third never settled
        const bool fourthPending = d.HitPending();
        d.EndCall();
        d.NoteBuilt();
        d.NoteBuilt();
        d.NoteBuilt();
        check("elf_draw/target_two_hits_in_one_call_are_settled_apart",
            first == DrawHit::Ours && firstPending && settled && second == DrawHit::Ours && third == DrawHit::Ours
                && fourth == DrawHit::Game && !fourthPending && d.Ours() == 3 && d.Built() == 2,
            DrawHitName(first) + " " + DrawHitName(second) + " " + DrawHitName(third) + " " + DrawHitName(fourth)
                + " built=" + std::to_string(d.Built()));
    }
    {
        // A roll nested in the call: its entries come off once, at the first
        // level, and go back once, at the last; inside it no hit is ours and
        // the call's open hit waits, then comes back with the entries.
        ElfDraw d;
        d.BeginCall(true, 0);
        d.NotePushed(elfStand, 1);
        d.NoteTier(0);
        const DrawHit before = d.Hit(10, 0, 20, 0.0);
        const bool off = d.EnterRoll();
        const bool aside = !d.HitPending();
        const DrawHit inside = d.Hit(10, 0, 20, 0.0);
        const DrawAction innerCall = d.BeginCall(true, 0);
        d.EndCall();
        const bool deeper = d.EnterRoll();
        const bool deeperBack = d.LeaveRoll();
        const bool back = d.LeaveRoll();
        const bool restored = d.HitPending();
        d.Settle(true);
        const DrawHit after = d.Hit(10, 0, 20, 0.0);
        const bool extraLeave = d.LeaveRoll();
        const bool callOff = d.EndCall();
        check("elf_draw/target_a_nested_roll_sets_the_entries_and_the_hit_aside",
            before == DrawHit::Ours && off && aside && inside == DrawHit::SetAside && innerCall == DrawAction::PassNested
                && !deeper && !deeperBack && back && restored && after == DrawHit::Ours && !extraLeave && callOff
                && d.Ours() == 2,
            DrawHitName(before) + " " + DrawHitName(inside) + " " + DrawHitName(after));
    }
    {
        // A DropItem call inside a pushing one pushes nothing, but the draw it
        // runs reads its own tier: the coin uses the innermost call's k.
        ElfDraw d;
        d.BeginCall(true, 0);
        d.NotePushed(elfStand, 1);
        d.NoteTier(0);
        const DrawAction inner = d.BeginCall(true, 0);
        const DrawHit innerUnread = d.Hit(10, 0, 20, 0.0);
        d.NoteTier(4);
        const DrawHit innerGame = d.Hit(10, 0, 20, 0.5);   // 0.5 >= 1/5
        const bool innerOff = d.EndCall();
        const DrawHit outer = d.Hit(10, 0, 20, 0.99);      // k 0 again: always ours
        const bool outerOff = d.EndCall();
        const DrawHit gone = d.Hit(10, 0, 20, 0.0);
        check("elf_draw/target_an_inner_call_reads_its_own_tier",
            inner == DrawAction::PassNested && innerUnread == DrawHit::TierUnread && innerGame == DrawHit::Game && !innerOff
                && outer == DrawHit::Ours && outerOff && gone == DrawHit::NotPushed,
            DrawHitName(innerUnread) + " " + DrawHitName(innerGame) + " " + DrawHitName(outer));
    }
    {
        // The stand-in is resolved again whenever any of [0..5] changed its
        // array or its length.
        ElfDrawCache c;
        DrawListKeys keys{};
        for (int k = 0; k < kDrawListCount; ++k) keys[static_cast<size_t>(k)] = DrawListKey{ 100.0 + k, 10 + k };
        const bool freshStale = c.Stale(keys) && c.Find() == nullptr;
        c.Store(keys, ResolveElfDrawStandIn(drawFixture(true), drawVerdict));
        const bool kept = !c.Stale(keys) && c.Find() && c.Find()->standIn.triple.b == 20;
        DrawListKeys longer = keys;
        longer[5].length = 99;
        DrawListKeys moved = keys;
        moved[0].id = 7.0;
        check("elf_draw/target_the_stand_in_resolves_again_when_a_list_changes",
            freshStale && kept && c.Stale(longer) && c.Stale(moved));
    }
    {
        ElfDraw d;
        const std::string offFresh = d.StandInField(false);
        const std::string onFresh = d.StandInField(true);
        d.NoteResolution(ResolveElfDrawStandIn(drawFixture(true), drawVerdict));
        const std::string resolved = d.StandInField(true);
        d.NoteResolution(ResolveElfDrawStandIn(drawFixture(false), drawVerdict));
        const std::string none = d.StandInField(true);
        d.NoteListsUnreadable();
        const std::string unreadable = d.StandInField(true);
        check("elf_draw/target_the_stand_in_field",
            offFresh == "none" && onFresh == "pending" && resolved == "0:20@6/5x0,0,1,1,0,0" && none == "none"
                && unreadable == "none",
            offFresh + " " + onFresh + " " + resolved + " " + none + " " + unreadable);
    }
    {
        ElfDraw d;
        d.BeginCall(false, 0);
        d.EndCall();
        d.BeginCall(true, 0);
        d.NoteResolution(ResolveElfDrawStandIn(drawFixture(true), drawVerdict));
        d.NotePushed(elfStand, 1);
        d.NoteTier(0);
        d.Hit(10, 0, 20, 0.0);
        d.Settle(true);
        d.EndCall();
        d.NoteBuilt();
        StatFields s;
        d.FillStat(s, true);
        s.elfDrawHook = "detoured";
        // The adapter's typed-build counters, then its class fields (#166 replan 1).
        s.elfDrawTyped = DrawTypedCounts{ 1, 5, 2, 3, 1, 0, 2, 0, 6, 2, 2, 0, 1, 0, 9, 1, 1 };
        const std::string line = StatLine(s);
        const std::string tail = " standin-elf=none elf-draw-hook=detoured elf-draw-calls=2 elf-draw-pushed=1 elf-draw=1/1 "
                                 "elf-draw-standin=0:20@6/5x0,0,1,1,0,0 elf-draw-typed=1/5 elf-draw-untyped=2 elf-draw-normal=3 "
                                 "elf-draw-outside=busy:1,aside:0,lists:2,roll:0 elf-draw-class=bool:6/2,num:2/0,none:1,other:0 "
                                 "elf-draw-class-check=9/1/1";
        check("elf_draw/target_the_stat_fields_after_use",
            line.size() > tail.size() && line.compare(line.size() - tail.size(), tail.size(), tail) == 0, line);
    }

    // ======================= install =======================================
    {
        Flight f;
        // The baseline, with a flight charm worn: armed by a charm, installed
        // once a character exists.
        const bool notArmed = f.InstallStateName() == "not-armed" && !f.ShouldInstall(true, true, true) && !f.Armed();
        f.Arm();
        const bool waiting = f.InstallStateName() == "waiting-for-character" && !f.ShouldInstall(false, true, true)
            && !f.ShouldInstall(true, false, true) && f.ShouldInstall(true, true, true);
        f.NoteLook(true, true);
        f.NoteInstall(true);
        f.Arm();
        const bool installed = f.InstallStateName() == "installed" && f.Installed() && !f.ShouldInstall(true, true, true);
        check("install/armed_by_a_charm_installed_once_a_character_exists", notArmed && waiting && installed);
    }
    {
        // #216 b2: a charm carried in the bag or stash arms flight, but the
        // hooks wait for one to be worn; the look reports what it waits for.
        Flight f;
        f.Arm();
        f.NoteLook(true, false);
        const bool waits = f.InstallStateName() == "waiting-for-worn-charm" && !f.ShouldInstall(true, true, false)
            && !f.Installed() && f.Armed();
        // Back at character select: waiting for a character again, not for a charm.
        f.NoteLook(false, false);
        const bool noCharacter = f.InstallStateName() == "waiting-for-character" && !f.ShouldInstall(true, false, false);
        f.NoteLook(true, false);
        const bool waitsAgain = f.InstallStateName() == "waiting-for-worn-charm";
        check("install/a_charm_not_worn_waits_for_a_worn_charm", waits && noCharacter && waitsAgain,
            std::string(f.InstallStateName()));
    }
    {
        Flight f;
        f.Arm();
        f.NoteLook(true, false);
        const bool waits = f.InstallStateName() == "waiting-for-worn-charm" && !f.ShouldInstall(true, true, false);
        // The next look finds one worn: that look installs.
        f.NoteLook(true, true);
        const bool ready = f.ShouldInstall(true, true, true) && !f.ShouldInstall(false, true, true);
        f.NoteInstall(true);
        // Installed, nothing changes when it is taken off again.
        f.NoteLook(true, false);
        const bool installed = f.InstallStateName() == "installed" && f.Installed() && !f.ShouldInstall(true, true, true);
        check("install/a_worn_charm_installs_at_the_next_look", waits && ready && installed,
            std::string(f.InstallStateName()));
    }
    {
        Flight f;
        f.Arm();
        f.NoteInstall(false);
        f.Arm();
        check("install/a_refusal_is_final_for_the_session",
            f.InstallStateName() == "refused" && !f.ShouldInstall(true, true, true) && !f.Installed()
                && RefusedLine("InstancePlaceTallest came back table-only")
                    == "flightcharm: refused - InstancePlaceTallest came back table-only");
    }
    {
        check("install/looks_for_the_player_every_60_frames",
            Flight::kInstallPollFrames == 60 && Flight::LooksForPlayer(0) && !Flight::LooksForPlayer(1)
                && !Flight::LooksForPlayer(59) && Flight::LooksForPlayer(60) && Flight::LooksForPlayer(120));
    }

    // ======================= state_at (#216 c4) =============================
    // What a hook asks before it builds a Query: the state, refreshed at most
    // once a frame, in one call rather than Refresh then CurrentState.
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Broom);
        const bool first = f.StateAt(7) == State::On && w.equippedReads == 1;
        w.equipped = 0;   // taken off within the frame: not read again until the next
        const bool sameFrame = f.StateAt(7) == State::On && f.StateAt(7) == State::On && w.equippedReads == 1;
        const bool nextFrame = f.StateAt(8) == State::Off && w.equippedReads == 2;
        const bool frameZero = [&]() {
            FlightWorld z;
            Flight g = makeFlight(z);
            z.equipped = CharmBit(Charm::Wings);
            return g.StateAt(0) == State::On && g.StateAt(0) == State::On && z.equippedReads == 1;
        }();
        check("state_at/reads_the_worn_charms_once_a_frame", first && sameFrame && nextFrame && frameZero,
            "equipped-reads=" + std::to_string(w.equippedReads));
    }
    {
        // The same world, frame by frame, through StateAt and through Refresh
        // then CurrentState: on, residual inside a prop, residual held within
        // the frame, off once free, on again, and an unreadable charm array.
        FlightWorld a, b;
        Flight viaStateAt = makeFlight(a);
        Flight viaRefresh = makeFlight(b);
        struct Step { int64_t frame; unsigned equipped; bool overlapping; bool equippedReadable; };
        const std::vector<Step> steps = {
            { 1, CharmBit(Charm::Elf), false, true }, { 2, 0, true, true }, { 2, 0, false, true },
            { 3, 0, true, true }, { 4, 0, false, true }, { 5, CharmBit(Charm::Wings), false, true },
            { 6, CharmBit(Charm::Wings), true, false }, { 7, 0, false, true },
        };
        bool same = true;
        std::string trail;
        for (const Step& s : steps) {
            for (FlightWorld* world : { &a, &b }) {
                world->equipped = s.equipped;
                world->overlapping = s.overlapping;
                world->equippedReadable = s.equippedReadable;
            }
            const State at = viaStateAt.StateAt(s.frame);
            viaRefresh.Refresh(s.frame);
            const State refreshed = viaRefresh.CurrentState();
            same = same && at == refreshed && viaStateAt.EquippedMask() == viaRefresh.EquippedMask();
            trail += std::string(" ") + std::string(StateName(at)) + "/" + std::string(StateName(refreshed));
        }
        same = same && a.equippedReads == b.equippedReads && a.overlapReads == b.overlapReads;
        check("state_at/answers_what_refresh_then_current_state_answered", same, trail);
    }

    // ======================= player_point (#216 c3) =========================
    // The local player is confirmed at the first hooked call of each frame,
    // before any `self` compare; the core decides which call that is. The
    // frame counter advances at the frame's end, so every call between two
    // advances is one frame.
    {
        Flight f;
        Flight g;
        const bool fresh = f.ConfirmsPlayer(5);
        g.ConfirmsPlayer(4);
        const bool afterAnother = g.ConfirmsPlayer(5);
        Flight z;
        const bool frameZero = z.ConfirmsPlayer(0);   // 0 is a frame like any other, not "none yet"
        check("player_point/the_first_call_of_a_frame_confirms_the_player", fresh && afterAnother && frameZero);
    }
    {
        Flight f;
        f.ConfirmsPlayer(5);
        bool none = true;
        for (int i = 0; i < 4; ++i) none = none && !f.ConfirmsPlayer(5);
        Flight z;
        z.ConfirmsPlayer(0);
        const bool zeroOnce = !z.ConfirmsPlayer(0);
        check("player_point/later_calls_of_the_frame_confirm_nothing", none && zeroOnce);
    }
    {
        Flight f;
        f.ConfirmsPlayer(5);
        f.ConfirmsPlayer(5);
        const bool next = f.ConfirmsPlayer(6) && !f.ConfirmsPlayer(6);
        const bool skipped = f.ConfirmsPlayer(9);
        const bool wrapped = f.ConfirmsPlayer(0) && !f.ConfirmsPlayer(0);   // a counter that wraps still changes
        // The state's own once-a-frame refresh is apart: confirming the player
        // reads nothing, and refreshing the state confirms nothing.
        FlightWorld w;
        Flight s = makeFlight(w);
        s.StateAt(3);
        const bool apart = s.ConfirmsPlayer(3) && w.equippedReads == 1 && w.overlapReads == 0;
        check("player_point/a_new_frame_confirms_again", next && skipped && wrapped && apart);
    }

    // ======================= worn_cache (#216 b1) ===========================
    // The worn-charm read cached while the adapter's signal (UpdateCharmData,
    // hooked inline) is on: re-read after an invalidation, and once every
    // kWornSafetyFrames whatever happens. The world's charms change behind the
    // cache's back in every scenario, so a frame that answers the old mask is
    // a frame that did not read.
    {
        FlightWorld w;
        Flight f = makeFlight(w);
        f.SetWornCache(true);
        w.equipped = CharmBit(Charm::Elf);
        const bool first = f.StateAt(1) == State::On && w.equippedReads == 1;
        w.equipped = 0;   // taken off with no signal: the cache still answers elf
        bool held = true;
        for (int64_t frame = 2; frame <= 60; ++frame)
            held = held && f.StateAt(frame) == State::On && f.EquippedMask() == CharmBit(Charm::Elf);
        const WornCacheCounters& c = f.WornCache();
        check("worn_cache/answers_without_a_re_read",
            f.WornCacheOn() && first && held && w.equippedReads == 1 && c.reads == 1 && c.safety == 0 && c.stale == 0,
            "equipped-reads=" + std::to_string(w.equippedReads) + " cache-reads=" + std::to_string(c.reads));
    }
    {
        // One invalidation of each cause: the frame after it reads once, and
        // only that frame; each cause counts in its own field alone.
        struct Cause { const char* label; WornInvalidation why; uint64_t WornCacheCounters::* field; };
        const Cause causes[] = {
            { "worn_cache/an_update_charm_data_call_invalidates", WornInvalidation::UpdateCharmData, &WornCacheCounters::ucd },
            { "worn_cache/a_player_change_invalidates", WornInvalidation::Player, &WornCacheCounters::player },
            { "worn_cache/a_dress_invalidates", WornInvalidation::Dress, &WornCacheCounters::dress },
        };
        for (const Cause& cause : causes) {
            FlightWorld w;
            Flight f = makeFlight(w);
            f.SetWornCache(true);
            w.equipped = CharmBit(Charm::Wings);
            f.StateAt(1);
            f.StateAt(2);
            const bool before = w.equippedReads == 1;
            w.equipped = 0;
            f.InvalidateWorn(cause.why);
            // Within the frame already refreshed nothing is read again.
            const bool sameFrame = f.StateAt(2) == State::On && w.equippedReads == 1;
            const bool next = f.StateAt(3) == State::Off && w.equippedReads == 2 && f.EquippedMask() == 0;
            w.equipped = CharmBit(Charm::Broom);   // put on with no signal: not seen until the next invalidation
            bool onlyThatFrame = true;
            for (int64_t frame = 4; frame <= 20; ++frame) onlyThatFrame = onlyThatFrame && f.StateAt(frame) == State::Off;
            onlyThatFrame = onlyThatFrame && w.equippedReads == 2;
            f.InvalidateWorn(cause.why);
            const bool again = f.StateAt(21) == State::On && f.EquippedMask() == CharmBit(Charm::Broom) && w.equippedReads == 3;
            const WornCacheCounters& c = f.WornCache();
            const uint64_t total = c.ucd + c.player + c.dress;
            const bool counted = c.*cause.field == 2 && total == 2 && c.reads == 3 && c.safety == 0 && c.stale == 0;
            check(cause.label, before && sameFrame && next && onlyThatFrame && again && counted,
                "equipped-reads=" + std::to_string(w.equippedReads) + " cache-reads=" + std::to_string(c.reads)
                    + " ucd=" + std::to_string(c.ucd) + " player=" + std::to_string(c.player) + " dress="
                    + std::to_string(c.dress));
        }
    }
    {
        // A read once every 60 frames while valid, counted as safety; a
        // stretch with no hooked call reads at the next frame that refreshes;
        // a read after an invalidation restarts the period and is not safety.
        FlightWorld w;
        Flight f = makeFlight(w);
        f.SetWornCache(true);
        w.equipped = CharmBit(Charm::Elf);
        f.StateAt(1);
        bool none = true;
        for (int64_t frame = 2; frame <= 60; ++frame) f.StateAt(frame);
        none = w.equippedReads == 1;
        f.StateAt(61);
        const bool sixtyOne = w.equippedReads == 2 && f.WornCache().safety == 1;
        for (int64_t frame = 62; frame <= 120; ++frame) f.StateAt(frame);
        const bool quiet = w.equippedReads == 2;
        f.StateAt(121);
        const bool twice = w.equippedReads == 3 && f.WornCache().safety == 2;
        f.StateAt(300);   // no hooked call from 122 to 299
        const bool gap = w.equippedReads == 4 && f.WornCache().safety == 3;
        f.InvalidateWorn(WornInvalidation::UpdateCharmData);
        f.StateAt(310);
        const bool invalid = w.equippedReads == 5 && f.WornCache().safety == 3;
        for (int64_t frame = 311; frame <= 369; ++frame) f.StateAt(frame);
        const bool restarted = w.equippedReads == 5;
        f.StateAt(370);
        const bool afterInvalid = w.equippedReads == 6 && f.WornCache().safety == 4;
        f.StateAt(5);   // a counter that wraps still reads
        const bool wrapped = w.equippedReads == 7 && f.WornCache().safety == 5;
        const WornCacheCounters& c = f.WornCache();
        check("worn_cache/a_safety_re_read_every_60_frames",
            Flight::kWornSafetyFrames == 60 && none && sixtyOne && quiet && twice && gap && invalid && restarted
                && afterInvalid && wrapped && c.reads == 7 && c.stale == 0,
            "equipped-reads=" + std::to_string(w.equippedReads) + " safety=" + std::to_string(c.safety));
    }
    {
        // A missed signal: the safety read finds another mask, counts it
        // stale and takes it. An agreeing safety read is not stale, and a
        // failed one answers none, is not stale and reads again next frame.
        FlightWorld w;
        Flight f = makeFlight(w);
        f.SetWornCache(true);
        w.equipped = CharmBit(Charm::Elf);
        f.StateAt(1);
        w.equipped = 0;
        const bool held = f.StateAt(60) == State::On;
        const bool takenOff = f.StateAt(61) == State::Off && f.EquippedMask() == 0 && f.WornCache().stale == 1;
        w.equipped = CharmBit(Charm::Wings);
        const bool putOn = f.StateAt(121) == State::On && f.EquippedMask() == CharmBit(Charm::Wings)
            && f.WornCache().stale == 2;
        const bool agreeing = f.StateAt(181) == State::On && f.WornCache().stale == 2 && f.WornCache().safety == 3;
        w.equippedReadable = false;
        const bool failed = f.StateAt(241) == State::Off && f.WornCache().stale == 2 && f.WornCache().safety == 4
            && w.equippedReads == 5;
        w.equippedReadable = true;
        const bool retried = f.StateAt(242) == State::On && w.equippedReads == 6 && f.WornCache().safety == 4
            && f.WornCache().stale == 2;
        check("worn_cache/a_stale_safety_read_is_counted_and_taken",
            held && takenOff && putOn && agreeing && failed && retried,
            "equipped-reads=" + std::to_string(w.equippedReads) + " stale=" + std::to_string(f.WornCache().stale));
    }
    {
        // The baseline: with the signal off (the default, and a signal that
        // went in table-only or not at all) the worn charms are read once a
        // frame exactly as before, invalidations or not; the cache reads
        // nothing of its own. Turned on and off again, it is off.
        FlightWorld w;
        Flight f = makeFlight(w);
        w.equipped = CharmBit(Charm::Elf);
        bool perFrame = !f.WornCacheOn();
        for (int64_t frame = 1; frame <= 10; ++frame) {
            f.StateAt(frame);
            f.StateAt(frame);
            perFrame = perFrame && w.equippedReads == frame;
        }
        f.InvalidateWorn(WornInvalidation::UpdateCharmData);
        w.equipped = 0;
        const bool seen = f.StateAt(11) == State::Off && w.equippedReads == 11;
        FlightWorld z;
        Flight g = makeFlight(z);
        g.SetWornCache(true);
        g.SetWornCache(false);
        z.equipped = CharmBit(Charm::Broom);
        for (int64_t frame = 1; frame <= 5; ++frame) g.StateAt(frame);
        const bool offAgain = !g.WornCacheOn() && z.equippedReads == 5;
        check("worn_cache/signal_off_reads_once_a_frame",
            perFrame && seen && offAgain && f.WornCache().reads == 0 && g.WornCache().reads == 0,
            "equipped-reads=" + std::to_string(w.equippedReads));
    }
    {
        // The stat's cache line after its route: field names and spellings.
        WornCacheCounters c;
        c.reads = 4;
        c.ucd = 2;
        c.player = 1;
        c.dress = 3;
        c.safety = 1;
        c.stale = 0;
        const std::string agree = CacheFields(c, CharmBit(Charm::Elf), true, CharmBit(Charm::Elf));
        const std::string differ = CacheFields(WornCacheCounters{}, 0, true, CharmBit(Charm::Wings));
        const std::string unread = CacheFields(WornCacheCounters{}, 0, false, 0);
        const bool ok = agree == "reads=4 invalidations=ucd:2,player:1,dress:3 safety=1 stale=0 worn=elf fresh=elf agree=yes"
            && differ == "reads=0 invalidations=ucd:0,player:0,dress:0 safety=0 stale=0 worn=none fresh=wings agree=no"
            && unread == "reads=0 invalidations=ucd:0,player:0,dress:0 safety=0 stale=0 worn=none fresh=unread agree=no";
        check("worn_cache/the_cache_line_names_its_fields", ok, agree + " | " + differ + " | " + unread);
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
                                  "anomalies=0 standin-elf=none elf-draw-hook=not-installed elf-draw-calls=0 "
                                  "elf-draw-pushed=0 elf-draw=0/0 elf-draw-standin=none elf-draw-typed=0/0 elf-draw-untyped=0 "
                                  "elf-draw-normal=0 elf-draw-outside=busy:0,aside:0,lists:0,roll:0 "
                                  "elf-draw-class=bool:0/0,num:0/0,none:0,other:0 elf-draw-class-check=0/0/0";
        Flight f;
        SizeGuard<Pending> g;
        ElfDraw e;
        StatFields s;
        f.FillStat(s);
        g.FillStat(s);
        e.FillStat(s, false);
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
                                "anomalies=0 standin-elf=aimbot elf-draw-hook=not-installed elf-draw-calls=0 "
                                "elf-draw-pushed=0 elf-draw=0/0 elf-draw-standin=none elf-draw-typed=0/0 elf-draw-untyped=0 "
                                "elf-draw-normal=0 elf-draw-outside=busy:0,aside:0,lists:0,roll:0 "
                                "elf-draw-class=bool:0/0,num:0/0,none:0,other:0 elf-draw-class-check=0/0/0";
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
