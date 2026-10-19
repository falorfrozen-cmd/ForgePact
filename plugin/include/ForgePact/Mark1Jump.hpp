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
//   - the direction: the cursor read at take-off. No cursor, or a cursor
//     closer than kMinCursorPx to the take-off (no usable direction), and the
//     jump is vanilla (refused-cursor=).
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
// candidate past M; the three refusals are vanilla jumps.
enum class Decision : int { None, Granted, BackedOff, RefusedCursor, RefusedLanding, RefusedGate };

inline constexpr std::string_view DecisionName(Decision d)
{
    switch (d) {
    case Decision::None:           return "none";
    case Decision::Granted:        return "granted";
    case Decision::BackedOff:      return "backed-off";
    case Decision::RefusedCursor:  return "refused-cursor";
    case Decision::RefusedLanding: return "refused-landing";
    case Decision::RefusedGate:    return "refused-gate";
    }
    return "?";
}

inline constexpr bool IsGranted(Decision d) { return d == Decision::Granted || d == Decision::BackedOff; }

// What the landing test found for one candidate: Ok, or the first thing that
// refused it (the room, the family, the path grid on one of the three points,
// then the gate or lock on the segment).
enum class Landing : int { Ok, Room, Family, Grid, Gate };

inline constexpr std::string_view LandingName(Landing l)
{
    switch (l) {
    case Landing::Ok:     return "ok";
    case Landing::Room:   return "room";
    case Landing::Family: return "family";
    case Landing::Grid:   return "grid";
    case Landing::Gate:   return "gate";
    }
    return "?";
}

// The path-grid cell under a point. NoGrid (the room has none) counts free.
enum class Cell : int { Free, Blocked, NoGrid };

// What the stat line prints. Kept across off/on.
struct Counters {
    uint64_t jumps = 0;           // local jumps that started while the bonus was active
    uint64_t granted = 0;         // jumps given a landing past M, backed off or not
    uint64_t backedOff = 0;       // of those, the ones whose 3M point was refused
    uint64_t refusedCursor = 0;   // vanilla: no cursor, or one too close to give a direction
    uint64_t refusedLanding = 0;  // vanilla: no candidate past M walkable (or the take-off unreadable)
    uint64_t refusedGate = 0;     // vanilla: every candidate walkable but behind a gate or lock
    uint64_t scaled = 0;          // skillsLeap calls whose displacement was rewritten
    uint64_t clamped = 0;         // granted jumps that reached their landing distance
    uint64_t answered = 0;        // family queries answered "no collision"
    uint64_t excluded = 0;        // granted-window queries a gate or lock blocked, or named
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
    // room_width / room_height; false when they cannot be read.
    using RoomSizeFn = std::function<bool(double& width, double& height)>;
    // The original place_meeting(x, y, Collision_Parent_obj) with the player
    // as self: true when the family is there.
    using PlaceMeetingFn = std::function<bool(double x, double y)>;
    // The global.pathGrid cell under (x, y).
    using GridFn = std::function<Cell(double x, double y)>;
    // The original collision_line from (x0, y0) to (x1, y1) against
    // Gate_Parent_obj and Lock_obj: true when either is on it.
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
    // unit (ux, uy), with the room already read.
    Landing Check(double tx, double ty, double ux, double uy, double dist, double roomW, double roomH) const
    {
        const double lx = tx + dist * ux, ly = ty + dist * uy;
        const double px[3] = { lx - kBandPx * ux, lx, lx + kBandPx * ux };
        const double py[3] = { ly - kBandPx * uy, ly, ly + kBandPx * uy };
        for (int k = 0; k < 3; ++k) {
            // Written so that a NaN fails it.
            if (!(px[k] >= 0.0 && px[k] < roomW && py[k] >= 0.0 && py[k] < roomH)) return Landing::Room;
        }
        for (int k = 0; k < 3; ++k) {
            if (FamilyAt(px[k], py[k])) return Landing::Family;
        }
        for (int k = 0; k < 3; ++k) {
            if (GridBlocked(px[k], py[k])) return Landing::Grid;
        }
        if (GateOn(tx, ty, lx, ly)) return Landing::Gate;
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
        try { return roomSize_(w, h); } catch (...) { return false; }
    }

    // Unreadable counts as blocked, so an unread landing is never accepted.
    bool FamilyAt(double x, double y) const
    {
        if (!placeMeeting_) return true;
        try { return placeMeeting_(x, y); } catch (...) { return true; }
    }

    // A room with no grid counts free; an unread cell counts blocked.
    bool GridBlocked(double x, double y) const
    {
        if (!grid_) return true;
        Cell c = Cell::Blocked;
        try { c = grid_(x, y); } catch (...) { return true; }
        return c == Cell::Blocked;
    }

    // A gate check that cannot run counts as a gate: a candidate is never
    // accepted because its gate check was skipped.
    bool GateOn(double x0, double y0, double x1, double y1) const
    {
        if (!gateOnLine_) return true;
        try { return gateOnLine_(x0, y0, x1, y1); } catch (...) { return true; }
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
        Decide();
    }

    void Decide()
    {
        last_ = Last{};
        last_.max = ReadMax();
        if (!jump_.haveTakeoff) { Refuse(Decision::RefusedLanding); return; }
        if (!jump_.haveCursor) { Refuse(Decision::RefusedCursor); return; }
        const double cx = jump_.cursorX - jump_.takeoffX, cy = jump_.cursorY - jump_.takeoffY;
        const double d = std::hypot(cx, cy);
        if (!(d >= kMinCursorPx)) { Refuse(Decision::RefusedCursor); return; }
        const double ux = cx / d, uy = cy / d;
        const double m = last_.max.value;
        double w = 0.0, h = 0.0;
        const bool room = ReadRoom(w, h);
        bool allGated = true;
        for (int k = 0;; ++k) {
            const double dist = kScale * m - kStepPx * k;
            if (!(dist > m)) break;
            ++last_.candidates;
            const Landing l = room ? Check(jump_.takeoffX, jump_.takeoffY, ux, uy, dist, w, h) : Landing::Room;
            if (l == Landing::Ok) {
                Grant(k == 0 ? Decision::Granted : Decision::BackedOff, dist, dist / std::min(d, m));
                return;
            }
            if (l != Landing::Gate) allGated = false;
        }
        Refuse(allGated && last_.candidates > 0 ? Decision::RefusedGate : Decision::RefusedLanding);
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
