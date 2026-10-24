#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ForgePact::FlightCharms {

// ---- Flight charms: the decision core (ForgePact #158, phase 2) -------------
//
// Three built-in Custom Forge charms. Pocket Elf (Satanic, 1x1) drops from the
// ordinary unique roll; Gabriel Wings (Angelic, 2x2) and Witch's Broomstick
// (Unholy, 1x5) drop from the Angelic roll. Each drop sits behind its own
// panel switch, off by default. While one is worn the local player walks
// through scenery (the Collision_Parent_obj family) and through monsters
// (Enemy_Parent_obj and its descendants, body collision only: the owner,
// 2026-10-13), but not through walls, gates, locks or the room's edge, and is
// drawn in front of scenery.
//
// This header decides; it never touches the game. It holds:
//   - the item table and the seed codec. A charm is saved as a plain Small
//     Charm whose seed `a` falls in one of three ranges; its rolls are decoded
//     from a - lo. That layout is a save format: changing it after release
//     re-rolls every copy a player owns.
//   - flight's per-query decision for the five builtins jumpscenery hooks and
//     InstancePlaceTallest, with the off / on / residual state, refreshed at
//     most once a frame by the first query of the local player in that frame
//     (the point of use, never EVENT_FRAME), and by `flightcharm stat`.
//   - flight's install state (armed when dressing first recognises a charm,
//     installed once a character exists).
//   - the flight layer the flying player is drawn on after the game's
//     UpdateDepth: its census once per room visit and its re-choice.
//   - the size guard, which defers a dressed charm to the end of its grid's
//     load so that it never takes a cell a vanilla item needs.
//   - Pocket Elf's stand-in rule, and the coin every flight drop is decided by.
//   - Pocket Elf's kill route on DropItem's own draw (#166): its SS stand-in
//     over lootListUnique[0..5], the coin at the call's recorded tier, and
//     one DropItem call's push, hits and nested rolls (ElfDraw).
//   - the `flightcharm` command's forms and its `stat` line.
//
// It is game-independent by contract - numbers and strings, never an instance
// or an RValue - so tests/flight_charms_harness.cpp compiles it whole. The
// adapter in ModuleMain.cpp resolves every object, script and builtin by name
// and supplies each game read through the callbacks below, as JumpScenery.hpp's
// does. The research is docs/flight-charms-research.md; the game facts are in
// the hub's docs/RUNTIME_DATA_MODELS.md (§ 19.6-19.8, § 23).
//
// The namespace is shared with FlightCharmProbe.hpp (the research build's
// charmprobe). No name here repeats one of that header's, and the harness
// compiles the two together.

// ---- the three charms ---------------------------------------------------------

enum class Charm : int { Elf, Wings, Broom };
inline constexpr int kCharmCount = 3;

// Every charm is built on the normal Small Charm: type 10, b 0, c 0, j 0.
inline constexpr int kCharmTypeT = 10;
inline constexpr int kCharmBaseB = 0;
inline constexpr int kMaxCharmStats = 2;

// One rolled stat: its item stat id and the inclusive range of its value.
struct StatRoll {
    int id;
    int lo;
    int hi;
};

struct CharmRow {
    Charm            charm;
    std::string_view key;       // the panel key's and `flightcharm`'s word
    std::string_view name;
    std::string_view lore;
    int              rarity;    // itemInfoStruct "27"
    int              tier;      // itemInfoStruct "32" (5 = SS)
    int              w;         // footprint in grid cells, itemInfoStruct "2"
    int              h;         // itemInfoStruct "3"
    long long        seedLo;    // the seed range; its size is exactly the number of rolls
    long long        seedHi;
    int              statCount;
    StatRoll         stats[kMaxCharmStats];
    int              bagW, bagH;        // icon sizes in px (assets/flight-charms/)
    int              groundW, groundH;
};

// The seeds collide with nothing in use: 777001-777003 are the signature items
// and the Miner's Helmet, 779000 and up the phase-1 research seeds.
// Gabriel Wings: a = 777200 + (p - 1) * 12 + (m - 1), p physical and m magic
// damage reduction. Witch's Broomstick: a = 777400 + (s - 10), s movement speed.
inline constexpr CharmRow kCharms[kCharmCount] = {
    { Charm::Elf, "elf", "Pocket Elf", "A tiny helper who knows every shortcut.",
      6, 5, 1, 1, 777100, 777100, 0, { { 0, 0, 0 }, { 0, 0, 0 } }, 32, 32, 16, 16 },
    { Charm::Wings, "wings", "Gabriel Wings", "Lighter than any wall is tall.",
      7, 5, 2, 2, 777200, 777343, 2, { { 159, 1, 12 }, { 160, 1, 12 } }, 64, 64, 24, 24 },
    { Charm::Broom, "broom", "Witch's Broomstick", "Swept clean of every obstacle.",
      10, 5, 1, 5, 777400, 777490, 1, { { 25, 10, 100 }, { 0, 0, 0 } }, 32, 160, 34, 17 },
};

// Each charm shows one gold affix row reading exactly this, and carries
// fp_mechanic = kMechanic. A charm the size guard shrank is tagged kGuardTag.
inline constexpr std::string_view kAffixRow = "Flight";
inline constexpr std::string_view kMechanic = "flight";
inline constexpr std::string_view kGuardTag = "fp_flight_guarded";

inline constexpr long long RollCount(const CharmRow& r)
{
    long long n = 1;
    for (int k = 0; k < r.statCount; ++k) n *= static_cast<long long>(r.stats[k].hi - r.stats[k].lo + 1);
    return n;
}

inline constexpr bool SeedRangesExact()
{
    for (const CharmRow& r : kCharms)
        if (r.seedHi - r.seedLo + 1 != RollCount(r)) return false;
    for (int i = 0; i < kCharmCount; ++i)
        if (static_cast<int>(kCharms[i].charm) != i) return false;
    return true;
}
static_assert(SeedRangesExact(), "each charm's seed range holds exactly its rolls, in table order");

inline const CharmRow& RowOf(Charm c)
{
    const int i = static_cast<int>(c);
    return kCharms[(i >= 0 && i < kCharmCount) ? i : 0];
}

inline constexpr unsigned CharmBit(Charm c) { return 1u << static_cast<int>(c); }
inline constexpr unsigned kAllCharmBits = (1u << kCharmCount) - 1u;

inline bool CharmFromKey(std::string_view key, Charm& out)
{
    for (const CharmRow& r : kCharms) {
        if (r.key == key) {
            out = r.charm;
            return true;
        }
    }
    return false;
}

// A decoded charm: which one, and its rolled stats in table order.
struct Rolled {
    Charm charm = Charm::Elf;
    int   count = 0;
    int   stat[kMaxCharmStats] = { 0, 0 };
    int   value[kMaxCharmStats] = { 0, 0 };
};

// The seed for a charm's rolls (v0 the first stat, v1 the second; extra values
// are ignored), or -1 when a value is outside its range. The first stat is the
// major digit.
inline long long SeedFor(Charm c, int v0 = 0, int v1 = 0)
{
    const int i = static_cast<int>(c);
    if (i < 0 || i >= kCharmCount) return -1;
    const CharmRow& r = kCharms[i];
    const int v[kMaxCharmStats] = { v0, v1 };
    long long offset = 0;
    for (int k = 0; k < r.statCount; ++k) {
        const StatRoll& s = r.stats[k];
        if (v[k] < s.lo || v[k] > s.hi) return -1;
        offset = offset * (s.hi - s.lo + 1) + (v[k] - s.lo);
    }
    return r.seedLo + offset;
}

// The best roll's seed (`flightprobe spawn`'s default).
inline long long TopSeed(Charm c) { return RowOf(c).seedHi; }

// A seed for a uniformly drawn roll, u in [0, 1). Every roll of the range is
// equally likely; u outside the range is clamped to its ends (NaN to the low).
inline long long SeedFromUniform(Charm c, double u)
{
    const CharmRow& r = RowOf(c);
    const long long n = r.seedHi - r.seedLo + 1;
    if (!(u > 0.0)) return r.seedLo;
    long long k = static_cast<long long>(std::floor(u * static_cast<double>(n)));
    if (k >= n) k = n - 1;
    return r.seedLo + k;
}

// The seed alone: which charm, and its rolls.
inline bool DecodeSeed(long long a, Rolled& out)
{
    for (const CharmRow& r : kCharms) {
        if (a < r.seedLo || a > r.seedHi) continue;
        Rolled d;
        d.charm = r.charm;
        d.count = r.statCount;
        long long rest = a - r.seedLo;
        for (int k = r.statCount - 1; k >= 0; --k) {
            const StatRoll& s = r.stats[k];
            const long long span = s.hi - s.lo + 1;
            d.stat[k] = s.id;
            d.value[k] = static_cast<int>(s.lo + rest % span);
            rest /= span;
        }
        out = d;
        return true;
    }
    return false;
}

// A whole number a real holds exactly (|v| <= 2^53 - 1).
inline bool AsWhole(double v, long long& out)
{
    if (!std::isfinite(v) || std::floor(v) != v || std::fabs(v) > 9007199254740991.0) return false;
    out = static_cast<long long>(v);
    return true;
}

// Recognition: type 10, b 0, c 0 and j 0, with `a` in one of the three ranges.
// The adapter matches sidecar entries first, and a sidecar entry for the same
// item still wins.
inline bool IsFlightCharm(double t, double a, double b, double c, double j, Rolled& out)
{
    long long tt = 0, aa = 0, bb = 0, cc = 0, jj = 0;
    if (!AsWhole(t, tt) || !AsWhole(a, aa) || !AsWhole(b, bb) || !AsWhole(c, cc) || !AsWhole(j, jj)) return false;
    if (tt != kCharmTypeT || bb != kCharmBaseB || cc != 0 || jj != 0) return false;
    return DecodeSeed(aa, out);
}

// The stats dressing writes, as (stat id, value), in table order.
inline std::vector<std::pair<int, int>> StatsFor(const Rolled& r)
{
    std::vector<std::pair<int, int>> out;
    for (int k = 0; k < r.count && k < kMaxCharmStats; ++k) out.emplace_back(r.stat[k], r.value[k]);
    return out;
}

// ---- flight: what it answers ---------------------------------------------------

// What a hooked call gets back. Real keeps the game's own answer; the other
// two are written into the result instead.
enum class Answer : int {
    Real,
    Noone,   // the instance-returning queries: no instance there
    False,   // the boolean meeting queries: nothing met
};

inline constexpr std::string_view AnswerName(Answer a)
{
    switch (a) {
    case Answer::Real:  return "real";
    case Answer::Noone: return "noone";
    case Answer::False: return "false";
    }
    return "?";
}

// GameMaker's `noone`.
inline constexpr double kNoone = -4.0;

// The rows flight answers: the three builtins Live 1's S7 measured answering
// (position_meeting, place_meeting, collision_line), the two jumpscenery hooks
// beside them, and InstancePlaceTallest. `objectArg` is the position of the
// object argument; `points` how many points the query names (collision_line
// both ends, the rest one); `rerun` whether a blocked answer is re-asked
// against Wall_Parent_obj and Lock_obj before it is answered.
// InstancePlaceTallest is not re-run: its own answer, an instance, is tested
// instead.
enum class Hooked : int { PositionMeeting, PlaceMeeting, InstancePosition, CollisionLine, CollisionCircle, InstancePlaceTallest };
inline constexpr int kHookedCount = 6;

struct HookedRow {
    std::string_view name;
    int              objectArg;
    Answer           answer;
    int              points;
    bool             rerun;
};

inline constexpr HookedRow kHooked[kHookedCount] = {
    { "position_meeting",     2, Answer::False, 1, true },
    { "place_meeting",        2, Answer::False, 1, true },
    { "instance_position",    2, Answer::Noone, 1, true },
    { "collision_line",       4, Answer::Noone, 2, true },
    { "collision_circle",     3, Answer::Noone, 1, true },
    { "InstancePlaceTallest", 2, Answer::Noone, 1, false },
};

// off: no charm worn. on: one is worn. residual: none is worn, but the player
// still stood inside the family or a monster at the last read, so flight keeps
// answering (monsters included) until a read comes back free and the player
// can walk out.
enum class State : int { Off, On, Residual };

inline constexpr std::string_view StateName(State s)
{
    switch (s) {
    case State::Off:      return "off";
    case State::On:       return "on";
    case State::Residual: return "residual";
    }
    return "?";
}

// The first four are the stat line's. The rest are the trace line's (TraceLine):
// they say how far a quiet flight got, and every return of OnQuery and
// OnUpdateDepth lands in exactly one of them, so a zero always has a cause.
//
// Queries: every hooked call handed to the core (OnQuery, or NoteCall for a
// call the adapter returned from before building a Query) counts once in
// `calls`, then in exactly one of notPlayer, unblocked and seen:
//   calls = notPlayer + unblocked + seen
// and every blocked query of the local player (`seen`) in exactly one of
//   seen = idle + unconfigured + unreadable + other + excluded + outsideRoom + answered
// The two monster counts are parts of that sum, not buckets of their own:
// `monster` is inside `answered` (a monster answered on place_meeting) and
// `monsterKept` inside `other` (a monster named on any other row, or an
// InstancePlaceTallest monster answer, kept).
//
// UpdateDepth: every call handed to the core counts once in `depthCalls`, then
//   depthCalls = depthOther + depthSeen
//   depthSeen  = depthIdle + noFront + depth
//
// The layer_get_fx reads, over the session (the census's, and the re-choice's
// for a layer that became visible and empty after it): each in exactly one of
// fxStruct (an effect), fxOther (an answer that is not a struct) and fxFailed
// (the read threw or was refused). The last two both class the layer as no
// effect; they are counted so a runner whose read never works shows it. A
// layer whose effect was never read counts in none of them; each visit's
// CensusTally (`flightprobe depth`'s `unread=`) says why the census did not.
//
// The adapter hands every hooked call to the core (OnQuery, NoteCall,
// OnUpdateDepth, NoteDepthOther) except at three returns, which count nothing:
//   - no self: there is nothing to compare;
//   - flight's own re-entry (g_FlightBusy): it is flight re-running the
//     original, and counting it would break the sums above;
//   - flight not installed: the stat line's `install=` already says so.
// FlightOnBuiltin also returns, uncounted, for a row outside the five.
struct FlightCounters {
    uint64_t answered = 0;      // queries answered "nothing there"
    uint64_t excluded = 0;      // kept because a wall or a lock blocked, or was named
    uint64_t outsideRoom = 0;   // kept because a point was outside the room (or the room unreadable)
    uint64_t depth = 0;         // flight-layer writes
    uint64_t calls = 0;         // hooked calls handed to the core, any self
    uint64_t notPlayer = 0;     // of calls, self was not the local player
    uint64_t unblocked = 0;     // of calls, the local player's, whose original answered free: it stands
    uint64_t seen = 0;          // of calls, the local player's blocked queries
    uint64_t idle = 0;          // of seen, kept because flight was off
    uint64_t unconfigured = 0;  // of seen, kept because the family or an exclusion did not resolve
    uint64_t unreadable = 0;    // of seen, kept because the adapter could not read the object (-1)
    uint64_t other = 0;         // of seen, kept because the object named is outside the family
    uint64_t depthCalls = 0;    // UpdateDepth calls handed to the core, any self
    uint64_t depthOther = 0;    // of depthCalls, self was not the local player
    uint64_t depthSeen = 0;     // of depthCalls, the local player's
    uint64_t depthIdle = 0;     // of depthSeen, no write because flight was not on (off or residual)
    uint64_t noFront = 0;       // of depthSeen while on, no write: the room unreadable, or no flight layer (the census yielded none), or no depth strictly behind the rearmost filter
    uint64_t fxStruct = 0;      // effect reads that answered a struct: an effect
    uint64_t fxOther = 0;       // effect reads that answered anything else: no effect
    uint64_t fxFailed = 0;      // effect reads that failed: no effect
    uint64_t monster = 0;       // of answered, a monster named on place_meeting
    uint64_t monsterKept = 0;   // of other, a monster named on another row, or an InstancePlaceTallest monster, kept
};

// One hooked call, after its original ran.
struct Query {
    Hooked  row;
    int64_t frame;
    bool    playerSelf;     // the call's self is the local player
    int     object;         // the object the query names; for InstancePlaceTallest, the object of
                            // the instance it answered; -1 when it is not an object index
    bool    reallyBlocked;  // the original's own answer: an instance, or true
    double  x0 = 0.0;       // the point: x, y for the meeting and position queries, the centre
    double  y0 = 0.0;       // for collision_circle, the first end for collision_line
    double  x1 = 0.0;       // collision_line's second end; unused by the other rows
    double  y1 = 0.0;
};

// ---- drawing in front: the flight layer ----------------------------------------
//
// While flight is on, the local player is drawn on a room layer of the mod's
// own, `fp_flight`, made in the current room. GameMaker draws a lower layer
// depth on top. Live 1 measured that no layer of global.gameLayer draws in
// front of a room's Tree_Leaves (RDM § 19.7), so the flight layer goes one
// below the lowest depth among the game layers' front end and every drawn room
// layer behind the rearmost filter layer. The owner's rule (2026-10-13, "Never
// past a filter"): the flying player is never drawn in front of a filter layer,
// so he keeps every tint he has on the ground.

inline constexpr std::string_view kFlightLayerName = "fp_flight";
// The re-choice runs at most this often, and a census that yielded nothing is
// retried no sooner.
inline constexpr double kRechoiceMs = 1000.0;

// A layer id and its layer_get_depth: an end of global.gameLayer[room], or a
// room layer as the census lists it.
struct LayerEnd {
    int64_t layer = -1;
    double  depth = 0.0;
};

struct DepthWrite {
    bool    write = false;
    int64_t layer = -1;
};

// The game layers' front end: the lower of global.gameLayer[room]'s two end
// depths (Live 1 read the last index at -5500 and index 0 at 0). A non-finite
// end names none, and nothing is written.
inline bool GameFrontDepth(const LayerEnd& first, const LayerEnd& last, double& depth)
{
    if (!std::isfinite(first.depth) || !std::isfinite(last.depth)) return false;
    depth = first.depth < last.depth ? first.depth : last.depth;
    return true;
}

// One room layer in front of the game layers' front end, as the census read
// it: layer_get_name, layer_get_visible, the length of layer_get_all_elements,
// and, for a visible empty layer only, whether layer_get_fx answered a struct.
// The re-choice reads the visibility and the count again, and the effect of a
// layer that has become visible and empty without it ever having been read.
struct RoomLayer {
    int64_t     id = -1;
    double      depth = 0.0;
    std::string name;
    bool        visible = false;
    int64_t     elements = 0;
    bool        effect = false;
    bool        effectRead = false;   // layer_get_fx was asked once, whatever it answered
};

// A filter layer: visible, holding no element, carrying a layer effect, and in
// front of the game layers' front end. A layer that holds an element is never
// a filter, so an effect-carrying instance layer (Tree_Leaves) is one only
// while it is empty. The flight layer is never one.
inline bool IsFilterLayer(const RoomLayer& l, double gameFront)
{
    return l.name != kFlightLayerName && std::isfinite(l.depth) && l.depth < gameFront && l.visible
        && l.elements == 0 && l.effect;
}

// A drawn layer: visible and holding at least one element. The flight layer is
// never one (it holds the flying player).
inline bool IsDrawnLayer(const RoomLayer& l)
{
    return l.name != kFlightLayerName && std::isfinite(l.depth) && l.visible && l.elements > 0;
}

// The flight layer's depth: one below the lowest of the game layers' front end
// and every drawn layer in front of it and behind the rearmost filter (the
// filter with the highest depth). With no filter, every drawn layer counts.
// `by` is the index in `layers` of the drawn layer that decided, -1 when the
// game layers' front end did. Rearmost, not frontmost: a drawn layer between
// two filters (Act_09_05's Smoke) stays in front of the flying player.
// With a filter, the depth lies strictly behind it: never at its depth, since
// layers of equal depth draw in an order that ignores depth, so the player
// could draw past the filter ("Never past a filter", the owner, 2026-10-13).
// Whole depths only (every measured depth was whole; whether the runner keeps
// a fractional one is not established), so when no whole depth lies strictly
// between the rearmost filter and that lowest depth (a filter at the front end
// minus one with nothing drawn behind it, or a drawn layer one behind the
// filter), there is no choice: `ok` is false and nothing is written.
struct FlightDepthChoice {
    bool   ok = false;
    double depth = 0.0;
    int    by = -1;
};

// The rearmost filter layer in `layers`, the one with the highest depth: its
// index, or -1 when there is none. On the element counts the layers carry, so
// after a re-choice it names the filter in force (`flightprobe depth`'s
// `filter=`).
inline int RearmostFilter(double gameFront, const std::vector<RoomLayer>& layers)
{
    int at = -1;
    for (size_t i = 0; i < layers.size(); ++i) {
        if (!IsFilterLayer(layers[i], gameFront)) continue;
        if (at < 0 || layers[i].depth > layers[(size_t)at].depth) at = static_cast<int>(i);
    }
    return at;
}

inline FlightDepthChoice ChooseFlightDepth(double gameFront, const std::vector<RoomLayer>& layers)
{
    FlightDepthChoice c;
    if (!std::isfinite(gameFront)) return c;
    const int filter = RearmostFilter(gameFront, layers);
    const bool haveFilter = filter >= 0;
    const double rearmost = haveFilter ? layers[(size_t)filter].depth : 0.0;
    double lowest = gameFront;
    for (size_t i = 0; i < layers.size(); ++i) {
        const RoomLayer& l = layers[i];
        if (!IsDrawnLayer(l) || l.depth >= gameFront) continue;
        if (haveFilter && l.depth <= rearmost) continue;   // in front of the rearmost filter: it stays in front
        if (l.depth < lowest) {
            lowest = l.depth;
            c.by = static_cast<int>(i);
        }
    }
    if (!haveFilter) {
        c.ok = true;
        c.depth = lowest - 1.0;
        return c;
    }
    // The nearest whole depth in front of `lowest`, and only if it is still
    // behind the rearmost filter.
    const double depth = std::ceil(lowest) - 1.0;
    if (!(depth > rearmost)) {
        c.by = -1;
        return c;
    }
    c.ok = true;
    c.depth = depth;
    return c;
}

// What one census did with each layer the layer list handed it: every entry in
// exactly one outcome, tested in this order, the first match winning.
//   noDepth   its depth is not finite (the adapter hands an entry whose id or
//             depth did not read over with a non-finite depth);
//   behind    at or behind the game layers' front end: the player's ordinary
//             draw is already in front of it;
//   noDetail  its name, visibility or element count did not read;
//   own       the flight layer itself;
//   hidden    it reads hidden;
//   holds     visible and holding an element (a count other than 0): drawn;
// and otherwise its layer_get_fx is read: fxStruct (an effect), fxOther (read,
// and not a struct) or fxFailed (the read threw or was refused). So
//   listed = fxStruct + fxOther + fxFailed + noDepth + behind + noDetail + own + hidden + holds
// and a `filter=none` names its cause: an answer of another kind, a failed
// read, or a layer whose effect was never read, and why.
struct CensusTally {
    uint64_t fxStruct = 0;
    uint64_t fxOther = 0;
    uint64_t fxFailed = 0;
    uint64_t behind = 0;
    uint64_t noDepth = 0;
    uint64_t noDetail = 0;
    uint64_t own = 0;
    uint64_t hidden = 0;
    uint64_t holds = 0;
};

// The tally as `flightprobe depth` prints it, the one rendering there is:
// `fx=<struct>/<other>/<failed> unread=behind:<n>,no-depth:<n>,no-detail:<n>,own:<n>,hidden:<n>,holds:<n>`.
inline std::string CensusTallyText(const CensusTally& t)
{
    return "fx=" + std::to_string(t.fxStruct) + "/" + std::to_string(t.fxOther) + "/" + std::to_string(t.fxFailed)
        + " unread=behind:" + std::to_string(t.behind) + ",no-depth:" + std::to_string(t.noDepth)
        + ",no-detail:" + std::to_string(t.noDetail) + ",own:" + std::to_string(t.own)
        + ",hidden:" + std::to_string(t.hidden) + ",holds:" + std::to_string(t.holds);
}

// This room visit's flight layer, for `flightprobe depth`. A visit starts at
// the census and ends when the flight layer no longer exists (layers die with
// their room) or the room changes.
struct FlightLayerVisit {
    bool                   have = false;           // a flight layer this visit
    int64_t                layer = -1;
    double                 depth = 0.0;
    int64_t                room = -1;
    bool                   gameFrontRead = false;
    double                 gameFront = 0.0;
    int                    by = -1;                // ChooseFlightDepth's `by`, into `layers`
    std::vector<RoomLayer> layers;                 // remembered: in front of the game layers' front end
    size_t                 listed = 0;             // every entry the layer list handed the census, unreadable ones included
    CensusTally            tally;                  // what the census did with each of them; the re-choice leaves it
    double                 censusMs = 0.0;
    uint64_t               moves = 0;              // re-choices that moved the layer this visit
    bool                   noChoice = false;       // the last re-choice found no depth behind the filter: no write until one does
};

// ---- drops: the gate ------------------------------------------------------------

// The ` gate=` field, as #74's ` angelicDrops=` is.
enum class Gate : int { Off, On, Refused, NotResolved, NoDetection };

inline constexpr std::string_view GateName(Gate g)
{
    switch (g) {
    case Gate::Off:         return "off";
    case Gate::On:          return "on";
    case Gate::Refused:     return "refused";
    case Gate::NotResolved: return "not-resolved";
    case Gate::NoDetection: return "no-detection";
    }
    return "?";
}

// ---- the `flightcharm stat` line -----------------------------------------------

// ` elf=<ours>/<built>`: drops that became the charm, of the stand-in's hits built.
struct DropTally {
    uint64_t ours = 0;
    uint64_t built = 0;
};

// Pocket Elf's typed-build counters on the kill draw (the draw's positive
// control, #166), over the CreateDefaultParams calls made in DropItem calls
// that carry its entry, each counted once before any coin: a typed pick
// (`standIn`, its whole triple Pocket Elf's stand-in; `natural`, every other),
// a unique build no read matched (`untyped`), a build whose third argument
// does not read as true the way the game reads it (`normal`, never typed), and
// a build while the draw's scope was false,
// under its first false condition (busy, aside, lists, roll).
struct DrawTypedCounts {
    uint64_t standIn = 0;
    uint64_t natural = 0;
    uint64_t untyped = 0;
    uint64_t normal = 0;
    uint64_t busy = 0;
    uint64_t aside = 0;
    uint64_t lists = 0;
    uint64_t roll = 0;
    // The class fields (#166 replan 1), over the same in-scope builds as
    // typed, untyped and normal: the third argument's kind and truth as the
    // runner's truth rule reads it (`bool`, `num` - real, int32 and int64 -
    // each true / false; `none` absent, undefined or unset; `other` any other
    // kind), and the check of the gate's verdict against the `c` the game's
    // CreateDefaultParams stored (agree, disagree, unread).
    uint64_t boolTrue = 0;
    uint64_t boolFalse = 0;
    uint64_t numTrue = 0;
    uint64_t numFalse = 0;
    uint64_t none = 0;
    uint64_t other = 0;
    uint64_t checkAgree = 0;
    uint64_t checkDisagree = 0;
    uint64_t checkUnread = 0;
};

// Every field of the line. A default StatFields prints the fresh session's
// line; Flight::FillStat and SizeGuard::FillStat fill their parts, and the
// adapter the rest.
struct StatFields {
    bool           switches[kCharmCount] = { false, false, false };
    std::string    install = "not-armed";
    std::string    state = "off";
    unsigned       equipped = 0;
    FlightCounters flight;
    std::string    icons = "not-loaded";
    uint64_t       ground = 0;
    uint64_t       guardKept = 0;
    uint64_t       guardFailed = 0;
    Gate           gate = Gate::Off;
    DropTally      drops[kCharmCount];
    uint64_t       injected = 0;
    uint64_t       refused = 0;
    uint64_t       anomalies = 0;
    std::string    standinElf = "none";
    // Pocket Elf's kill route on DropItem's own draw (#166). The adapter sets
    // the hook's route; ElfDraw::FillStat the rest.
    std::string    elfDrawHook = "not-installed";
    uint64_t       elfDrawCalls = 0;
    uint64_t       elfDrawPushed = 0;
    DropTally      elfDraw;
    std::string    elfDrawStandIn = "none";
    DrawTypedCounts elfDrawTyped;   // the adapter sets these
};

// The worn charms' keys, in table order, joined by `+`; `none` for none. Keys,
// not display names: a name's space would split the field.
inline std::string EquippedText(unsigned mask)
{
    std::string s;
    for (const CharmRow& r : kCharms) {
        if (!(mask & CharmBit(r.charm))) continue;
        if (!s.empty()) s += "+";
        s += r.key;
    }
    return s.empty() ? std::string("none") : s;
}

// `flightcharm stat`. The fields up to standin-elf= are the requirement, in
// this order; anything added goes after them, as the kill draw's `elf-draw-`
// fields do (#166).
inline std::string StatLine(const StatFields& f)
{
    std::string s = "flightcharm: switches";
    for (int i = 0; i < kCharmCount; ++i)
        s += " " + std::string(kCharms[i].key) + "=" + (f.switches[i] ? "on" : "off");
    s += " | flight install=" + f.install + " state=" + f.state + " equipped=" + EquippedText(f.equipped);
    s += " answered=" + std::to_string(f.flight.answered);
    s += " excluded=" + std::to_string(f.flight.excluded);
    s += " outside-room=" + std::to_string(f.flight.outsideRoom);
    s += " depth=" + std::to_string(f.flight.depth);
    s += " | icons=" + f.icons + " ground=" + std::to_string(f.ground);
    s += " | guard kept=" + std::to_string(f.guardKept) + " failed=" + std::to_string(f.guardFailed);
    s += " | drops gate=" + std::string(GateName(f.gate));
    for (int i = 0; i < kCharmCount; ++i)
        s += " " + std::string(kCharms[i].key) + "=" + std::to_string(f.drops[i].ours) + "/" + std::to_string(f.drops[i].built);
    s += " injected=" + std::to_string(f.injected);
    s += " refused=" + std::to_string(f.refused);
    s += " anomalies=" + std::to_string(f.anomalies);
    s += " standin-elf=" + f.standinElf;
    s += " elf-draw-hook=" + f.elfDrawHook;
    s += " elf-draw-calls=" + std::to_string(f.elfDrawCalls);
    s += " elf-draw-pushed=" + std::to_string(f.elfDrawPushed);
    s += " elf-draw=" + std::to_string(f.elfDraw.ours) + "/" + std::to_string(f.elfDraw.built);
    s += " elf-draw-standin=" + f.elfDrawStandIn;
    const DrawTypedCounts& t = f.elfDrawTyped;
    s += " elf-draw-typed=" + std::to_string(t.standIn) + "/" + std::to_string(t.natural);
    s += " elf-draw-untyped=" + std::to_string(t.untyped);
    s += " elf-draw-normal=" + std::to_string(t.normal);
    s += " elf-draw-outside=busy:" + std::to_string(t.busy) + ",aside:" + std::to_string(t.aside) + ",lists:"
        + std::to_string(t.lists) + ",roll:" + std::to_string(t.roll);
    s += " elf-draw-class=bool:" + std::to_string(t.boolTrue) + "/" + std::to_string(t.boolFalse) + ",num:"
        + std::to_string(t.numTrue) + "/" + std::to_string(t.numFalse) + ",none:" + std::to_string(t.none) + ",other:"
        + std::to_string(t.other);
    s += " elf-draw-class-check=" + std::to_string(t.checkAgree) + "/" + std::to_string(t.checkDisagree) + "/"
        + std::to_string(t.checkUnread);
    return s;
}

// ---- the worn-charm cache (#216 b1) ---------------------------------------------
// What tells the cache the worn charms may have changed, each counted on its
// own: an UpdateCharmData call (the script that rebuilds the worn-charm array:
// a static reading, RDM § 23.1; Live 1 measured one call per equip change and
// none while standing or walking), the local player becoming another instance
// (a room or character change), and the Custom Forge dressing a charm (the
// `fp_mechanic` the read tests is written then).
enum class WornInvalidation { UpdateCharmData, Player, Dress };

struct WornCacheCounters {
    uint64_t reads = 0;    // the cache's own reads of the worn charms: invalid, or the safety period up
    uint64_t ucd = 0;      // invalidations by an UpdateCharmData call
    uint64_t player = 0;   // ... by the local player changing
    uint64_t dress = 0;    // ... by a charm being dressed
    uint64_t safety = 0;   // reads made only because the safety period was up
    uint64_t stale = 0;    // of those, the ones that found a mask other than the remembered one
};

// `flightcharm stat`'s cache line after its `route=`, which the adapter writes
// (how the signal went in: not-installed, detour, table-only, not-found).
// `worn=` is the mask the last refresh answered, `fresh=` a read made for the
// line alone (`unread` when it failed), and `agree=` whether the two match; a
// failed fresh read never agrees. Live procedure 2 checks these field names.
inline std::string CacheFields(const WornCacheCounters& c, unsigned worn, bool freshRead, unsigned fresh)
{
    std::string s = "reads=" + std::to_string(c.reads);
    s += " invalidations=ucd:" + std::to_string(c.ucd) + ",player:" + std::to_string(c.player) + ",dress:"
        + std::to_string(c.dress);
    s += " safety=" + std::to_string(c.safety);
    s += " stale=" + std::to_string(c.stale);
    s += " worn=" + EquippedText(worn);
    s += " fresh=" + (freshRead ? EquippedText(fresh) : std::string("unread"));
    s += " agree=" + std::string(freshRead && fresh == worn ? "yes" : "no");
    return s;
}

// ---- flight ----------------------------------------------------------------------

class Flight {
public:
    // Is `ancestor` an ancestor of `object`? (object_is_ancestor: an object is
    // not its own ancestor.) Asked once per object, then remembered.
    using IsAncestorFn = std::function<bool(int object, int ancestor)>;
    // The flight charms in the local player's localCharmItemArray, as
    // CharmBit()s; false when the array cannot be read (counts as none).
    using EquippedFn = std::function<bool(unsigned& mask)>;
    // The saved original place_meeting at the player's own x and y against
    // Collision_Parent_obj and then Enemy_Parent_obj, with the player as self;
    // overlapping when either meets, so a charm taken off inside a monster
    // stays residual until both reads are free. False when unreadable (counts
    // as free, so flight goes off: the game's own answers again).
    using OverlapFn = std::function<bool(bool& overlapping)>;
    // room_width / room_height; false when they cannot be read.
    using RoomSizeFn = std::function<bool(double& width, double& height)>;
    // global.gameLayer[room]'s first and last entries with their depths;
    // false when they cannot be read.
    using LayerEndsFn = std::function<bool(int64_t room, LayerEnd& first, LayerEnd& last)>;
    // The flight layer's reads and writes, each a builtin the adapter calls by
    // name in the current room. Every one may fail (false), and none is called
    // per frame except `valid`, once before each write.
    struct LayerOps {
        // layer_get_all, each with its layer_get_depth. The census only.
        std::function<bool(std::vector<LayerEnd>& all)> all;
        // A listed layer's name, visibility and element count. The census only,
        // for a layer in front of the game layers' front end.
        std::function<bool(int64_t id, RoomLayer& layer)> detail;
        // Whether layer_get_fx answers a struct, for a visible empty layer: at
        // the census, or at the re-choice for a remembered layer that has
        // become visible and empty since; once per layer per visit. A failed
        // read is no effect.
        std::function<bool(int64_t id, bool& effect)> effect;
        // A remembered layer's visibility and element count again. The
        // re-choice only.
        std::function<bool(int64_t id, bool& visible, int64_t& elements)> count;
        // The layer named kFlightLayerName, looked up by name (layer_get_id)
        // and moved to `depth`, or created at `depth` by name when the room
        // holds none. The census only, so a room never holds two.
        std::function<bool(double depth, int64_t& id)> place;
        // layer_depth: the re-choice moving the flight layer.
        std::function<bool(int64_t id, double depth)> move;
        // layer_exists and layer_get_name: the cached layer still exists and
        // carries our name (a new room may reuse the id). Before each write.
        std::function<bool(int64_t id)> valid;
        // A steady clock in milliseconds, for the re-choice's schedule and the
        // census's time. Unset, the frame counter stands in at 60 a second.
        std::function<double()> clock;
    };

    // The family table forgets everything once it holds this many objects, so
    // an adapter passing something other than an object index cannot grow it
    // without bound.
    static constexpr size_t kFamilyTableMax = 65536;

    void SetIsAncestor(IsAncestorFn fn) { isAncestor_ = std::move(fn); familyTable_.clear(); }

    // The family (Collision_Parent_obj) and the excluded objects
    // (Wall_Parent_obj, Lock_obj), as indices the adapter resolved by name. A
    // -1 in either means a name did not resolve, and flight then answers for
    // nothing. The exclusion list is the adapter's one edit away.
    void SetFamily(int family, std::vector<int> excluded)
    {
        family_ = family;
        excluded_ = std::move(excluded);
        familyTable_.clear();
    }

    // The monster object (Enemy_Parent_obj), as an index the adapter resolved
    // by name. A -1 answers for nothing: no object is then a monster, and a
    // monster query keeps its answer as any other object's does. The family's
    // own rule does not depend on it.
    void SetMonster(int monster)
    {
        monster_ = monster;
        familyTable_.clear();
    }

    void SetEquipped(EquippedFn fn) { equipped_ = std::move(fn); }
    void SetOverlap(OverlapFn fn) { overlap_ = std::move(fn); }
    void SetRoomSize(RoomSizeFn fn) { roomSize_ = std::move(fn); }
    void SetLayerEnds(LayerEndsFn fn) { layerEnds_ = std::move(fn); visit_ = FlightLayerVisit{}; haveCensus_ = false; }
    void SetLayerOps(LayerOps ops) { ops_ = std::move(ops); visit_ = FlightLayerVisit{}; haveCensus_ = false; }

    bool Configured() const
    {
        if (family_ < 0) return false;
        for (int e : excluded_) if (e < 0) return false;
        return true;
    }

    // ---- the install: armed by a charm, hooked once a character wears one ----
    // Dressing arms flight the first time it recognises a flight charm; a
    // hook is never installed from inside CreateItemNew. While armed, the
    // adapter's frame tick looks for the local player on the frames
    // LooksForPlayer picks (HhResolveLocalPlayer, which accepts the runner's
    // VALUE_REF); each look that finds one reads the worn charms and reports
    // both through NoteLook. The builtins, InstancePlaceTallest and
    // UpdateDepth install once, at the first look that finds a flight charm
    // worn (#216 b2): a charm carried in the bag or stash arms flight but
    // costs nothing per frame beyond that look, once a second. A refusal (a
    // table-only hook, a research hook holding one of them) is final for the
    // session; the adapter prints RefusedLine once. Nothing runs per frame
    // before arming, and once installed the hooks stay.
    static constexpr unsigned long long kInstallPollFrames = 60;
    static constexpr bool LooksForPlayer(unsigned long long armedFrames)
    {
        return armedFrames % kInstallPollFrames == 0;
    }
    void Arm() { armed_ = true; }
    bool Armed() const { return armed_; }
    bool ShouldInstall(bool setupDone, bool playerResolved, bool charmWorn) const
    {
        return armed_ && !installed_ && !refused_ && setupDone && playerResolved && charmWorn;
    }
    // What the latest look found: whether the local player resolved and, if
    // so, whether a flight charm is worn. Only InstallStateName reads it.
    void NoteLook(bool playerResolved, bool charmWorn)
    {
        lookFoundPlayer_ = playerResolved;
        lookFoundWorn_ = playerResolved && charmWorn;
    }
    void NoteInstall(bool ok)
    {
        if (ok) installed_ = true;
        else refused_ = true;
    }
    bool Installed() const { return installed_; }
    // ` install=`: installed, refused; while armed, waiting-for-worn-charm when
    // the latest look found the player wearing no flight charm, and
    // waiting-for-character otherwise; not-armed before arming.
    std::string_view InstallStateName() const
    {
        if (installed_) return "installed";
        if (refused_) return "refused";
        if (!armed_) return "not-armed";
        return lookFoundPlayer_ && !lookFoundWorn_ ? "waiting-for-worn-charm" : "waiting-for-character";
    }

    // ---- the state, refreshed at the point of use ----------------------------
    // At most once a frame: the first player-self call of a frame reads the
    // worn charms, and, only when none is worn after flight was on or
    // residual, whether the player still overlaps the family or a monster. `flightcharm
    // stat` refreshes too (RefreshForStat), so what it prints never depends on
    // a hooked call having run since the last equip. With the worn-charm
    // cache on (SetWornCache, below), the worn charms are read only when the
    // cache says so; the rest is the same either way.
    void Refresh(int64_t frame)
    {
        if (haveFrame_ && frame == frame_) return;
        haveFrame_ = true;
        frame_ = frame;
        const unsigned mask = WornAt(frame);
        equippedMask_ = mask;
        if (mask != 0) {
            state_ = State::On;
            return;
        }
        if (state_ == State::Off) return;
        bool inside = false;
        state_ = (ReadOverlap(inside) && inside) ? State::Residual : State::Off;
    }

    // Before `flightcharm stat` (and flightprobe's depth and stat) fill the
    // line. A player standing still may make no hooked call at all: nothing
    // measured shows UpdateDepth or a collision query running while the player
    // stands, so without this a charm equipped in place would print
    // `state=off equipped=none` until the next step. The adapter passes the
    // frame counter its queries use and whether the local player resolved;
    // the overlap read needs the player as self, so an unresolved player
    // reads nothing and leaves the state as it was. Nothing is read before
    // arming, so the fresh session's line stays the same.
    void RefreshForStat(int64_t frame, bool playerResolved)
    {
        if (armed_ && playerResolved) Refresh(frame);
    }

    // Refresh, then the state: what a hook asks before it builds a Query
    // (#216 c4). Still at most one read of the worn charms a frame, so every
    // later call of the frame answers from what the first one read.
    State StateAt(int64_t frame)
    {
        Refresh(frame);
        return state_;
    }

    State CurrentState() const { return state_; }
    unsigned EquippedMask() const { return equippedMask_; }

    // ---- the worn-charm cache (#216 b1) ----------------------------------------
    // Off by default, and then Refresh reads the worn charms once a frame as
    // above. The adapter turns it on only when its signal, UpdateCharmData,
    // went in as an inline detour: a table-only hook would miss the game's own
    // direct calls and leave a wrong mask standing. On, Refresh re-reads only
    // after an invalidation (or before any read), and otherwise answers with
    // the remembered mask; once the safety period has passed since the last
    // read it reads anyway, since the array's other writers are not
    // established. A safety read that finds another mask counts as stale and
    // takes it. Everything after the mask is unchanged, so the residual overlap
    // read still runs once a frame while residual. A failed read answers none,
    // as before, and leaves the cache invalid, so the next frame reads again.
    static constexpr int64_t kWornSafetyFrames = static_cast<int64_t>(kInstallPollFrames);
    void SetWornCache(bool on)
    {
        wornCacheOn_ = on;
        wornValid_ = false;
    }
    bool WornCacheOn() const { return wornCacheOn_; }
    // Counted whether the cache is on or not; reads nothing.
    void InvalidateWorn(WornInvalidation why)
    {
        wornValid_ = false;
        switch (why) {
        case WornInvalidation::UpdateCharmData: ++wornCounters_.ucd; break;
        case WornInvalidation::Player:          ++wornCounters_.player; break;
        case WornInvalidation::Dress:           ++wornCounters_.dress; break;
        }
    }
    const WornCacheCounters& WornCache() const { return wornCounters_; }

    // ---- the local player, confirmed at the point of use (#216 c3) -----------
    // The adapter's frame counter advances at the end of a frame, but the
    // hooks compare `self` against the local player during the next frame's
    // Step, so a player confirmed only at the frame's end is the previous
    // frame's (a room change gives a new instance). Each hook asks this before
    // its `self` compare: true for the first call of a frame, which then
    // resolves the player again; false for every later call of that frame,
    // which costs the adapter one compare. Reads nothing, and is apart from
    // Refresh's own once-a-frame mark.
    bool ConfirmsPlayer(int64_t frame)
    {
        if (havePlayerFrame_ && frame == playerFrame_) return false;
        havePlayerFrame_ = true;
        playerFrame_ = frame;
        return true;
    }

    // One hooked call, after its original ran. `excludedBlocks()` re-runs the
    // same original, same arguments and self, against Wall_Parent_obj and
    // then Lock_obj, and says whether either is there. It is asked only for a
    // blocked, family-named query (or a monster-named place_meeting) of the
    // local player while flight is on or residual, with every point inside the
    // room, and never for InstancePlaceTallest. Any other self returns at
    // once, reading nothing.
    //
    // Every return is counted (FlightCounters): another self in `notPlayer`, a
    // free answer in `unblocked`, and each kept or answered blocked query in one
    // bucket of `seen`. Out-of-range rows are not hooked calls and count
    // nothing.
    template <class ExcludedBlocksFn>
    Answer OnQuery(const Query& q, ExcludedBlocksFn&& excludedBlocks)
    {
        const int i = static_cast<int>(q.row);
        if (i < 0 || i >= kHookedCount) return Answer::Real;
        ++counters_.calls;
        if (!q.playerSelf) {
            ++counters_.notPlayer;
            return Answer::Real;
        }
        Refresh(q.frame);
        if (!q.reallyBlocked) {
            ++counters_.unblocked;
            return Answer::Real;
        }
        ++counters_.seen;
        if (state_ == State::Off) {
            ++counters_.idle;
            return Answer::Real;
        }
        if (!Configured()) {
            ++counters_.unconfigured;
            return Answer::Real;
        }
        if (q.object < 0) {
            ++counters_.unreadable;
            return Answer::Real;
        }
        const Class c = Classify(q.object);
        if (!c.family && c.monster) {
            // A monster (the owner, 2026-10-13: body collision only). Only
            // place_meeting, the mask's own test, is answered; a point or line
            // query at a monster may be how a target is picked, so every other
            // row, and an InstancePlaceTallest monster answer, keeps it.
            if (q.row != Hooked::PlaceMeeting) {
                ++counters_.other;
                ++counters_.monsterKept;
                return Answer::Real;
            }
            if (c.excluded) {
                ++counters_.excluded;
                return Answer::Real;
            }
            if (!InsideRoom(q, kHooked[i].points)) {
                ++counters_.outsideRoom;
                return Answer::Real;
            }
            bool wall = true;
            try { wall = static_cast<bool>(excludedBlocks()); } catch (...) { wall = true; }
            if (wall) {
                ++counters_.excluded;
                return Answer::Real;
            }
            ++counters_.answered;
            ++counters_.monster;
            return kHooked[i].answer;
        }
        if (!c.family) {
            ++counters_.other;
            return Answer::Real;
        }
        if (c.excluded) {
            ++counters_.excluded;
            return Answer::Real;
        }
        if (!InsideRoom(q, kHooked[i].points)) {
            ++counters_.outsideRoom;
            return Answer::Real;
        }
        if (kHooked[i].rerun) {
            bool wall = true;
            try { wall = static_cast<bool>(excludedBlocks()); } catch (...) { wall = true; }
            if (wall) {
                ++counters_.excluded;
                return Answer::Real;
            }
        }
        ++counters_.answered;
        return kHooked[i].answer;
    }

    // The same, with no re-run available: a blocked re-run row keeps its
    // answer. Meant for InstancePlaceTallest, which is never re-run.
    Answer OnQuery(const Query& q)
    {
        return OnQuery(q, []() { return true; });
    }

    // A hooked call the adapter returns from before building a Query (another
    // self, or the original answered free), so that the call still counts:
    // `calls` and then `notPlayer` or `unblocked`, exactly as OnQuery would count
    // it. Reads nothing and refreshes nothing, so another self still costs
    // the adapter one compare and one increment. A blocked call of the local
    // player belongs to OnQuery, not here; passed one, it counts as free.
    void NoteCall(bool playerSelf)
    {
        ++counters_.calls;
        if (!playerSelf) ++counters_.notPlayer;
        else ++counters_.unblocked;
    }

    // An UpdateDepth call of another self the adapter returns from at once:
    // `depthCalls` and `depthOther`, as OnUpdateDepth would count it.
    void NoteDepthOther()
    {
        ++counters_.depthCalls;
        ++counters_.depthOther;
    }

    // After the game's UpdateDepth ran: whether to set the player's `layer`,
    // and to which layer. Only for the local player, only while flight is on
    // (not residual). The write goes to this room visit's flight layer, which
    // is validated first (LayerOps::valid); a failed check, or a new room,
    // starts a new visit, whose census runs once and places the layer. While
    // the visit lasts, the re-choice runs at most once a second. A negative
    // room is one the adapter could not read: no write, counted in noFront.
    // A census that yields no layer writes nothing (noFront) and is retried no
    // sooner than a second later; a re-choice that finds no depth strictly
    // behind the rearmost filter writes nothing (noFront) until a later one does.
    DepthWrite OnUpdateDepth(int64_t frame, bool playerSelf, int64_t room)
    {
        ++counters_.depthCalls;
        if (!playerSelf) {
            ++counters_.depthOther;
            return DepthWrite{};
        }
        ++counters_.depthSeen;
        Refresh(frame);
        if (state_ != State::On) {
            ++counters_.depthIdle;
            return DepthWrite{};
        }
        if (room < 0) {
            ++counters_.noFront;
            return DepthWrite{};
        }
        const double now = Now(frame);
        if (visit_.have && visit_.room == room && LayerValid(visit_.layer)) {
            Rechoose(now);
            if (visit_.noChoice) {
                ++counters_.noFront;
                return DepthWrite{};
            }
        } else {
            if (haveCensus_ && !visit_.have && visit_.room == room && now - censusAt_ < kRechoiceMs) {
                ++counters_.noFront;
                return DepthWrite{};
            }
            Census(room, now);
            if (!visit_.have) {
                ++counters_.noFront;
                return DepthWrite{};
            }
        }
        ++counters_.depth;
        return DepthWrite{ true, visit_.layer };
    }

    // This room visit's flight layer, as the last census and re-choice left
    // it. Reading it runs nothing.
    const FlightLayerVisit& Visit() const { return visit_; }

    const FlightCounters& Stats() const { return counters_; }

    // Prints the last refresh; call RefreshForStat first.
    void FillStat(StatFields& s) const
    {
        s.install = std::string(InstallStateName());
        s.state = std::string(StateName(state_));
        s.equipped = equippedMask_;
        s.flight = counters_;
    }

private:
    struct Class { bool family = false; bool excluded = false; bool monster = false; };

    // The object's own index or one of its descendants. A failed ancestry
    // question is "no" for the family and "yes" for the exclusion: either way
    // the query keeps the game's answer.
    bool IsOrDescends(int object, int ancestor, bool onFailure) const
    {
        if (object == ancestor) return true;
        if (!isAncestor_) return onFailure;
        try { return isAncestor_(object, ancestor); } catch (...) { return onFailure; }
    }

    Class Classify(int object)
    {
        if (object < 0) return Class{};
        const auto it = familyTable_.find(object);
        if (it != familyTable_.end())
            return Class{ (it->second & 1) != 0, (it->second & 2) != 0, (it->second & 4) != 0 };
        Class c;
        c.family = IsOrDescends(object, family_, false);
        for (int e : excluded_) {
            if (IsOrDescends(object, e, true)) {
                c.excluded = true;
                break;
            }
        }
        // A failed ancestry question is "not a monster": the query keeps the
        // game's answer. An unresolved monster (-1) names nothing.
        c.monster = monster_ >= 0 && IsOrDescends(object, monster_, false);
        if (familyTable_.size() >= kFamilyTableMax) familyTable_.clear();
        familyTable_.emplace(object, static_cast<uint8_t>((c.family ? 1 : 0) | (c.excluded ? 2 : 0) | (c.monster ? 4 : 0)));
        return c;
    }

    // Every point inside [0, room_width) x [0, room_height). Written so that a
    // NaN fails it; an unreadable room fails it too.
    bool InsideRoom(const Query& q, int points) const
    {
        double w = 0.0, h = 0.0;
        if (!roomSize_) return false;
        bool ok = false;
        try { ok = roomSize_(w, h); } catch (...) { ok = false; }
        if (!ok) return false;
        const auto in = [w, h](double x, double y) { return x >= 0.0 && x < w && y >= 0.0 && y < h; };
        if (!in(q.x0, q.y0)) return false;
        if (points >= 2 && !in(q.x1, q.y1)) return false;
        return true;
    }

    bool ReadEquipped(unsigned& mask) const
    {
        if (!equipped_) return false;
        try { return equipped_(mask); } catch (...) { return false; }
    }

    // The frame's worn mask. Cache off: one read, as always. Cache on: the
    // remembered mask while it is valid and the last read is less than
    // kWornSafetyFrames behind (a frame counter that went backwards reads);
    // otherwise one read, a safety read when the cache was still valid.
    unsigned WornAt(int64_t frame)
    {
        if (wornCacheOn_ && wornValid_ && frame > wornReadFrame_ && frame - wornReadFrame_ < kWornSafetyFrames)
            return equippedMask_;
        unsigned mask = 0;
        const bool read = ReadEquipped(mask);
        mask = read ? (mask & kAllCharmBits) : 0u;
        if (!wornCacheOn_) return mask;
        ++wornCounters_.reads;
        if (wornValid_) {
            ++wornCounters_.safety;
            if (read && mask != equippedMask_) ++wornCounters_.stale;
        }
        wornValid_ = read;
        wornReadFrame_ = frame;
        return mask;
    }

    bool ReadOverlap(bool& inside) const
    {
        if (!overlap_) return false;
        try { return overlap_(inside); } catch (...) { return false; }
    }

    bool ReadLayerEnds(int64_t room, LayerEnd& first, LayerEnd& last) const
    {
        if (!layerEnds_) return false;
        try { return layerEnds_(room, first, last); } catch (...) { return false; }
    }

    // Each LayerOps call, with a missing callback or a throw read as a failure.
    template <class Fn, class... Args>
    static bool Call(const Fn& fn, Args&&... args)
    {
        if (!fn) return false;
        try { return static_cast<bool>(fn(std::forward<Args>(args)...)); } catch (...) { return false; }
    }

    double Now(int64_t frame) const
    {
        if (ops_.clock) {
            try {
                const double ms = ops_.clock();
                if (std::isfinite(ms)) return ms;
            } catch (...) {}
        }
        return static_cast<double>(frame) * (1000.0 / 60.0);
    }

    bool LayerValid(int64_t id) const { return Call(ops_.valid, id); }

    // Once per room visit: the game layers' front end, every layer's depth,
    // and, for the layers in front of that end, their name, visibility and
    // element count, plus layer_get_fx for a visible empty one. Remembers those
    // layers, chooses, and places the flight layer by name. Every entry listed
    // lands in exactly one of the visit's CensusTally outcomes.
    void Census(int64_t room, double now)
    {
        haveCensus_ = true;
        censusAt_ = now;
        visit_ = FlightLayerVisit{};
        visit_.room = room;
        LayerEnd first, last;
        visit_.gameFrontRead = ReadLayerEnds(room, first, last) && GameFrontDepth(first, last, visit_.gameFront);
        std::vector<LayerEnd> all;
        if (visit_.gameFrontRead && Call(ops_.all, all)) {
            visit_.listed = all.size();
            CensusTally& t = visit_.tally;
            for (const LayerEnd& e : all) {
                if (!std::isfinite(e.depth)) { ++t.noDepth; continue; }
                if (e.depth >= visit_.gameFront) { ++t.behind; continue; }
                RoomLayer l;
                if (!Call(ops_.detail, e.layer, l)) { ++t.noDetail; continue; }
                if (l.name == kFlightLayerName) { ++t.own; continue; }   // the flight layer itself never counts
                l.id = e.layer;
                l.depth = e.depth;
                l.effect = false;
                if (!l.visible) ++t.hidden;
                else if (l.elements != 0) ++t.holds;
                if (l.visible && l.elements == 0) {
                    // Each read in one of three counts, for the visit and the
                    // session alike; `other` and `failed` both leave the layer
                    // with no effect.
                    switch (ReadEffect(l)) {
                    case EffectRead::Failed: ++t.fxFailed; break;
                    case EffectRead::Struct: ++t.fxStruct; break;
                    case EffectRead::Other:  ++t.fxOther; break;
                    }
                }
                visit_.layers.push_back(std::move(l));
            }
            const FlightDepthChoice c = ChooseFlightDepth(visit_.gameFront, visit_.layers);
            int64_t id = -1;
            if (c.ok && Call(ops_.place, c.depth, id)) {
                visit_.have = true;
                visit_.layer = id;
                visit_.depth = c.depth;
                visit_.by = c.by;
            }
        }
        // The census's own time, on the clock alone (the frame stand-in does
        // not move within a call).
        if (ops_.clock) {
            const double end = Now(0);
            if (end > now) visit_.censusMs = end - now;
        }
        choseAt_ = now;
    }

    // One layer_get_fx read of `l`, marked read and counted in the session's
    // totals; `other` and `failed` both leave the layer with no effect.
    enum class EffectRead { Struct, Other, Failed };
    EffectRead ReadEffect(RoomLayer& l)
    {
        bool effect = false;
        const bool read = Call(ops_.effect, l.id, effect);
        l.effectRead = true;
        l.effect = read && effect;
        if (!read) { ++counters_.fxFailed; return EffectRead::Failed; }
        if (l.effect) { ++counters_.fxStruct; return EffectRead::Struct; }
        ++counters_.fxOther;
        return EffectRead::Other;
    }

    // At most once a second while on: the remembered layers' visibility and
    // elements again (a remembered filter that now holds one is a drawn layer),
    // the rule re-applied, and the layer moved when the depth moves. A layer
    // that is now visible and empty and whose effect was never read (it held
    // an element, or was hidden, at the census: an effect-carrying
    // Tree_Leaves that empties) has it read once here, so it counts as the
    // filter it now is ("Never past a filter"); an effect already read is not
    // read again. These reads count in the session's totals, not in the
    // census's tally. A failed recount keeps what the layer had.
    void Rechoose(double now)
    {
        if (now - choseAt_ < kRechoiceMs) return;
        choseAt_ = now;
        for (RoomLayer& l : visit_.layers) {
            bool visible = false;
            int64_t elements = 0;
            if (Call(ops_.count, l.id, visible, elements)) {
                l.visible = visible;
                l.elements = elements;
            }
            if (l.visible && l.elements == 0 && !l.effectRead) ReadEffect(l);
        }
        const FlightDepthChoice c = ChooseFlightDepth(visit_.gameFront, visit_.layers);
        // No depth strictly behind the filter: the layer stays where it was,
        // and nothing is written to it until a re-choice finds one again.
        visit_.noChoice = !c.ok;
        if (!c.ok) return;
        if (c.depth != visit_.depth) {
            if (!Call(ops_.move, visit_.layer, c.depth)) return;
            visit_.depth = c.depth;
            ++visit_.moves;
        }
        visit_.by = c.by;
    }

    IsAncestorFn isAncestor_;
    EquippedFn equipped_;
    OverlapFn overlap_;
    RoomSizeFn roomSize_;
    LayerEndsFn layerEnds_;
    int family_ = -1;
    int monster_ = -1;
    std::vector<int> excluded_;
    std::unordered_map<int, uint8_t> familyTable_;
    bool armed_ = false;
    bool installed_ = false;
    bool refused_ = false;
    bool lookFoundPlayer_ = false;   // NoteLook: the latest look resolved the local player
    bool lookFoundWorn_ = false;     // NoteLook: ... and found a flight charm worn
    bool haveFrame_ = false;
    int64_t frame_ = 0;
    bool havePlayerFrame_ = false;
    int64_t playerFrame_ = 0;
    unsigned equippedMask_ = 0;
    bool wornCacheOn_ = false;     // SetWornCache: the adapter's signal went in inline
    bool wornValid_ = false;       // a read since the last invalidation, and it succeeded
    int64_t wornReadFrame_ = 0;    // the frame of the cache's last read
    WornCacheCounters wornCounters_;
    State state_ = State::Off;
    LayerOps ops_;
    FlightLayerVisit visit_;
    bool haveCensus_ = false;   // a census has run (it may have yielded nothing)
    double censusAt_ = 0.0;     // when the last census ran, in clock ms
    double choseAt_ = 0.0;      // when the last census or re-choice chose
    FlightCounters counters_;
};

inline std::string RefusedLine(std::string_view why) { return "flightcharm: refused - " + std::string(why); }

// Flight's own trace line, which `flightcharm stat` prints after the stat line
// (whose fresh-session text stays as it is). `player` is the adapter's check
// of the player its hooks compare against: `match` when it is the local player
// resolved now, `differs` when it is another instance, `none` when either is
// missing. Read with the stat line, it says where a flight that answered
// nothing stopped, one cause per field (the sums are FlightCounters'):
//   - `calls=0`: no hooked call reached the core at all (the hooks);
//   - `not-player=` only: the calls came, but none from the player the hooks
//     compare against (the player check; see `player=`);
//   - `free=` only: the player's calls came, but none was blocked;
//   - of the blocked ones (`seen=`): off (`idle=`), the family or an
//     exclusion unresolved (`unconfigured=`), the object unreadable
//     (`unreadable=`), outside the family (`other=`), or kept for a wall, a
//     lock or the room's edge (the stat line's `excluded=`, `outside-room=`).
// For drawing in front, the same: no call (`depth-calls=0`), none from the
// player (`depth-other=` only), flight not on (`depth-idle=`), or no front
// layer could be chosen (`no-front=`), against the stat line's `depth=`.
// Last, the censuses' layer_get_fx reads over the session (`fx-struct=`,
// `fx-other=`, `fx-failed=`). The census reads the effect only of a visible,
// empty layer in front of the game layers, so `fx-struct=0` with `fx-other=0
// fx-failed=0` means no effect read was made, not that the read does not
// work: `flightprobe depth`'s `unread=` says why each layer went unread. Only
// `fx-other=` or `fx-failed=` rising with no `fx-struct=` says the read does
// not answer a struct on this runner, so "Never past a filter" did nothing.
// At the end, the monsters (the owner's scope, 2026-10-13): `monster=`, the
// monster-named place_meeting queries answered (a part of `answered=`), and
// `monster-kept=`, the monster-named blocked queries kept on another row and
// the InstancePlaceTallest monster answers kept (a part of `other=`). A walk
// still held at a monster while `monster-kept=` rises says another row holds it.
inline std::string TraceLine(std::string_view player, const FlightCounters& c)
{
    return "flightcharm trace: player=" + std::string(player)
        + " calls=" + std::to_string(c.calls) + " not-player=" + std::to_string(c.notPlayer)
        + " free=" + std::to_string(c.unblocked) + " seen=" + std::to_string(c.seen)
        + " idle=" + std::to_string(c.idle) + " unconfigured=" + std::to_string(c.unconfigured)
        + " unreadable=" + std::to_string(c.unreadable) + " other=" + std::to_string(c.other)
        + " depth-calls=" + std::to_string(c.depthCalls) + " depth-other=" + std::to_string(c.depthOther)
        + " depth-seen=" + std::to_string(c.depthSeen) + " depth-idle=" + std::to_string(c.depthIdle)
        + " no-front=" + std::to_string(c.noFront)
        + " fx-struct=" + std::to_string(c.fxStruct) + " fx-other=" + std::to_string(c.fxOther)
        + " fx-failed=" + std::to_string(c.fxFailed)
        + " monster=" + std::to_string(c.monster) + " monster-kept=" + std::to_string(c.monsterKept);
}

// ---- the size guard --------------------------------------------------------------
//
// A save keeps only the compact definition, so a reload rebuilds each charm
// and dressing writes its true footprint again. The game's grid load places
// each rebuilt item at its saved cell and ignores a refused placement (RDM
// § 23.6), so a charm whose footprint no longer fits would vanish, and a charm
// placed early could take a cell a vanilla item needs. The adapter hooks the
// load (ParseItemToGrid: Open before the original, Close after it, even when it
// throws) and the placement (InventoryGridAddItemPos: Defer first, and when it
// says so, return without calling the original). When a scope closes, every
// vanilla item of that grid is placed; each deferred charm then goes in at its
// full footprint, or, refused, at 1x1 at its own saved cell (the adapter
// writes 1x1 under "2" and "3", refreshes the item's hash, tags kGuardTag and
// places it again). Outside a scope nothing changes: a player's own drag onto
// a cell where the charm does not fit stays refused, as in the game.
//
// `Pending` is whatever the adapter needs to place the item again (its grid,
// item, cell and flags); the guard only keeps and hands it back. When it has
// `x` and `y` members, they are the saved cell (negative for unknown), and the
// flush keeps charms off each other's cells. Vanilla items always go in first;
// between two charms, the yield rule decides: a charm whose full footprint
// would cover another held-back charm's own cell goes straight to 1x1
// (`Yielded`), so every full placement leaves every other held charm's cell
// free and no save order of two charms loses one. The footprint is tested both
// ways round, w by h and h by w, because which argument the game reads as the
// column and which as the row is a static reading only (the comment on the
// adapter's FlightGuardPending); a charm may therefore load 1x1 for a session
// when only its transposed box touches another charm, a shrink and never a
// loss. When this charm's cell or another held charm's is unknown, it yields
// too; a charm alone in its load never yields. A charm with the same cell as
// another is not shrunk for it: both cannot fit.
//
// The counters say how far a load got, so a quiet line can be told apart from
// a guard that never saw anything: `opens` loads, `seen` placements inside one
// that reached the adapter, `charms` of those whose item it recognised.

enum class GuardOutcome : int { Full, Kept, Failed, Yielded };

struct GuardCounters {
    uint64_t opens = 0;      // grid loads (ParseItemToGrid scopes) opened
    uint64_t seen = 0;       // placements the adapter saw inside a load
    uint64_t charms = 0;     // of those, the ones whose item is a flight charm
    uint64_t deferred = 0;   // placements held back to the end of their load
    uint64_t full = 0;       // placed at the full footprint when the load ended
    uint64_t kept = 0;       // placed at 1x1: refused at full size, or yielded
    uint64_t yielded = 0;    // of kept, the ones that yielded to another held charm (CoversAnotherCell)
    uint64_t failed = 0;     // refused at 1x1 too
};

template <class Pending>
class SizeGuard {
public:
    // A grid load starts. Loads nest; each flushes only its own list.
    void Open()
    {
        scopes_.emplace_back();
        ++counters_.opens;
    }
    size_t Depth() const { return scopes_.size(); }

    // The adapter saw a placement; counted only inside a load and outside a
    // flush. `flightCharm` when it recognised the item as one.
    void Saw(bool flightCharm)
    {
        if (scopes_.empty() || flushing_) return;
        ++counters_.seen;
        if (flightCharm) ++counters_.charms;
    }

    // A placement inside a load. True when it was recorded instead of passed
    // on: a flight charm whose dressed footprint is larger than 1x1. Never
    // while a scope is being flushed (the flush's own placements go through).
    bool Defer(const Pending& p, bool flightCharm, int w, int h)
    {
        if (scopes_.empty() || flushing_ || !flightCharm) return false;
        if (w <= 1 && h <= 1) return false;
        Held held{ p, -1, -1, w, h };
        if constexpr (requires(const Pending& q) { q.x; q.y; }) {
            held.x = static_cast<long long>(p.x);
            held.y = static_cast<long long>(p.y);
        }
        scopes_.back().push_back(std::move(held));
        ++counters_.deferred;
        return true;
    }

    // The load ended. `placeFull(p)` places at the full footprint through the
    // original; `placeOne(p)` shrinks to 1x1 and places again; each answers
    // whether the item went in, and a throw counts as no. `report(p, outcome)`
    // hears each result (the adapter logs GuardLine for Kept and Failed).
    template <class PlaceFull, class PlaceOne, class Report>
    void Close(PlaceFull&& placeFull, PlaceOne&& placeOne, Report&& report)
    {
        if (scopes_.empty()) return;
        std::vector<Held> list = std::move(scopes_.back());
        scopes_.pop_back();
        const bool wasFlushing = flushing_;
        flushing_ = true;
        for (size_t i = 0; i < list.size(); ++i) {
            const Pending& p = list[i].p;
            const bool yields = CoversAnotherCell(list, i);
            if (!yields) {
                bool full = false;
                try { full = static_cast<bool>(placeFull(p)); } catch (...) { full = false; }
                if (full) {
                    ++counters_.full;
                    Tell(report, p, GuardOutcome::Full);
                    continue;
                }
            }
            bool one = false;
            try { one = static_cast<bool>(placeOne(p)); } catch (...) { one = false; }
            if (one) {
                ++counters_.kept;
                if (yields) ++counters_.yielded;
                Tell(report, p, yields ? GuardOutcome::Yielded : GuardOutcome::Kept);
            } else {
                ++counters_.failed;
                Tell(report, p, GuardOutcome::Failed);
            }
        }
        flushing_ = wasFlushing;
    }

    template <class PlaceFull, class PlaceOne>
    void Close(PlaceFull&& placeFull, PlaceOne&& placeOne)
    {
        Close(placeFull, placeOne, [](const Pending&, GuardOutcome) {});
    }

    const GuardCounters& Stats() const { return counters_; }

    void FillStat(StatFields& s) const
    {
        s.guardKept = counters_.kept;
        s.guardFailed = counters_.failed;
    }

private:
    struct Held {
        Pending p;
        long long x = -1;   // the saved cell; negative when unknown
        long long y = -1;
        int w = 1;          // the dressed footprint
        int h = 1;
    };

    // Should list[i] go straight to 1x1? Yes when its full footprint, read
    // either way round (w by h, or h by w, from its own cell), would cover the
    // saved cell of another charm held back in the same load, and yes when
    // its own cell or another held charm's is unknown, since the guard cannot
    // tell. A charm alone in its load never yields, and nor does a charm for
    // another on the very same cell: both cannot fit.
    static bool CoversAnotherCell(const std::vector<Held>& list, size_t i)
    {
        const Held& a = list[i];
        for (size_t j = 0; j < list.size(); ++j) {
            if (j == i) continue;
            const Held& b = list[j];
            if (a.x < 0 || a.y < 0 || b.x < 0 || b.y < 0) return true;
            if (b.x == a.x && b.y == a.y) continue;
            const auto covers = [&a, &b](int w, int h) {
                return b.x >= a.x && b.x < a.x + w && b.y >= a.y && b.y < a.y + h;
            };
            if (covers(a.w, a.h) || covers(a.h, a.w)) return true;
        }
        return false;
    }

    template <class Report>
    static void Tell(Report& report, const Pending& p, GuardOutcome o)
    {
        try { report(p, o); } catch (...) {}
    }

    std::vector<std::vector<Held>> scopes_;
    bool flushing_ = false;
    GuardCounters counters_;
};

// The one line the adapter logs for a charm the guard shrank or could not
// place; empty for one placed at full size.
inline std::string GuardLine(std::string_view name, long long x, long long y, GuardOutcome o)
{
    const std::string cell = std::to_string(x) + "," + std::to_string(y);
    if (o == GuardOutcome::Kept)
        return "flightcharm: guard kept " + std::string(name) + " at 1x1 in cell " + cell + " (its full size did not fit)";
    if (o == GuardOutcome::Yielded)
        return "flightcharm: guard kept " + std::string(name) + " at 1x1 in cell " + cell
            + " (its full size would cover another flight charm's cell)";
    if (o == GuardOutcome::Failed)
        return "flightcharm: guard failed " + std::string(name) + " in cell " + cell + " (no room even at 1x1)";
    return std::string();
}

// The guard's own line, which `flightcharm stat` prints after the stat line.
// `route` is the adapter's install result: `on`, `table-only`, `refused` or
// `not-installed`. Read top to bottom, the counters say where a quiet guard
// stopped: no load reached it (`opens=0`), no placement inside one did
// (`seen=0`), no item was recognised (`charms=0`), or every charm fitted.
inline std::string GuardStatLine(std::string_view route, const GuardCounters& c)
{
    return "flightcharm guard: route=" + std::string(route) + " opens=" + std::to_string(c.opens)
        + " seen=" + std::to_string(c.seen) + " charms=" + std::to_string(c.charms)
        + " deferred=" + std::to_string(c.deferred) + " full=" + std::to_string(c.full)
        + " kept=" + std::to_string(c.kept) + " yielded=" + std::to_string(c.yielded)
        + " failed=" + std::to_string(c.failed);
}

// ---- drops: the stand-ins and the coin --------------------------------------------
//
// A flight charm drops in place of a stand-in: an entry of the game's own
// unique lists, [type, sub, b], pushed for the length of one roll. A hit on a
// stand-in is ours with probability c / (n + c), where n is the stand-in's own
// count in the sub-list and c the copies pushed, so the game's own drop of
// that charm keeps its rate. Gabriel Wings and Witch's Broomstick use the
// Angelic roll's lootListUnique[5] and a fixed stand-in of the same shape and
// rarity. Pocket Elf uses the ordinary unique roll, which draws once from one
// of lootListUnique[0..4] and cannot say which (RDM § 23.8); its coin is exact
// only when the stand-in occurs equally often in all five, so its stand-in is
// the first candidate, in table order, with the same count in each of them
// that also passes the roll's own filter.

struct StandIn {
    std::string_view key;    // the ` standin-elf=` word
    std::string_view name;
    int              t;
    int              sub;
    int              b;
};

inline constexpr int kUniqueSubLists = 5;    // lootListUnique[0..4], the ordinary unique roll
inline constexpr int kAngelicSubList = 5;    // lootListUnique[5], the Angelic roll
inline constexpr int kUniqueRarityLimit = 7; // the ordinary roll keeps rarities below this

inline constexpr int kElfCandidateCount = 5;
inline constexpr StandIn kElfCandidates[kElfCandidateCount] = {
    { "aimbot",        "Aimbot.exe",             10, 0, 76 },
    { "witches-claw",  "Witches Claw",           10, 0, 41 },
    { "map-journal",   "Traveler's Map Journal", 10, 0, 24 },
    { "crows-feather", "Crow's Feather",         10, 0, 68 },
    { "steve",         "Hello its me, Steve!",   10, 0, 12 },
};
inline constexpr StandIn kWingsStandIn = { "water-melon", "Water Melon", 10, 0, 47 };
inline constexpr StandIn kBroomStandIn = { "soul-collector", "Soul Collector", 10, 0, 91 };

// The fixed stand-in; nullptr for Pocket Elf, whose stand-in is chosen.
inline const StandIn* StandInFor(Charm c)
{
    if (c == Charm::Wings) return &kWingsStandIn;
    if (c == Charm::Broom) return &kBroomStandIn;
    return nullptr;
}

inline constexpr std::string_view kElfNoStandIn = "elf: no stand-in with an even count across lootListUnique[0..4]";

// One entry of a unique sub-list, read as numbers; the adapter skips entries
// that are not three-element arrays.
struct UniqueEntry {
    double t;
    double sub;
    double b;
};

inline int CountIn(const std::vector<UniqueEntry>& list, const StandIn& s)
{
    int n = 0;
    for (const UniqueEntry& e : list)
        if (e.t == s.t && e.sub == s.sub && e.b == s.b) ++n;
    return n;
}

// The ordinary unique roll's own filter on what GetUniqueRepoStruct answers.
inline bool PassesUniqueFilter(int rarity, bool hidden) { return rarity < kUniqueRarityLimit && !hidden; }

using ListCounts = std::array<int, kUniqueSubLists>;

struct CandidateCounts {
    ListCounts counts{};     // the candidate's count in each of lootListUnique[0..4]
    bool       passes = false;
};

inline bool EvenCount(const ListCounts& c)
{
    for (int n : c)
        if (n < 0 || n != c[0]) return false;
    return true;
}

// The index of Pocket Elf's stand-in in kElfCandidates, or -1: refused with
// kElfNoStandIn, the gate stays off. Asked when the lists resolve and again
// whenever a sub-list's length changes.
inline int PickElfStandIn(const std::array<CandidateCounts, kElfCandidateCount>& c)
{
    for (int i = 0; i < kElfCandidateCount; ++i)
        if (c[i].passes && EvenCount(c[i].counts)) return i;
    return -1;
}

// ` standin-elf=`: none before the lists resolve, the chosen key, or the
// refusal with every candidate's counts.
inline std::string ElfStandInText(bool resolved, int picked, const std::array<CandidateCounts, kElfCandidateCount>& c)
{
    if (!resolved) return "none";
    if (picked >= 0 && picked < kElfCandidateCount) return std::string(kElfCandidates[picked].key);
    std::string s = "refused(";
    for (int i = 0; i < kElfCandidateCount; ++i) {
        if (i) s += ",";
        s += std::string(kElfCandidates[i].key) + ":";
        for (int k = 0; k < kUniqueSubLists; ++k) {
            if (k) s += "/";
            s += std::to_string(c[i].counts[k]);
        }
        if (!c[i].passes) s += "-filtered";
    }
    return s + ")";
}

// The share of a stand-in hit that becomes the charm: c / (n + c).
inline double CoinShare(long long n, long long c)
{
    if (n < 0 || c <= 0) return 0.0;
    return static_cast<double>(c) / static_cast<double>(n + c);
}

// Whether a hit is ours, for u drawn uniformly from [0, 1).
inline bool CoinOurs(long long n, long long c, double u)
{
    if (!(u >= 0.0 && u < 1.0)) return false;
    return u < CoinShare(n, c);
}

// ---- drops: Pocket Elf's kill route on DropItem's own draw (#166) -----------------
//
// A kill does not reach the ordinary unique roll above (DropUniqueItems): its
// unique comes from DropItem's own draw, which reads lootListUnique[k] for the
// k LoadSatanicDropTier returned in that call (Live 1, measured), and keeps an
// entry whose definition reads rarity 6 with fields 40 and 41 clear (static
// reading). So for the length of one DropItem call the kill route pushes
// `copies` of one stand-in onto each of [0..5], and a typed hit on it is Pocket
// Elf's with probability c / (n_k + c): n_k is the stand-in's own count on list
// k, k the tier recorded in this call. With no tier recorded the hit is refused.
//
// The stand-in is a class-10 entry the draw keeps as read, at SS (tier 5): the
// rarity and tier of Aimbot.exe, which Pocket Elf's drop is to match, with no
// fallback below SS (the owner, 2026-10-21). A class 10 with no SS candidate
// has no stand-in, and Pocket Elf does not drop from the draw. Among the SS
// candidates the smallest largest count over the six lists wins (0 everywhere
// first: then every hit on it is Pocket Elf's), then the lowest (sub, b). The
// coin uses the recorded k, so no equal-count rule is needed, and it stays
// exact even if the draw weighs entries: the copies and the natural ones are
// the same triple. The DropUniqueItems route above keeps its own stand-in.
//
// The adapter owns the hooks (DropManager's one DropItem detour, raredrop's
// one LoadSatanicDropTier detour), the list reads and the push; ElfDraw
// decides and counts.

inline constexpr int kDrawListCount = 6;    // lootListUnique[0..5], what the draw can read
inline constexpr int kDrawRarity = 6;       // the Satanic rarity the draw keeps (static reading)
inline constexpr int kDrawSsTier = 5;       // the definition's SS tier
inline constexpr int kElfDrawType = 10;     // Pocket Elf's class
inline constexpr int kElfDrawTier = kDrawSsTier;   // Aimbot.exe's tier; no fallback below it
inline constexpr int kDrawMaxCopies = 400;  // the research lever's ceiling

using DrawCounts = std::array<int, kDrawListCount>;
using DrawLists = std::array<std::vector<UniqueEntry>, kDrawListCount>;

struct DrawTriple {
    int t = 0;
    int sub = 0;
    int b = 0;
};

// One triple's definition as the draw reads it: rarity, tier, and fields 40
// and 41. The adapter reads it by name, outside a roll; false when unreadable.
struct DrawVerdict {
    int  rarity = 0;
    int  tier = -1;
    bool f40 = false;
    bool f41 = false;
};
using DrawVerdictFn = std::function<bool(const DrawTriple& key, DrawVerdict& out)>;

// Whether the draw keeps an entry with this definition as read.
inline bool DrawKeeps(const DrawVerdict& v) { return v.rarity == kDrawRarity && !v.f40 && !v.f41; }

struct DrawStandIn {
    bool       resolved = false;
    DrawTriple triple;
    int        rarity = 0;
    int        tier = -1;
    DrawCounts counts{};   // its own count on each of [0..5]
};

struct ElfDrawResolution {
    DrawStandIn standIn;
    int         candidates = 0;   // distinct class-10 triples the draw keeps, any tier
    int         ss = 0;           // of those, tier 5
    int         unread = 0;       // distinct class-10 triples whose definition could not be read
};

// Pocket Elf's draw stand-in from the six lists as read.
inline ElfDrawResolution ResolveElfDrawStandIn(const DrawLists& lists, const DrawVerdictFn& verdict)
{
    ElfDrawResolution r;
    std::vector<std::pair<DrawTriple, DrawCounts>> seen;
    for (int k = 0; k < kDrawListCount; ++k) {
        for (const UniqueEntry& e : lists[static_cast<size_t>(k)]) {
            long long t = 0, sub = 0, b = 0;
            if (!AsWhole(e.t, t) || !AsWhole(e.sub, sub) || !AsWhole(e.b, b) || t != kElfDrawType) continue;
            if (sub < INT32_MIN || sub > INT32_MAX || b < INT32_MIN || b > INT32_MAX) continue;
            const DrawTriple key{ kElfDrawType, static_cast<int>(sub), static_cast<int>(b) };
            size_t i = 0;
            while (i < seen.size() && !(seen[i].first.sub == key.sub && seen[i].first.b == key.b)) ++i;
            if (i == seen.size()) seen.push_back({ key, DrawCounts{} });
            ++seen[i].second[static_cast<size_t>(k)];
        }
    }
    const auto largest = [](const DrawCounts& c) {
        int m = 0;
        for (int n : c) m = n > m ? n : m;
        return m;
    };
    for (const auto& [key, counts] : seen) {
        DrawVerdict v;
        if (!verdict || !verdict(key, v)) {
            ++r.unread;
            continue;
        }
        if (!DrawKeeps(v)) continue;
        ++r.candidates;
        if (v.tier == kDrawSsTier) ++r.ss;
        if (v.tier != kElfDrawTier) continue;
        DrawStandIn& best = r.standIn;
        bool better = !best.resolved;
        if (!better) {
            const int mc = largest(counts), mb = largest(best.counts);
            better = mc != mb ? mc < mb
                : key.sub != best.triple.sub ? key.sub < best.triple.sub
                : key.b < best.triple.b;
        }
        if (!better) continue;
        best.resolved = true;
        best.triple = key;
        best.rarity = v.rarity;
        best.tier = v.tier;
        best.counts = counts;
    }
    return r;
}

// ` elf-draw-standin=`: `<sub>:<b>@<rarity>/<tier>x<n0>,...,<n5>`, or none.
inline std::string ElfDrawStandInText(const DrawStandIn& s)
{
    if (!s.resolved) return "none";
    std::string counts;
    for (int k = 0; k < kDrawListCount; ++k) counts += (k ? "," : "") + std::to_string(s.counts[static_cast<size_t>(k)]);
    return std::to_string(s.triple.sub) + ":" + std::to_string(s.triple.b) + "@" + std::to_string(s.rarity) + "/"
        + std::to_string(s.tier) + "x" + counts;
}

// The copies one call pushes onto each list: none without a stand-in or below
// one copy, at most kDrawMaxCopies.
inline int ElfDrawCopies(const DrawStandIn& s, int copies)
{
    if (!s.resolved || copies < 1) return 0;
    return copies > kDrawMaxCopies ? kDrawMaxCopies : copies;
}

// A stand-in hit's share at tier k: c / (n_k + c); 0 for a k outside [0..5].
inline double ElfDrawShare(const DrawStandIn& s, int k, int copies)
{
    if (!s.resolved || k < 0 || k >= kDrawListCount) return 0.0;
    return CoinShare(s.counts[static_cast<size_t>(k)], copies);
}

// One list's identity as the adapter read it: its handle and its length.
struct DrawListKey {
    double id = -1.0;
    int    length = -1;
};
using DrawListKeys = std::array<DrawListKey, kDrawListCount>;

// The stand-in, resolved again whenever any of [0..5] changed its array or
// its length.
class ElfDrawCache {
public:
    bool Stale(const DrawListKeys& keys) const
    {
        if (!have_) return true;
        for (size_t k = 0; k < keys.size(); ++k)
            if (keys[k].id != keys_[k].id || keys[k].length != keys_[k].length) return true;
        return false;
    }
    void Store(const DrawListKeys& keys, const ElfDrawResolution& r)
    {
        have_ = true;
        keys_ = keys;
        resolution_ = r;
    }
    const ElfDrawResolution* Find() const { return have_ ? &resolution_ : nullptr; }

private:
    bool              have_ = false;
    DrawListKeys      keys_{};
    ElfDrawResolution resolution_;
};

// What a DropItem call does for Pocket Elf: nothing (switch, refusal or gate
// off), nothing because a roll or another DropItem call is already in
// progress, or resolve the stand-in and push it.
enum class DrawAction : int { PassOff, PassNested, Push };

inline std::string DrawActionName(DrawAction a)
{
    switch (a) {
    case DrawAction::PassOff:    return "pass-off";
    case DrawAction::PassNested: return "pass-nested";
    case DrawAction::Push:       return "push";
    }
    return "?";
}

// A typed hit, as ElfDraw::Hit decides it. Ours goes to the rewrite point;
// every other answer leaves the hit the game's own.
enum class DrawHit : int {
    NotPushed,    // no DropItem call carries Pocket Elf's entries
    SetAside,     // a roll nested in the call runs: its own machinery decides
    NotStandIn,   // another triple
    TierUnread,   // no tier recorded in this call: refused
    Game,         // the stand-in, and the coin gave it to the game
    Ours,
};

inline std::string DrawHitName(DrawHit h)
{
    switch (h) {
    case DrawHit::NotPushed:  return "not-pushed";
    case DrawHit::SetAside:   return "set-aside";
    case DrawHit::NotStandIn: return "not-stand-in";
    case DrawHit::TierUnread: return "tier-unread";
    case DrawHit::Game:       return "game";
    case DrawHit::Ours:       return "ours";
    }
    return "?";
}

// One session's kill draw for Pocket Elf. The adapter calls, per DropItem
// call: BeginCall, then on Push the resolution and NotePushed with what went
// on; NoteTier from LoadSatanicDropTier's detour (the innermost call in
// progress); EnterRoll / LeaveRoll around a nested roll (#74's Angelic roll,
// the shared DropUniqueItems detour, Mark 1's Heroic hook), taking the entries
// off when EnterRoll answers true and putting them back when LeaveRoll does;
// Hit for each typed hit, Settle once its rewrite ran or was refused, and
// EndCall, which answers whether the entries come off. NoteBuilt counts one at
// the forge's final pass.
class ElfDraw {
public:
    DrawAction BeginCall(bool on, int rollDepth)
    {
        ++calls_;
        frames_.push_back(Frame{});
        if (!on) return DrawAction::PassOff;
        if (rollDepth > 0 || rollDepth_ > 0 || frames_.size() > 1) {
            ++nested_;
            return DrawAction::PassNested;
        }
        return DrawAction::Push;
    }

    void NoteResolution(const ElfDrawResolution& r)
    {
        state_ = ListState::Resolved;
        last_ = r.standIn;
    }
    void NoteListsUnreadable()
    {
        state_ = ListState::Unreadable;
        last_ = DrawStandIn{};
    }

    // `copies` of `s` went onto each of [0..5] and were held read-back.
    void NotePushed(const DrawStandIn& s, int copies)
    {
        if (frames_.empty() || pushFrame_ >= 0 || !s.resolved || copies < 1) return;
        frames_.back().pushed = true;
        pushFrame_ = static_cast<int>(frames_.size()) - 1;
        plan_ = s;
        copies_ = copies;
        ++pushedCalls_;
        injected_ += static_cast<uint64_t>(copies) * kDrawListCount;
    }

    // The tier LoadSatanicDropTier returned, for the innermost call in
    // progress; one outside [0..5] is no tier.
    void NoteTier(int k)
    {
        if (frames_.empty()) return;
        frames_.back().tier = k >= 0 && k < kDrawListCount ? k : -1;
    }

    // A nested roll begins: true when the call's entries must come off now.
    // The open hit waits until the roll returns.
    bool EnterRoll()
    {
        ++rollDepth_;
        if (rollDepth_ != 1 || pushFrame_ < 0) return false;
        asideHit_ = hit_;
        hit_ = HitState{};
        return true;
    }
    // A nested roll returned: true when the entries must go back now.
    bool LeaveRoll()
    {
        if (rollDepth_ <= 0) return false;
        --rollDepth_;
        if (rollDepth_ != 0 || pushFrame_ < 0) return false;
        hit_ = asideHit_;
        asideHit_ = HitState{};
        return true;
    }

    // A typed hit on (t, sub, b), for u drawn uniformly from [0, 1). An open
    // hit is closed first: each hit is settled apart.
    DrawHit Hit(double t, double sub, double b, double u)
    {
        if (frames_.empty() || pushFrame_ < 0) return DrawHit::NotPushed;
        if (rollDepth_ > 0) return DrawHit::SetAside;
        hit_ = HitState{};
        if (t != plan_.triple.t || sub != plan_.triple.sub || b != plan_.triple.b) return DrawHit::NotStandIn;
        const int k = frames_.back().tier;
        if (k < 0) {
            ++tierUnread_;
            return DrawHit::TierUnread;
        }
        if (!CoinOurs(plan_.counts[static_cast<size_t>(k)], copies_, u)) return DrawHit::Game;
        hit_.ours = true;
        ++ours_;
        return DrawHit::Ours;
    }
    // Whether an Ours hit is waiting for its rewrite.
    bool HitPending() const { return hit_.ours; }
    // The open hit's rewrite ran (`rewritten`) or was refused.
    void Settle(bool rewritten)
    {
        if (hit_.ours && rewritten) ++pendingBuilt_;
        hit_ = HitState{};
    }
    // The forge's final pass counted a rewritten Pocket Elf built.
    void NoteBuilt()
    {
        if (pendingBuilt_ == 0) return;
        --pendingBuilt_;
        ++built_;
    }

    // The call returned: true when its entries must come off now (it pushed
    // them, and no nested roll has them off already).
    bool EndCall()
    {
        if (frames_.empty()) return false;
        const bool pushed = frames_.back().pushed;
        frames_.pop_back();
        if (!pushed) return false;
        pushFrame_ = -1;
        hit_ = HitState{};
        asideHit_ = HitState{};
        return rollDepth_ == 0;
    }

    // ` elf-draw-standin=`: pending while the switch is on and nothing was
    // resolved yet, else the last resolution's stand-in, or none.
    std::string StandInField(bool on) const
    {
        if (state_ == ListState::Unknown) return on ? "pending" : "none";
        return ElfDrawStandInText(last_);
    }

    uint64_t Calls() const { return calls_; }
    uint64_t PushedCalls() const { return pushedCalls_; }
    uint64_t Nested() const { return nested_; }
    uint64_t Injected() const { return injected_; }
    uint64_t TierUnread() const { return tierUnread_; }
    uint64_t Ours() const { return ours_; }
    uint64_t Built() const { return built_; }
    // The tier NoteTier recorded for the innermost call in progress (the one
    // Hit reads), -1 for none: the k on Pocket Elf's hit line.
    int Tier() const { return frames_.empty() ? -1 : frames_.back().tier; }

    // Every `elf-draw-` field but the hook's route, which the adapter sets.
    void FillStat(StatFields& s, bool on) const
    {
        s.elfDrawCalls = calls_;
        s.elfDrawPushed = pushedCalls_;
        s.elfDraw = DropTally{ ours_, built_ };
        s.elfDrawStandIn = StandInField(on);
    }

private:
    enum class ListState : int { Unknown, Resolved, Unreadable };
    struct Frame {
        int  tier = -1;
        bool pushed = false;
    };
    struct HitState {
        bool ours = false;
    };

    std::vector<Frame> frames_;
    int                pushFrame_ = -1;
    int                rollDepth_ = 0;
    DrawStandIn        plan_;
    int                copies_ = 0;
    HitState           hit_, asideHit_;
    ListState          state_ = ListState::Unknown;
    DrawStandIn        last_;
    uint64_t           calls_ = 0;
    uint64_t           pushedCalls_ = 0;
    uint64_t           nested_ = 0;
    uint64_t           injected_ = 0;
    uint64_t           tierUnread_ = 0;
    uint64_t           ours_ = 0;
    uint64_t           built_ = 0;
    uint64_t           pendingBuilt_ = 0;
};

// ---- the `flightcharm` command ------------------------------------------------------

inline constexpr std::string_view kUsage = "usage: flightcharm elf|wings|broom 1|0 | flightcharm stat";

enum class CommandForm : int { Stat, Switch };

struct Command {
    CommandForm form = CommandForm::Stat;
    Charm       charm = Charm::Elf;
    bool        on = false;
};

// The words after `flightcharm`. Bare or `stat` prints the stat line;
// `<key> 1|0` sets that charm's drop switch.
inline bool ParseCommand(const std::vector<std::string>& args, Command& out, std::string& why)
{
    why.clear();
    if (args.empty() || (args.size() == 1 && args[0] == "stat")) {
        out = Command{};
        return true;
    }
    Charm c = Charm::Elf;
    if (args.size() == 2 && CharmFromKey(args[0], c) && (args[1] == "1" || args[1] == "0")) {
        out.form = CommandForm::Switch;
        out.charm = c;
        out.on = args[1] == "1";
        return true;
    }
    why = "flightcharm: " + std::string(kUsage);
    return false;
}

} // namespace ForgePact::FlightCharms
