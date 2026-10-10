#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ForgePact::JumpSceneryMod {

// ---- Jump through scenery: the decision core (ForgePact #16) ---------------
//
// `jumpscenery 1` lets the local player's universal jump get over rocks,
// fences, carts and other scenery that stops it today, and only when the jump
// would land on open ground inside the room. Otherwise the game keeps its own
// answer, so a refused jump is the vanilla jump.
//
// What the game does (docs/jump-scenery-research.md, Live 1): in the take-off
// frame the jump walks along its direction in steps of about 4 px, asking at
// each step collision_circle(cx, cy, 15, Wall_Parent_obj, ...) and
// instance_position at two points beside it against Collision_Parent_obj. A
// blocked step stops the jump. While airborne the jump script skillsLeap runs
// every frame. Answering the five builtins below "no collision" for the
// player, against the Collision_Parent_obj family, lets the jump cross.
//
// The rule this header decides:
//   - the window: a family query whose self is the local player is inside it
//     when the latest player skillsLeap entry was this frame or the previous
//     one. That is checked on the query itself, at the point of use, so the
//     window lasts exactly as long as the jump. An entry after a gap of two
//     frames or more starts a new jump.
//   - the original runs first. A free answer is returned as it is.
//   - each jump is decided once, at its first really-blocked family query:
//     direction from the walk's first two circle centres, landing = take-off
//     + distance x direction. The landing and the points kLandingBandPx
//     before and after it must be inside the room and free of the family (the
//     original place_meeting). Granted, or refused.
//   - the distance (v2.2.1): a jump ends near its cursor, not at a fixed
//     length (Live 1 of the v2.2.1 hotfix measured 88.8 px for a cursor
//     100 px away and 113.0 px for one 111.5 px away), up to a maximum, the
//     cap. So with the cursor read at take-off, the distance is the cursor's,
//     no more than the cap when one is known; without a cursor, the reach of
//     the last clean jump; with neither, kStartReachPx. The cap is the length
//     of the latest clean jump that ended more than kCursorSlackPx short of
//     its cursor; a clean jump longer than the cap clears it.
//   - the reach and the cap survive a new player instance id: the game gives
//     the player a new one on every room change.
//   - a granted jump answers each really-blocked family query of its window
//     with the builtin's measured "no collision", except where a gate or a
//     lock (the excluded objects) is what blocks it, or is what it names.
//
// It is game-independent by contract - frame numbers, object indices,
// coordinates and booleans, never an instance or an RValue - so
// tests/jump_scenery_mod_harness.cpp compiles it whole. The adapter in
// ModuleMain.cpp supplies the frame, whether a call's self is the local
// player, the object argument, the original's own answer, whether an excluded
// object blocks the same query (the same original, same arguments and self,
// against each excluded object), the original place_meeting at a point, the
// room size, the player position and the mouse in room coordinates; it
// resolves every object, script and builtin by name.

// What a hooked call gets back. Real keeps the game's own answer; the other
// two are written into the result instead (measured in Live 1's J3).
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

// GameMaker's `noone`, the value an instance-returning query answers when it
// finds nothing. The game's own comes back as a ref to instance -4.
inline constexpr double kNoone = -4.0;

// The five builtins whose answers crossed a jump in Live 1's J3. `objectArg`
// is the position of the object argument.
enum class Builtin : int { PositionMeeting, PlaceMeeting, InstancePosition, CollisionLine, CollisionCircle };
inline constexpr int kBuiltinCount = 5;

struct BuiltinRow {
    std::string_view name;
    int              objectArg;
    Answer           answer;
};

inline constexpr BuiltinRow kBuiltins[kBuiltinCount] = {
    { "position_meeting",  2, Answer::False },
    { "place_meeting",     2, Answer::False },
    { "instance_position", 2, Answer::Noone },
    { "collision_line",    4, Answer::Noone },
    { "collision_circle",  3, Answer::Noone },
};

// A jump that moved less than this is a hop on the spot, not a reach.
inline constexpr double kMinReachPx = 32.0;
// The landing is checked here before and after the landing point too.
inline constexpr double kLandingBandPx = 16.0;
// A query is inside the window while frame - last entry < this.
inline constexpr int64_t kWindowFrames = 2;
// A clean jump that ends more than this short of its cursor was stopped by the
// jump's maximum length, so its length is the cap. Twice the larger miss Live 1
// measured on open ground (11 px).
inline constexpr double kCursorSlackPx = 24.0;
// The distance checked when there is no cursor and no clean jump to learn the
// reach from yet: the open-ground jump measured for save slot 14 (curated J11).
inline constexpr double kStartReachPx = 175.0;
// Two walk circles closer than this give no direction.
inline constexpr double kMinDirectionPx = 0.001;
// The family table forgets everything once it holds this many objects, so an
// adapter that passes something other than an object index cannot grow it
// without bound.
inline constexpr size_t kFamilyTableMax = 65536;

// RefusedNoReach no longer occurs (v2.2.1: a jump with nothing learned is
// checked at the cursor or at kStartReachPx); it stays so the stat line keeps
// its field.
enum class Decision : int { Undecided, Granted, RefusedLanding, RefusedRoom, RefusedNoReach };

// Where the last decided jump's landing distance came from (` last-target=`).
enum class Target : int { None, Cursor, Cap, Reach, Start };

inline constexpr std::string_view TargetName(Target t)
{
    switch (t) {
    case Target::None:   return "none";
    case Target::Cursor: return "cursor";
    case Target::Cap:    return "cap";
    case Target::Reach:  return "reach";
    case Target::Start:  return "start";
    }
    return "?";
}

// What `jumpscenery stat` prints. Kept across off/on.
struct Counters {
    uint64_t jumps = 0;           // the player's jumps the mod saw start while on
    uint64_t granted = 0;         // jumps let through
    uint64_t answered = 0;        // queries answered "no collision"
    uint64_t refusedLanding = 0;  // landing (or its band) blocked, or the take-off unreadable
    uint64_t refusedRoom = 0;     // landing (or its band) outside the room, or the room unreadable
    uint64_t refusedNoReach = 0;  // always 0 since v2.2.1 (the starting reach stands in)
    uint64_t noDirection = 0;     // a blocked query before the walk gave two circles (left undecided)
    uint64_t landedInside = 0;    // a granted jump that ended inside the family
    uint64_t beforeOpen = 0;      // blocked family queries in a take-off frame before its window opened
    // Where the take-off walk ran, blocked or clear, so a clear jump is the
    // positive control on the window. before-open= cannot say: it sees only
    // blocked queries, and walking and standing collision checks land in it
    // too. Both read 0 when no walk circle reached the mod as family at all
    // (a failed ancestry question looks like that).
    uint64_t walkBeforeOpen = 0;  // family collision_circle queries, blocked or not, in a take-off frame before its window opened
    uint64_t walkInWindow = 0;    // jumps whose take-off frame gave at least two family circle centres inside the window
    uint64_t excluded = 0;        // granted-window queries a gate or lock blocked, or named
};

// One hooked builtin call, after the original ran.
struct Query {
    Builtin builtin;
    int64_t frame;
    bool    playerSelf;      // the call's self is the local player
    int     object;          // the object argument; -1 when it is not an object index
    bool    reallyBlocked;   // the original's own answer: an instance, or true
    double  x = 0.0;         // collision_circle's centre; unused by the other rows
    double  y = 0.0;
};

class Mod {
public:
    // Is `ancestor` an ancestor of `object`? (object_is_ancestor: an object
    // is not its own ancestor.) Asked once per object, then remembered.
    using IsAncestorFn = std::function<bool(int object, int ancestor)>;
    // The original place_meeting(x, y, Collision_Parent_obj) with the player
    // as self: true when the family is there.
    using PlaceMeetingFn = std::function<bool(double x, double y)>;
    // room_width / room_height; false when they cannot be read.
    using RoomSizeFn = std::function<bool(double& width, double& height)>;
    // The local player's x / y; false when it cannot be read.
    using PositionFn = std::function<bool(double& x, double& y)>;
    // The mouse in room coordinates, and the name of the route that answered
    // (the stat line prints it); false when it cannot be read. A non-finite
    // value counts as unreadable.
    using CursorFn = std::function<bool(double& x, double& y, std::string& route)>;

    void SetIsAncestor(IsAncestorFn fn) { isAncestor_ = std::move(fn); familyTable_.clear(); }

    // The family (Collision_Parent_obj) and the excluded objects (Gate_Parent_obj,
    // Lock_obj), as indices the adapter resolved by name. A -1 in either means
    // a name did not resolve, and the mod then answers for nothing.
    void SetFamily(int family, std::vector<int> excluded)
    {
        family_ = family;
        excluded_ = std::move(excluded);
        familyTable_.clear();
    }

    void SetPlaceMeeting(PlaceMeetingFn fn) { placeMeeting_ = std::move(fn); }
    void SetRoomSize(RoomSizeFn fn) { roomSize_ = std::move(fn); }
    void SetPlayerPosition(PositionFn fn) { playerPosition_ = std::move(fn); }
    // Read once when a jump starts (with the take-off position) and when the
    // stat line is built.
    void SetCursor(CursorFn fn) { cursor_ = std::move(fn); }

    bool Configured() const
    {
        if (family_ < 0) return false;
        for (int e : excluded_) if (e < 0) return false;
        return true;
    }

    // `jumpscenery 1|0`. Any jump in progress is dropped either way; the
    // reach and the counters stay.
    void SetEnabled(bool on)
    {
        if (on == enabled_) return;
        enabled_ = on;
        DropJump();
    }
    bool Enabled() const { return enabled_; }

    // ---- the install: armed at launch, hooked once a character exists ----
    // The guide's Known Limitations item 8: a hook installed at character
    // select stalls the runner. A switch already on as the game starts (the
    // panel's launch commands carry `jumpscenery 1`, consumed while character
    // selection still runs) only arms; the six hooks go in once setup is done
    // and the local player resolves (the adapter asks HhResolveLocalPlayer).
    // While armed, the adapter's tick looks for the player only on the frames
    // LooksForPlayer picks, counted from the first armed frame, and tries once
    // per session; `jumpscenery 1` with a character loaded tries at once, again
    // after a refusal. A refused install turns the switch off.
    static constexpr unsigned long long kInstallPollFrames = 60;
    static constexpr bool LooksForPlayer(unsigned long long armedFrames)
    {
        return armedFrames % kInstallPollFrames == 0;
    }
    // Whether the adapter installs now: the switch on, the hooks not in yet,
    // setup done, the local player resolved, and, for the tick, no try of its
    // own yet this session.
    bool ShouldInstall(bool setupDone, bool playerResolved, bool fromTick) const
    {
        return enabled_ && !installed_ && !(fromTick && tickTried_) && setupDone && playerResolved;
    }
    // What the adapter's install returned: every hook in, or refused (the
    // adapter prints why, once).
    void NoteInstall(bool ok, bool fromTick)
    {
        if (fromTick) tickTried_ = true;
        if (ok) {
            installed_ = true;
            refused_ = false;
            return;
        }
        refused_ = true;
        SetEnabled(false);
    }
    bool Installed() const { return installed_; }
    // The ` install=` field the adapter appends to `jumpscenery 1` and `stat`:
    // installed once the hooks are in, refused after a refused install,
    // waiting-for-character while on without them, not-armed otherwise.
    std::string_view InstallStateName() const
    {
        if (installed_) return "installed";
        if (refused_) return "refused";
        return enabled_ ? "waiting-for-character" : "not-armed";
    }

    // The local player's identity, whenever the adapter resolves it. The game
    // gives the player a new instance id on every room change (Live 1: 261723
    // in Town_01_rm, 297089 in Act_01_01), so a new id keeps the reach and the
    // cap; only a jump in progress is dropped. Another character loaded keeps
    // them as well: the guide's Known Limitations item 47 already accepts a
    // jump decided at old values after a Jump Power change, and a cap too long
    // for the new character checks a point past where it lands, which
    // landed-inside= counts.
    void NotePlayer(int64_t id)
    {
        if (havePlayer_ && id == player_) return;
        havePlayer_ = true;
        player_ = id;
        DropJump();
    }

    // A skillsLeap entry, before the original runs (so a walk inside its
    // first call is inside the window). True when it counted: the mod is on
    // and the self is the local player.
    bool OnLeapEntry(int64_t frame, bool playerSelf)
    {
        if (!enabled_ || !playerSelf) return false;
        if (!WindowOpen(frame)) {
            if (jump_.active) CloseJump();
            StartJump(frame);
        }
        jump_.lastEntry = frame;
        return true;
    }

    bool WindowOpen(int64_t frame) const
    {
        return enabled_ && jump_.active && frame >= jump_.lastEntry && frame - jump_.lastEntry < kWindowFrames;
    }

    // One hooked builtin call, after its original ran. `excludedBlocks()`
    // runs the same original against each excluded object and says whether
    // any of them is there; it is called only for a really-blocked family
    // query inside a granted window.
    template <class ExcludedBlocksFn>
    Answer OnQuery(const Query& q, ExcludedBlocksFn&& excludedBlocks)
    {
        const int i = static_cast<int>(q.builtin);
        if (!enabled_ || !q.playerSelf || i < 0 || i >= kBuiltinCount || !Configured()) return Answer::Real;
        const Class c = Classify(q.object);
        if (!c.family && !c.excluded) return Answer::Real;
        if (!WindowOpen(q.frame)) {
            if (c.family) NotePending(q.frame, q.reallyBlocked, q.builtin == Builtin::CollisionCircle);
            return Answer::Real;
        }
        if (q.frame == jump_.takeoffFrame && c.family) {
            if (q.builtin == Builtin::CollisionCircle && jump_.circles < 2) {
                jump_.circleX[jump_.circles] = q.x;
                jump_.circleY[jump_.circles] = q.y;
                ++jump_.circles;
            }
            if (q.reallyBlocked) jump_.takeoffBlocked = true;
        }
        if (!q.reallyBlocked) return Answer::Real;
        if (c.excluded) {
            if (jump_.decision == Decision::Granted) ++counters_.excluded;
            return Answer::Real;
        }
        if (jump_.decision == Decision::Undecided) Decide();
        if (jump_.decision != Decision::Granted) return Answer::Real;
        bool gate = true;
        try { gate = static_cast<bool>(excludedBlocks()); } catch (...) { gate = true; }
        if (gate) {
            ++counters_.excluded;
            return Answer::Real;
        }
        ++counters_.answered;
        ++jump_.answered;
        return kBuiltins[i].answer;
    }

    // The per-frame tick: housekeeping only. Once a jump's window has closed
    // it records the landing, learns the reach and counts landed-inside=.
    // Returns at once while off.
    void Tick(int64_t frame)
    {
        if (!enabled_) return;
        if (jump_.active && !WindowOpen(frame)) CloseJump();
    }

    // A reach learned from a clean jump; without one, kStartReachPx stands in.
    bool HasReach() const { return haveReach_; }
    double Reach() const { return reach_; }
    bool HasCap() const { return haveCap_; }
    double Cap() const { return cap_; }
    const Counters& Stats() const { return counters_; }
    Decision JumpDecision() const { return jump_.decision; }
    bool JumpActive() const { return jump_.active; }

    // `jumpscenery 1` / `jumpscenery 0`.
    std::string StatusLine() const { return enabled_ ? "jumpscenery: on" : "jumpscenery: off"; }

    // Bare `jumpscenery` / `jumpscenery stat`.
    std::string StatLine() const
    {
        const Counters& c = counters_;
        std::string s = StatusLine();
        s += " reach=" + (haveReach_ ? std::to_string(std::llround(reach_))
                                     : std::to_string(std::llround(kStartReachPx)) + " (start)");
        s += " jumps=" + std::to_string(c.jumps);
        s += " granted=" + std::to_string(c.granted);
        s += " answered=" + std::to_string(c.answered);
        s += " refused-landing=" + std::to_string(c.refusedLanding);
        s += " refused-room=" + std::to_string(c.refusedRoom);
        s += " refused-no-reach=" + std::to_string(c.refusedNoReach);
        s += " no-direction=" + std::to_string(c.noDirection);
        s += " landed-inside=" + std::to_string(c.landedInside);
        s += " before-open=" + std::to_string(c.beforeOpen);
        s += " walk-before-open=" + std::to_string(c.walkBeforeOpen);
        s += " walk-in-window=" + std::to_string(c.walkInWindow);
        s += " excluded=" + std::to_string(c.excluded);
        s += " cap=" + (haveCap_ ? std::to_string(std::llround(cap_)) : std::string("none"));
        double cx = 0.0, cy = 0.0;
        std::string route;
        if (ReadCursor(cx, cy, route))
            s += " cursor=" + std::to_string(std::llround(cx)) + "," + std::to_string(std::llround(cy)) + "@" + route;
        else
            s += " cursor=unreadable";
        s += " last-target=" + std::string(TargetName(lastTarget_));
        s += " last-check=" + (haveLastCheck_ ? std::to_string(std::llround(lastCheck_)) : std::string("none"));
        double w = 0.0, h = 0.0;
        if (ReadRoom(w, h))
            s += " room=" + std::to_string(std::llround(w)) + "x" + std::to_string(std::llround(h));
        else
            s += " room=unknown";
        return s;
    }

private:
    struct Class { bool family = false; bool excluded = false; };

    struct Jump {
        bool active = false;
        int64_t takeoffFrame = 0;
        int64_t lastEntry = 0;
        bool haveTakeoff = false;
        double takeoffX = 0.0, takeoffY = 0.0;
        bool haveCursor = false;       // the cursor, read with the take-off position
        double cursorX = 0.0, cursorY = 0.0;
        int circles = 0;
        double circleX[2] = { 0.0, 0.0 };
        double circleY[2] = { 0.0, 0.0 };
        bool takeoffBlocked = false;   // a really-blocked family query in the take-off frame
        uint64_t answered = 0;
        Decision decision = Decision::Undecided;
    };

    // The object's own index or one of its descendants. A failed ancestry
    // question is "no" for the family and "yes" for the exclusion: either
    // way the query keeps the game's answer.
    bool IsOrDescends(int object, int ancestor, bool onFailure) const
    {
        if (object == ancestor) return true;
        if (!isAncestor_) return false;
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
            if (IsOrDescends(object, e, true)) { c.excluded = true; break; }
        }
        if (familyTable_.size() >= kFamilyTableMax) familyTable_.clear();
        familyTable_.emplace(object, static_cast<uint8_t>((c.family ? 1 : 0) | (c.excluded ? 2 : 0)));
        return c;
    }

    bool ReadRoom(double& w, double& h) const
    {
        if (!roomSize_) return false;
        try { return roomSize_(w, h); } catch (...) { return false; }
    }

    bool ReadPosition(double& x, double& y) const
    {
        if (!playerPosition_) return false;
        try { return playerPosition_(x, y); } catch (...) { return false; }
    }

    bool ReadCursor(double& x, double& y, std::string& route) const
    {
        if (!cursor_) return false;
        bool ok = false;
        try { ok = cursor_(x, y, route); } catch (...) { return false; }
        return ok && std::isfinite(x) && std::isfinite(y);
    }

    // Unreadable counts as blocked: the landing is then refused.
    bool FamilyAt(double x, double y) const
    {
        if (!placeMeeting_) return true;
        try { return placeMeeting_(x, y); } catch (...) { return true; }
    }

    // A player-self family query outside any window. Only the ones in the
    // frame a jump then starts in are kept: the blocked ones for before-open=,
    // the circles (blocked or not) for walk-before-open=.
    void NotePending(int64_t frame, bool blocked, bool circle)
    {
        if (!pendingValid_ || pendingFrame_ != frame) {
            pendingValid_ = true;
            pendingFrame_ = frame;
            pendingBlocked_ = 0;
            pendingCircles_ = 0;
        }
        if (blocked) ++pendingBlocked_;
        if (circle) ++pendingCircles_;
    }

    void DropJump()
    {
        jump_ = Jump{};
        pendingValid_ = false;
        pendingBlocked_ = 0;
        pendingCircles_ = 0;
    }

    void StartJump(int64_t frame)
    {
        jump_ = Jump{};
        jump_.active = true;
        jump_.takeoffFrame = frame;
        jump_.lastEntry = frame;
        jump_.haveTakeoff = ReadPosition(jump_.takeoffX, jump_.takeoffY);
        std::string route;
        jump_.haveCursor = ReadCursor(jump_.cursorX, jump_.cursorY, route);
        ++counters_.jumps;
        if (pendingValid_ && pendingFrame_ == frame) {
            counters_.walkBeforeOpen += pendingCircles_;
            if (pendingBlocked_ > 0) {
                counters_.beforeOpen += pendingBlocked_;
                jump_.takeoffBlocked = true;
            }
        }
        pendingValid_ = false;
        pendingBlocked_ = 0;
        pendingCircles_ = 0;
    }

    void CloseJump()
    {
        double x = 0.0, y = 0.0;
        const bool landed = ReadPosition(x, y);
        if (landed && jump_.haveTakeoff && jump_.answered == 0 && !jump_.takeoffBlocked) {
            const double moved = std::hypot(x - jump_.takeoffX, y - jump_.takeoffY);
            if (moved >= kMinReachPx) {
                haveReach_ = true;
                reach_ = moved;
                // Longer than the cap: the cap was out of date. Well short of
                // its own cursor: the jump's maximum stopped it, so it is the cap.
                if (haveCap_ && moved > cap_) {
                    haveCap_ = false;
                    cap_ = 0.0;
                }
                if (jump_.haveCursor) {
                    const double aimed = std::hypot(jump_.cursorX - jump_.takeoffX, jump_.cursorY - jump_.takeoffY);
                    if (aimed - moved > kCursorSlackPx) {
                        haveCap_ = true;
                        cap_ = moved;
                    }
                }
            }
        }
        if (landed && jump_.decision == Decision::Granted && FamilyAt(x, y)) ++counters_.landedInside;
        if (jump_.circles >= 2) ++counters_.walkInWindow;
        jump_ = Jump{};
    }

    void Decide()
    {
        if (jump_.circles < 2) { ++counters_.noDirection; return; }
        const double dx = jump_.circleX[1] - jump_.circleX[0];
        const double dy = jump_.circleY[1] - jump_.circleY[0];
        const double len = std::hypot(dx, dy);
        if (!(len > kMinDirectionPx)) { ++counters_.noDirection; return; }
        const double ux = dx / len, uy = dy / len;
        if (!jump_.haveTakeoff) {
            lastTarget_ = Target::None;
            haveLastCheck_ = false;
            Refuse(Decision::RefusedLanding);
            return;
        }
        // The distance: the take-off cursor's, no more than the cap; without a
        // cursor the learned reach; with neither, the starting reach.
        double dist = 0.0;
        if (jump_.haveCursor) {
            dist = std::hypot(jump_.cursorX - jump_.takeoffX, jump_.cursorY - jump_.takeoffY);
            lastTarget_ = Target::Cursor;
            if (haveCap_ && dist > cap_) {
                dist = cap_;
                lastTarget_ = Target::Cap;
            }
        } else if (haveReach_) {
            dist = reach_;
            lastTarget_ = Target::Reach;
        } else {
            dist = kStartReachPx;
            lastTarget_ = Target::Start;
        }
        haveLastCheck_ = true;
        lastCheck_ = dist;
        const double lx = jump_.takeoffX + dist * ux;
        const double ly = jump_.takeoffY + dist * uy;
        const double px[3] = { lx - kLandingBandPx * ux, lx, lx + kLandingBandPx * ux };
        const double py[3] = { ly - kLandingBandPx * uy, ly, ly + kLandingBandPx * uy };
        double w = 0.0, h = 0.0;
        if (!ReadRoom(w, h)) { Refuse(Decision::RefusedRoom); return; }
        for (int k = 0; k < 3; ++k) {
            // Written so that a NaN fails it.
            if (!(px[k] >= 0.0 && px[k] < w && py[k] >= 0.0 && py[k] < h)) { Refuse(Decision::RefusedRoom); return; }
        }
        for (int k = 0; k < 3; ++k) {
            if (FamilyAt(px[k], py[k])) { Refuse(Decision::RefusedLanding); return; }
        }
        jump_.decision = Decision::Granted;
        ++counters_.granted;
    }

    void Refuse(Decision d)
    {
        jump_.decision = d;
        if (d == Decision::RefusedLanding) ++counters_.refusedLanding;
        else if (d == Decision::RefusedRoom) ++counters_.refusedRoom;
        else if (d == Decision::RefusedNoReach) ++counters_.refusedNoReach;
    }

    IsAncestorFn isAncestor_;
    PlaceMeetingFn placeMeeting_;
    RoomSizeFn roomSize_;
    PositionFn playerPosition_;
    CursorFn cursor_;
    int family_ = -1;
    std::vector<int> excluded_;
    std::unordered_map<int, uint8_t> familyTable_;
    bool enabled_ = false;
    bool installed_ = false;
    bool tickTried_ = false;
    bool refused_ = false;
    bool havePlayer_ = false;
    int64_t player_ = 0;
    bool haveReach_ = false;
    double reach_ = 0.0;
    bool haveCap_ = false;
    double cap_ = 0.0;
    Target lastTarget_ = Target::None;
    bool haveLastCheck_ = false;
    double lastCheck_ = 0.0;
    Jump jump_;
    bool pendingValid_ = false;
    int64_t pendingFrame_ = 0;
    uint64_t pendingBlocked_ = 0;
    uint64_t pendingCircles_ = 0;
    Counters counters_;
};

} // namespace ForgePact::JumpSceneryMod
