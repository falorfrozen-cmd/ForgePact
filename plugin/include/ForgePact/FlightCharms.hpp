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
// through scenery (the Collision_Parent_obj family) but not through walls,
// gates, locks, monsters or the room's edge, and is drawn in front of scenery.
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
//   - the front-layer choice made after the game's UpdateDepth.
//   - the size guard, which defers a dressed charm to the end of its grid's
//     load so that it never takes a cell a vanilla item needs.
//   - Pocket Elf's stand-in rule, and the coin every flight drop is decided by.
//   - the `flightcharm` command's forms and its `stat` line.
//
// It is game-independent by contract - numbers and strings, never an instance
// or an RValue - so tests/flight_charms_harness.cpp compiles it whole. The
// adapter in ModuleMain.cpp resolves every object, script and builtin by name
// and supplies each game read through the callbacks below, as JumpScenery.hpp's
// does. The research is docs/flight-charms-research.md; the game facts are in
// the hub's docs/RUNTIME_DATA_MODELS.md (§ 19.6-19.8, § 22).
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
// still stood inside the family at the last read, so flight keeps answering
// until a read comes back free and the player can walk out.
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

struct FlightCounters {
    uint64_t answered = 0;      // queries answered "nothing there"
    uint64_t excluded = 0;      // kept because a wall or a lock blocked, or was named
    uint64_t outsideRoom = 0;   // kept because a point was outside the room (or the room unreadable)
    uint64_t depth = 0;         // front-layer writes
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

// One end of global.gameLayer[room]: the layer id and its layer_get_depth.
struct LayerEnd {
    int64_t layer = -1;
    double  depth = 0.0;
};

struct DepthWrite {
    bool    write = false;
    int64_t layer = -1;
};

// GameMaker draws a lower layer depth on top, so the front end is the one with
// the lower depth. Equal or non-finite depths name no front: nothing is
// written. Which end is in front was not settled by reading (RDM § 19.7), so
// it is decided here, per room, and measured live (`flight-depth`).
inline bool ChooseFrontLayer(const LayerEnd& first, const LayerEnd& last, int64_t& layer)
{
    if (!std::isfinite(first.depth) || !std::isfinite(last.depth)) return false;
    if (first.depth < last.depth) {
        layer = first.layer;
        return true;
    }
    if (last.depth < first.depth) {
        layer = last.layer;
        return true;
    }
    return false;
}

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
// this order; anything added goes after them.
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
    // Collision_Parent_obj, with the player as self; false when unreadable
    // (counts as free, so flight goes off: the game's own answers again).
    using OverlapFn = std::function<bool(bool& overlapping)>;
    // room_width / room_height; false when they cannot be read.
    using RoomSizeFn = std::function<bool(double& width, double& height)>;
    // global.gameLayer[room]'s first and last entries with their depths;
    // false when they cannot be read.
    using LayerEndsFn = std::function<bool(int64_t room, LayerEnd& first, LayerEnd& last)>;

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

    void SetEquipped(EquippedFn fn) { equipped_ = std::move(fn); }
    void SetOverlap(OverlapFn fn) { overlap_ = std::move(fn); }
    void SetRoomSize(RoomSizeFn fn) { roomSize_ = std::move(fn); }
    void SetLayerEnds(LayerEndsFn fn) { layerEnds_ = std::move(fn); haveRoom_ = false; }

    bool Configured() const
    {
        if (family_ < 0) return false;
        for (int e : excluded_) if (e < 0) return false;
        return true;
    }

    // ---- the install: armed by a charm, hooked once a character exists ----
    // Dressing arms flight the first time it recognises a flight charm; a
    // hook is never installed from inside CreateItemNew. While armed, the
    // adapter's frame tick looks for the local player on the frames
    // LooksForPlayer picks (HhResolveLocalPlayer, which accepts the runner's
    // VALUE_REF) and installs the builtins, InstancePlaceTallest and
    // UpdateDepth once. A refusal (a table-only hook, a research hook holding
    // one of them) is final for the session; the adapter prints RefusedLine
    // once. Nothing runs per frame before arming.
    static constexpr unsigned long long kInstallPollFrames = 60;
    static constexpr bool LooksForPlayer(unsigned long long armedFrames)
    {
        return armedFrames % kInstallPollFrames == 0;
    }
    void Arm() { armed_ = true; }
    bool Armed() const { return armed_; }
    bool ShouldInstall(bool setupDone, bool playerResolved) const
    {
        return armed_ && !installed_ && !refused_ && setupDone && playerResolved;
    }
    void NoteInstall(bool ok)
    {
        if (ok) installed_ = true;
        else refused_ = true;
    }
    bool Installed() const { return installed_; }
    // ` install=`: installed, refused, waiting-for-character while armed,
    // not-armed otherwise.
    std::string_view InstallStateName() const
    {
        if (installed_) return "installed";
        if (refused_) return "refused";
        return armed_ ? "waiting-for-character" : "not-armed";
    }

    // ---- the state, refreshed at the point of use ----------------------------
    // At most once a frame: the first player-self call of a frame reads the
    // worn charms, and, only when none is worn after flight was on or
    // residual, whether the player still overlaps the family. `flightcharm
    // stat` refreshes too (RefreshForStat), so what it prints never depends on
    // a hooked call having run since the last equip.
    void Refresh(int64_t frame)
    {
        if (haveFrame_ && frame == frame_) return;
        haveFrame_ = true;
        frame_ = frame;
        unsigned mask = 0;
        if (!ReadEquipped(mask)) mask = 0;
        mask &= kAllCharmBits;
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

    State CurrentState() const { return state_; }
    unsigned EquippedMask() const { return equippedMask_; }

    // One hooked call, after its original ran. `excludedBlocks()` re-runs the
    // same original, same arguments and self, against Wall_Parent_obj and
    // then Lock_obj, and says whether either is there. It is asked only for a
    // blocked, family-named query of the local player while flight is on or
    // residual, with every point inside the room, and never for
    // InstancePlaceTallest. Any other self returns at once, reading nothing.
    template <class ExcludedBlocksFn>
    Answer OnQuery(const Query& q, ExcludedBlocksFn&& excludedBlocks)
    {
        const int i = static_cast<int>(q.row);
        if (!q.playerSelf || i < 0 || i >= kHookedCount) return Answer::Real;
        Refresh(q.frame);
        if (state_ == State::Off || !q.reallyBlocked || !Configured()) return Answer::Real;
        const Class c = Classify(q.object);
        if (!c.family) return Answer::Real;
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

    // After the game's UpdateDepth ran: whether to set the player's `layer`,
    // and to which layer. Only for the local player, only while flight is on
    // (not residual); the room's two layer ends are read once per room.
    DepthWrite OnUpdateDepth(int64_t frame, bool playerSelf, int64_t room)
    {
        if (!playerSelf) return DepthWrite{};
        Refresh(frame);
        if (state_ != State::On) return DepthWrite{};
        if (!haveRoom_ || room != room_) {
            haveRoom_ = true;
            room_ = room;
            LayerEnd first, last;
            roomFront_ = ReadLayerEnds(room, first, last) && ChooseFrontLayer(first, last, roomLayer_);
        }
        if (!roomFront_) return DepthWrite{};
        ++counters_.depth;
        return DepthWrite{ true, roomLayer_ };
    }

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
    struct Class { bool family = false; bool excluded = false; };

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
        if (it != familyTable_.end()) return Class{ (it->second & 1) != 0, (it->second & 2) != 0 };
        Class c;
        c.family = IsOrDescends(object, family_, false);
        for (int e : excluded_) {
            if (IsOrDescends(object, e, true)) {
                c.excluded = true;
                break;
            }
        }
        if (familyTable_.size() >= kFamilyTableMax) familyTable_.clear();
        familyTable_.emplace(object, static_cast<uint8_t>((c.family ? 1 : 0) | (c.excluded ? 2 : 0)));
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

    IsAncestorFn isAncestor_;
    EquippedFn equipped_;
    OverlapFn overlap_;
    RoomSizeFn roomSize_;
    LayerEndsFn layerEnds_;
    int family_ = -1;
    std::vector<int> excluded_;
    std::unordered_map<int, uint8_t> familyTable_;
    bool armed_ = false;
    bool installed_ = false;
    bool refused_ = false;
    bool haveFrame_ = false;
    int64_t frame_ = 0;
    unsigned equippedMask_ = 0;
    State state_ = State::Off;
    bool haveRoom_ = false;
    int64_t room_ = 0;
    bool roomFront_ = false;
    int64_t roomLayer_ = -1;
    FlightCounters counters_;
};

inline std::string RefusedLine(std::string_view why) { return "flightcharm: refused - " + std::string(why); }

// ---- the size guard --------------------------------------------------------------
//
// A save keeps only the compact definition, so a reload rebuilds each charm
// and dressing writes its true footprint again. The game's grid load places
// each rebuilt item at its saved cell and ignores a refused placement (RDM
// § 22.6), so a charm whose footprint no longer fits would vanish, and a charm
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
// item, cell and flags); the guard only keeps and hands it back.

enum class GuardOutcome : int { Full, Kept, Failed };

struct GuardCounters {
    uint64_t deferred = 0;   // placements held back to the end of their load
    uint64_t full = 0;       // placed at the full footprint when the load ended
    uint64_t kept = 0;       // refused at full size, placed at 1x1
    uint64_t failed = 0;     // refused at 1x1 too
};

template <class Pending>
class SizeGuard {
public:
    // A grid load starts. Loads nest; each flushes only its own list.
    void Open() { scopes_.emplace_back(); }
    size_t Depth() const { return scopes_.size(); }

    // A placement inside a load. True when it was recorded instead of passed
    // on: a flight charm whose dressed footprint is larger than 1x1. Never
    // while a scope is being flushed (the flush's own placements go through).
    bool Defer(const Pending& p, bool flightCharm, int w, int h)
    {
        if (scopes_.empty() || flushing_ || !flightCharm) return false;
        if (w <= 1 && h <= 1) return false;
        scopes_.back().push_back(p);
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
        std::vector<Pending> list = std::move(scopes_.back());
        scopes_.pop_back();
        const bool wasFlushing = flushing_;
        flushing_ = true;
        for (const Pending& p : list) {
            bool full = false;
            try { full = static_cast<bool>(placeFull(p)); } catch (...) { full = false; }
            if (full) {
                ++counters_.full;
                Tell(report, p, GuardOutcome::Full);
                continue;
            }
            bool one = false;
            try { one = static_cast<bool>(placeOne(p)); } catch (...) { one = false; }
            if (one) {
                ++counters_.kept;
                Tell(report, p, GuardOutcome::Kept);
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
    template <class Report>
    static void Tell(Report& report, const Pending& p, GuardOutcome o)
    {
        try { report(p, o); } catch (...) {}
    }

    std::vector<std::vector<Pending>> scopes_;
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
    if (o == GuardOutcome::Failed)
        return "flightcharm: guard failed " + std::string(name) + " in cell " + cell + " (no room even at 1x1)";
    return std::string();
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
// of lootListUnique[0..4] and cannot say which (RDM § 22.8); its coin is exact
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
