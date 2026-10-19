#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>

#include "ForgePact/JumpScenery.hpp"

namespace ForgePact::Mark1Jump {

// ---- Mark 1 set: the jump's decision core (ForgePact #166) -----------------
//
// With all four Mark 1 pieces worn and the "Mark 1 set" switch on, the local
// player's universal jump always goes three times its natural maximum toward
// the cursor, passes scenery the way `jumpscenery 1` lets it, and lands only
// somewhere walkable (owner, 2026-10-19). This header decides each jump; the
// adapter in ModuleMain.cpp supplies every read and carries out every answer.
//
// The rule this header decides:
//   - the jump: the first local-player skillsLeap entry after a gap of two
//     frames or more (jumpscenery's window rule), decided once, at that
//     entry, before the original runs, while the bonus is active. Inactive,
//     nothing is read and nothing is decided.
//   - the direction: the cursor read at take-off. A cursor closer than
//     kMinCursorPx to the take-off (no usable direction), and the jump is
//     vanilla (refused-cursor=); no readable cursor, and it is vanilla too
//     (refused-unreadable=).
//   - M, the natural maximum: jumpscenery's learned cap, else its learned
//     reach, else its 175 px start value (ChooseMax).
//   - the landing is always the full distance: the first candidate is
//     take-off + 3M along the cursor's direction, whatever the cursor's
//     distance. A candidate is accepted when it and the points kBandPx before
//     and after it are walkable (inside the room, no Collision_Parent_obj
//     family meeting there, and a free path-grid cell when the room has a
//     grid) and the segment from the take-off to it crosses no gate or lock.
//   - the back-off: a refused candidate gives way to the one kStepPx nearer,
//     for as long as the distance stays greater than M; the first (farthest)
//     one accepted is the landing (backed-off=). When none is, the jump is
//     vanilla: refused-gate= when every candidate was walkable and failed only
//     on a gate or lock, refused-landing= otherwise.
//   - a failed read (unbound, throwing, false or non-finite) still fails
//     closed, but is counted by its kind and never as the game outcome it
//     guards: an unreadable take-off, cursor or room is refused-unreadable=
//     (a readable cursor too close stays refused-cursor=); a candidate whose
//     family, grid or gate read failed is skipped and the back-off goes on;
//     with no candidate accepted and any skipped on a failed read, the jump is
//     refused-unreadable=. Candidates refused by a successful read are counted
//     by reason (room, family, grid, gate).
//   - the scale: L is the landing's distance, N = min(cursor distance, M) the
//     natural length (a vanilla jump ends at a cursor inside M), f = L / N.
//     After each local skillsLeap call that moved the player, the call's
//     displacement is scaled by f; the call that would carry the player past
//     L from the take-off ends at L, and later calls leave the player there.
//   - a granted jump answers each really-blocked family query of the player
//     inside its window with jumpscenery's measured "no collision", except
//     where a gate or a lock blocks it or is what it names.
//   - the learner: when the jump's window closes, its natural (unscaled)
//     displacement, length and take-off cursor are handed on (LearnFn), so
//     the reach and the cap keep being learned while jumps are scaled.
//
// Known limit: when M (or, for a near cursor, N) overestimates this
// character's natural jump, a granted jump ends short of its landing, on the
// segment, at a point the landing test did not check.
//
// It is game-independent by contract - frame numbers, coordinates, booleans
// and the measured answers jumpscenery already names, never an instance or an
// RValue - so tests/mark1_jump_harness.cpp compiles it whole. The adapter
// supplies the frame, whether a call's self is the local player, the player's
// position, the cursor in room coordinates, M, the room size, the original
// place_meeting against the family with the player as self, the path-grid
// cell, the original collision_line against the gates and locks, and writes
// the position OnLeapExit returns; it resolves every name at run time.

using JumpSceneryMod::Answer;
using JumpSceneryMod::Builtin;

// The jump goes this many times the natural maximum.
inline constexpr double kScale = 3.0;
// The back-off's step toward the take-off.
inline constexpr double kStepPx = 16.0;
// The landing is also checked this far before and after it (jumpscenery's band).
inline constexpr double kBandPx = JumpSceneryMod::kLandingBandPx;
// A cursor closer than this to the take-off gives no usable direction, so the
// largest factor is 3M / kMinCursorPx.
inline constexpr double kMinCursorPx = 16.0;
// An entry is part of the jump while frame - last entry < this.
inline constexpr int64_t kWindowFrames = JumpSceneryMod::kWindowFrames;
// M when nothing has been learned yet.
inline constexpr double kStartMaxPx = JumpSceneryMod::kStartReachPx;
// A learned length shorter than this is a hop on the spot, not a maximum.
inline constexpr double kMinMaxPx = JumpSceneryMod::kMinReachPx;
// A call that moved less than this did not move the player.
inline constexpr double kMovedEpsilonPx = 1e-6;

// Where M came from (`max=<M>(<source>)` on the stat line).
enum class MaxSource : int { Cap, Reach, Start };

inline constexpr std::string_view MaxSourceName(MaxSource s)
{
    switch (s) {
    case MaxSource::Cap:   return "cap";
    case MaxSource::Reach: return "reach";
    case MaxSource::Start: return "start";
    }
    return "?";
}

struct Max {
    double value = kStartMaxPx;
    MaxSource source = MaxSource::Start;
};

// jumpscenery's learned cap, else its learned reach, else the start value. A
// value that is not a finite length of at least kMinMaxPx is passed over.
inline Max ChooseMax(bool haveCap, double cap, bool haveReach, double reach)
{
    if (haveCap && std::isfinite(cap) && cap >= kMinMaxPx) return Max{ cap, MaxSource::Cap };
    if (haveReach && std::isfinite(reach) && reach >= kMinMaxPx) return Max{ reach, MaxSource::Reach };
    return Max{ kStartMaxPx, MaxSource::Start };
}

// How a jump was decided. Granted lands at 3M; BackedOff at a nearer
// candidate past M; the four refusals are vanilla jumps. RefusedCursor,
// RefusedLanding and RefusedGate mean every read succeeded and the game's
// answer refused; RefusedUnreadable means a read the decision needed failed.
enum class Decision : int { None, Granted, BackedOff, RefusedCursor, RefusedLanding, RefusedGate, RefusedUnreadable };

inline constexpr std::string_view DecisionName(Decision d)
{
    switch (d) {
    case Decision::None:           return "none";
    case Decision::Granted:        return "granted";
    case Decision::BackedOff:      return "backed-off";
    case Decision::RefusedCursor:  return "refused-cursor";
    case Decision::RefusedLanding: return "refused-landing";
    case Decision::RefusedGate:    return "refused-gate";
    case Decision::RefusedUnreadable: return "refused-unreadable";
    }
    return "?";
}

inline constexpr bool IsGranted(Decision d) { return d == Decision::Granted || d == Decision::BackedOff; }

// What the landing test found for one candidate: Ok, the first thing the
// game's answer refused it on (the room, the family, the path grid on one of
// the three points, then the gate or lock on the segment), or Unread when a
// read failed first. An Unread candidate is never accepted.
enum class Landing : int { Ok, Room, Family, Grid, Gate, Unread };

inline constexpr std::string_view LandingName(Landing l)
{
    switch (l) {
    case Landing::Ok:     return "ok";
    case Landing::Room:   return "room";
    case Landing::Family: return "family";
    case Landing::Grid:   return "grid";
    case Landing::Gate:   return "gate";
    case Landing::Unread: return "unread";
    }
    return "?";
}

// The reads a decision makes, for counting the one that failed.
enum class ReadKind : int { Takeoff, Cursor, Room, Family, Grid, Gate };
inline constexpr int kReadKinds = 6;

inline constexpr std::string_view ReadKindName(ReadKind k)
{
    switch (k) {
    case ReadKind::Takeoff: return "takeoff";
    case ReadKind::Cursor:  return "cursor";
    case ReadKind::Room:    return "room";
    case ReadKind::Family:  return "family";
    case ReadKind::Grid:    return "grid";
    case ReadKind::Gate:    return "gate";
    }
    return "?";
}

// The path-grid cell under a point. NoGrid (the room has none) counts free;
// Unreadable (the adapter could not read the cell) is a failed read.
enum class Cell : int { Free, Blocked, NoGrid, Unreadable };

// What the stat line prints. Kept across off/on.
struct Counters {
    uint64_t jumps = 0;           // local jumps that started while the bonus was active
    uint64_t granted = 0;         // jumps given a landing past M, backed off or not
    uint64_t backedOff = 0;       // of those, the ones whose 3M point was refused
    uint64_t refusedCursor = 0;   // vanilla: a cursor read, too close to give a direction
    uint64_t refusedLanding = 0;  // vanilla: every candidate read, none past M walkable
    uint64_t refusedGate = 0;     // vanilla: every candidate read and walkable but behind a gate or lock
    uint64_t scaled = 0;          // skillsLeap calls whose displacement was rewritten
    uint64_t clamped = 0;         // granted jumps that reached their landing distance
    uint64_t answered = 0;        // family queries answered "no collision"
    uint64_t excluded = 0;        // granted-window queries a gate or lock blocked, or named
    // vanilla: the take-off, the cursor or the room could not be read, or no
    // candidate passed and at least one was skipped on a failed read
    uint64_t refusedUnreadable = 0;
    // Candidates refused by a successful read, by the reason the test gave.
    uint64_t rejectedRoom = 0;
    uint64_t rejectedFamily = 0;
    uint64_t rejectedGrid = 0;
    uint64_t rejectedGate = 0;
    // Failed reads, by kind: per jump for the take-off, the cursor and the
    // room; per candidate for the family, the grid and the gate line.
    uint64_t unreadTakeoff = 0;
    uint64_t unreadCursor = 0;
    uint64_t unreadRoom = 0;
    uint64_t unreadFamily = 0;
    uint64_t unreadGrid = 0;
    uint64_t unreadGate = 0;
};

// The position OnLeapExit asks the adapter to write, when write is true.
struct Move {
    bool write = false;
    double x = 0.0;
    double y = 0.0;
};

// What the learner is handed when a jump's window closes.
struct Learned {
    double takeoffX = 0.0, takeoffY = 0.0;
    bool haveCursor = false;
    double cursorX = 0.0, cursorY = 0.0;
    double naturalDx = 0.0, naturalDy = 0.0;  // the sum of the unscaled displacements
    double naturalLength = 0.0;               // its length
    Decision decision = Decision::None;
};

class Core {
public:
    // The local player's x / y; false when it cannot be read.
    using PositionFn = std::function<bool(double& x, double& y)>;
    // The mouse in room coordinates; false when it cannot be read. A
    // non-finite value counts as unreadable.
    using CursorFn = std::function<bool(double& x, double& y)>;
    // M, asked once per jump at take-off; unset, the start value.
    using MaxFn = std::function<Max()>;
    // room_width / room_height; false when they cannot be read. A non-finite
    // value counts as unreadable.
    using RoomSizeFn = std::function<bool(double& width, double& height)>;
    // The original place_meeting(x, y, Collision_Parent_obj) with the player
    // as self: true when the family is there. The adapter reports a failed
    // read by throwing; unset, every read fails.
    using PlaceMeetingFn = std::function<bool(double x, double y)>;
    // The global.pathGrid cell under (x, y); Cell::Unreadable, a throw or an
    // unset callback is a failed read.
    using GridFn = std::function<Cell(double x, double y)>;
    // The original collision_line from (x0, y0) to (x1, y1) against
    // Gate_Parent_obj and Lock_obj: true when either is on it. A failed read
    // throws; unset, every read fails.
    using GateOnLineFn = std::function<bool(double x0, double y0, double x1, double y1)>;
    using LearnFn = std::function<void(const Learned&)>;

    void SetPlayerPosition(PositionFn fn) { position_ = std::move(fn); }
    void SetCursor(CursorFn fn) { cursor_ = std::move(fn); }
    void SetMax(MaxFn fn) { max_ = std::move(fn); }
    void SetRoomSize(RoomSizeFn fn) { roomSize_ = std::move(fn); }
    void SetPlaceMeeting(PlaceMeetingFn fn) { placeMeeting_ = std::move(fn); }
    void SetGrid(GridFn fn) { grid_ = std::move(fn); }
    void SetGateOnLine(GateOnLineFn fn) { gateOnLine_ = std::move(fn); }
    void SetLearner(LearnFn fn) { learn_ = std::move(fn); }

    // The set bonus is active (the switch on and all four pieces worn). Any
    // jump in progress is dropped on a change, unlearned; counters stay.
    void SetActive(bool on)
    {
        if (on == active_) return;
        active_ = on;
        jump_ = Jump{};
    }
    bool Active() const { return active_; }

    bool WindowOpen(int64_t frame) const
    {
        return active_ && jump_.active && frame >= jump_.lastEntry && frame - jump_.lastEntry < kWindowFrames;
    }

    // A skillsLeap entry, before the original runs. The first after a gap of
    // kWindowFrames or more starts and decides a jump. True when it counted:
    // the bonus is active and the self is the local player.
    bool OnLeapEntry(int64_t frame, bool playerSelf)
    {
        if (!active_ || !playerSelf) return false;
        if (!WindowOpen(frame)) {
            if (jump_.active) CloseJump();
            StartJump(frame);
        } else {
            jump_.haveBefore = ReadPosition(jump_.beforeX, jump_.beforeY);
        }
        jump_.lastEntry = frame;
        return true;
    }

    // After that call's original ran: what to write into the player's
    // position, if anything. The call's displacement always counts toward
    // the natural length; it is rewritten only for a granted jump.
    Move OnLeapExit(int64_t frame, bool playerSelf)
    {
        Move m;
        if (!active_ || !playerSelf || !jump_.active || frame != jump_.lastEntry || !jump_.haveBefore) return m;
        jump_.haveBefore = false;
        double ax = 0.0, ay = 0.0;
        if (!ReadPosition(ax, ay)) return m;
        const double dx = ax - jump_.beforeX, dy = ay - jump_.beforeY;
        jump_.naturalDx += dx;
        jump_.naturalDy += dy;
        ++jump_.calls;
        if (!IsGranted(jump_.decision)) return m;
        if (!(std::hypot(dx, dy) > kMovedEpsilonPx)) return m;
        ++counters_.scaled;
        m.write = true;
        if (jump_.reachedLanding) {
            m.x = jump_.beforeX;
            m.y = jump_.beforeY;
            return m;
        }
        double nx = jump_.beforeX + jump_.factor * dx;
        double ny = jump_.beforeY + jump_.factor * dy;
        const double rx = nx - jump_.takeoffX, ry = ny - jump_.takeoffY;
        const double r = std::hypot(rx, ry);
        if (r >= jump_.landing) {
            nx = jump_.takeoffX + rx * (jump_.landing / r);
            ny = jump_.takeoffY + ry * (jump_.landing / r);
            jump_.reachedLanding = true;
            ++counters_.clamped;
        }
        m.x = nx;
        m.y = ny;
        return m;
    }

    // One hooked collision builtin, after its original ran: the adapter says
    // whether the object argument is the family or a gate or lock (or a
    // descendant), and `excludedBlocks()` runs the same original against each
    // gate and lock. Only a granted jump's window answers.
    template <class ExcludedBlocksFn>
    Answer OnQuery(Builtin builtin, int64_t frame, bool playerSelf, bool family, bool excluded, bool reallyBlocked,
        ExcludedBlocksFn&& excludedBlocks)
    {
        const int i = static_cast<int>(builtin);
        if (!playerSelf || !reallyBlocked || i < 0 || i >= JumpSceneryMod::kBuiltinCount) return Answer::Real;
        if (!WindowOpen(frame) || !IsGranted(jump_.decision)) return Answer::Real;
        if (excluded) {
            ++counters_.excluded;
            return Answer::Real;
        }
        if (!family) return Answer::Real;
        bool gate = true;
        try { gate = static_cast<bool>(excludedBlocks()); } catch (...) { gate = true; }
        if (gate) {
            ++counters_.excluded;
            return Answer::Real;
        }
        ++counters_.answered;
        return JumpSceneryMod::kBuiltins[i].answer;
    }

    // The per-frame tick: housekeeping only. Once a jump's window has closed
    // it hands the natural length to the learner.
    void Tick(int64_t frame)
    {
        if (jump_.active && !WindowOpen(frame)) CloseJump();
    }

    // The landing test for the candidate `dist` px from (tx, ty) along the
    // unit (ux, uy), with the room already read. Stops at the first refusal
    // or failed read; on Landing::Unread, `failed` (when given) names the
    // read that failed.
    Landing Check(double tx, double ty, double ux, double uy, double dist, double roomW, double roomH,
        ReadKind* failed = nullptr) const
    {
        const double lx = tx + dist * ux, ly = ty + dist * uy;
        const double px[3] = { lx - kBandPx * ux, lx, lx + kBandPx * ux };
        const double py[3] = { ly - kBandPx * uy, ly, ly + kBandPx * uy };
        for (int k = 0; k < 3; ++k) {
            // Written so that a NaN fails it.
            if (!(px[k] >= 0.0 && px[k] < roomW && py[k] >= 0.0 && py[k] < roomH)) return Landing::Room;
        }
        for (int k = 0; k < 3; ++k) {
            bool meets = true;
            if (!ReadFamily(px[k], py[k], meets)) return Unread(failed, ReadKind::Family);
            if (meets) return Landing::Family;
        }
        for (int k = 0; k < 3; ++k) {
            Cell c = Cell::Blocked;
            if (!ReadGrid(px[k], py[k], c)) return Unread(failed, ReadKind::Grid);
            if (c == Cell::Blocked) return Landing::Grid;
        }
        bool gate = true;
        if (!ReadGate(tx, ty, lx, ly, gate)) return Unread(failed, ReadKind::Gate);
        if (gate) return Landing::Gate;
        return Landing::Ok;
    }

    const Counters& Stats() const { return counters_; }
    bool JumpActive() const { return jump_.active; }
    // The latest decided jump's decision, M and its source, landing distance
    // and factor (0 for a vanilla jump).
    Decision LastDecision() const { return last_.decision; }
    Max LastMax() const { return last_.max; }
    double LastLanding() const { return last_.landing; }
    double LastFactor() const { return last_.factor; }
    // How many candidates the latest decision tested.
    int LastCandidates() const { return last_.candidates; }

private:
    struct Jump {
        bool active = false;
        int64_t lastEntry = 0;
        bool haveTakeoff = false;
        double takeoffX = 0.0, takeoffY = 0.0;
        bool haveCursor = false;
        double cursorX = 0.0, cursorY = 0.0;
        bool haveBefore = false;       // the position read at this call's entry
        double beforeX = 0.0, beforeY = 0.0;
        Decision decision = Decision::None;
        double landing = 0.0;          // L
        double factor = 0.0;           // f = L / N
        bool reachedLanding = false;
        double naturalDx = 0.0, naturalDy = 0.0;
        uint64_t calls = 0;
    };

    struct Last {
        Decision decision = Decision::None;
        Max max;
        double landing = 0.0;
        double factor = 0.0;
        int candidates = 0;
    };

    bool ReadPosition(double& x, double& y) const
    {
        if (!position_) return false;
        bool ok = false;
        try { ok = position_(x, y); } catch (...) { return false; }
        return ok && std::isfinite(x) && std::isfinite(y);
    }

    bool ReadCursor(double& x, double& y) const
    {
        if (!cursor_) return false;
        bool ok = false;
        try { ok = cursor_(x, y); } catch (...) { return false; }
        return ok && std::isfinite(x) && std::isfinite(y);
    }

    Max ReadMax() const
    {
        if (!max_) return Max{};
        Max m;
        try { m = max_(); } catch (...) { return Max{}; }
        if (!std::isfinite(m.value) || m.value < kMinMaxPx) return Max{};
        return m;
    }

    bool ReadRoom(double& w, double& h) const
    {
        if (!roomSize_) return false;
        bool ok = false;
        try { ok = roomSize_(w, h); } catch (...) { return false; }
        return ok && std::isfinite(w) && std::isfinite(h);
    }

    static Landing Unread(ReadKind* failed, ReadKind kind)
    {
        if (failed) *failed = kind;
        return Landing::Unread;
    }

    // Each landing read answers whether it succeeded; the caller treats a
    // failed one as a candidate it cannot accept, and never as the game's
    // answer (so a skipped gate check is never a pass, nor a gate found).
    bool ReadFamily(double x, double y, bool& meets) const
    {
        if (!placeMeeting_) return false;
        try { meets = placeMeeting_(x, y); } catch (...) { return false; }
        return true;
    }

    // A room with no grid counts free.
    bool ReadGrid(double x, double y, Cell& c) const
    {
        if (!grid_) return false;
        try { c = grid_(x, y); } catch (...) { return false; }
        return c == Cell::Free || c == Cell::Blocked || c == Cell::NoGrid;
    }

    bool ReadGate(double x0, double y0, double x1, double y1, bool& gate) const
    {
        if (!gateOnLine_) return false;
        try { gate = gateOnLine_(x0, y0, x1, y1); } catch (...) { return false; }
        return true;
    }

    void StartJump(int64_t frame)
    {
        jump_ = Jump{};
        jump_.active = true;
        jump_.lastEntry = frame;
        ++counters_.jumps;
        jump_.haveTakeoff = ReadPosition(jump_.takeoffX, jump_.takeoffY);
        jump_.haveBefore = jump_.haveTakeoff;
        jump_.beforeX = jump_.takeoffX;
        jump_.beforeY = jump_.takeoffY;
        jump_.haveCursor = ReadCursor(jump_.cursorX, jump_.cursorY);
        if (!jump_.haveTakeoff) ++counters_.unreadTakeoff;
        if (!jump_.haveCursor) ++counters_.unreadCursor;
        Decide();
    }

    // Fails closed throughout: a candidate is never accepted on a read that
    // failed. A failed read is counted by its kind and the jump that ends
    // vanilla on one is refused-unreadable, so refused-landing, refused-gate
    // and refused-cursor only ever mean the game's own answer.
    void Decide()
    {
        last_ = Last{};
        last_.max = ReadMax();
        if (!jump_.haveTakeoff || !jump_.haveCursor) { Refuse(Decision::RefusedUnreadable); return; }
        const double cx = jump_.cursorX - jump_.takeoffX, cy = jump_.cursorY - jump_.takeoffY;
        const double d = std::hypot(cx, cy);
        if (!(d >= kMinCursorPx)) { Refuse(Decision::RefusedCursor); return; }
        const double ux = cx / d, uy = cy / d;
        const double m = last_.max.value;
        double w = 0.0, h = 0.0;
        if (!ReadRoom(w, h)) {
            ++counters_.unreadRoom;
            Refuse(Decision::RefusedUnreadable);
            return;
        }
        bool allGated = true;
        bool anyUnread = false;
        for (int k = 0;; ++k) {
            const double dist = kScale * m - kStepPx * k;
            if (!(dist > m)) break;
            ++last_.candidates;
            ReadKind failed = ReadKind::Family;
            const Landing l = Check(jump_.takeoffX, jump_.takeoffY, ux, uy, dist, w, h, &failed);
            if (l == Landing::Ok) {
                Grant(k == 0 ? Decision::Granted : Decision::BackedOff, dist, dist / std::min(d, m));
                return;
            }
            if (l == Landing::Unread) {
                anyUnread = true;
                CountUnread(failed);
                continue;
            }
            CountRejected(l);
            if (l != Landing::Gate) allGated = false;
        }
        if (anyUnread) Refuse(Decision::RefusedUnreadable);
        else Refuse(allGated && last_.candidates > 0 ? Decision::RefusedGate : Decision::RefusedLanding);
    }

    void CountUnread(ReadKind kind)
    {
        switch (kind) {
        case ReadKind::Takeoff: ++counters_.unreadTakeoff; break;
        case ReadKind::Cursor:  ++counters_.unreadCursor; break;
        case ReadKind::Room:    ++counters_.unreadRoom; break;
        case ReadKind::Family:  ++counters_.unreadFamily; break;
        case ReadKind::Grid:    ++counters_.unreadGrid; break;
        case ReadKind::Gate:    ++counters_.unreadGate; break;
        }
    }

    void CountRejected(Landing l)
    {
        switch (l) {
        case Landing::Room:   ++counters_.rejectedRoom; break;
        case Landing::Family: ++counters_.rejectedFamily; break;
        case Landing::Grid:   ++counters_.rejectedGrid; break;
        case Landing::Gate:   ++counters_.rejectedGate; break;
        case Landing::Ok:
        case Landing::Unread: break;
        }
    }

    void Grant(Decision d, double landing, double factor)
    {
        jump_.decision = d;
        jump_.landing = landing;
        jump_.factor = factor;
        last_.decision = d;
        last_.landing = landing;
        last_.factor = factor;
        ++counters_.granted;
        if (d == Decision::BackedOff) ++counters_.backedOff;
    }

    void Refuse(Decision d)
    {
        jump_.decision = d;
        last_.decision = d;
        if (d == Decision::RefusedCursor) ++counters_.refusedCursor;
        else if (d == Decision::RefusedGate) ++counters_.refusedGate;
        else if (d == Decision::RefusedUnreadable) ++counters_.refusedUnreadable;
        else ++counters_.refusedLanding;
    }

    void CloseJump()
    {
        if (learn_ && jump_.haveTakeoff && jump_.calls > 0) {
            Learned l;
            l.takeoffX = jump_.takeoffX;
            l.takeoffY = jump_.takeoffY;
            l.haveCursor = jump_.haveCursor;
            l.cursorX = jump_.cursorX;
            l.cursorY = jump_.cursorY;
            l.naturalDx = jump_.naturalDx;
            l.naturalDy = jump_.naturalDy;
            l.naturalLength = std::hypot(jump_.naturalDx, jump_.naturalDy);
            l.decision = jump_.decision;
            try { learn_(l); } catch (...) {}
        }
        jump_ = Jump{};
    }

    PositionFn position_;
    CursorFn cursor_;
    MaxFn max_;
    RoomSizeFn roomSize_;
    PlaceMeetingFn placeMeeting_;
    GridFn grid_;
    GateOnLineFn gateOnLine_;
    LearnFn learn_;
    bool active_ = false;
    Jump jump_;
    Last last_;
    Counters counters_;
};

} // namespace ForgePact::Mark1Jump
