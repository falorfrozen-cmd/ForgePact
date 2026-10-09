// Behavioral regression harness for the Jump through scenery mod's decision
// core (JumpScenery.hpp, `jumpscenery 1|0|stat`, ForgePact #16).
//
// The Python runner injects the REAL ForgePact::JumpSceneryMod header below.
// No game is touched: the controlled world is an object table with parents
// (object_is_ancestor answers from it the way the runtime does - an object is
// not its own ancestor), a list of solid boxes that the original
// place_meeting(x, y, Collision_Parent_obj) answers from, a room size, a
// player position and, for a Mod made with makeCursor, a mouse position in room
// coordinates. Frames are plain numbers.
//
// Baseline: with the mod off, every query gets its real answer; with it on, a
// query outside the window, from a self that is not the player, against
// another family (Enemy_Parent_obj), or that was really free gets its real
// answer too; a refused jump gets real answers for its whole window; the
// window closes two frames after the last skillsLeap entry. Target: a granted
// jump answers each of the five builtins with its measured value and is
// decided once; the landing guard's refusals; the reach learned only from a
// clear jump of at least 32 px; landed-inside=, before-open=, walk-before-open=,
// walk-in-window= and excluded=. The landing distance (v2.2.1), on Live 1's two
// measured prop jumps: with no cursor the learned reach decides and refuses
// both (baseline); with a cursor the landing is checked at its distance, up to
// the cap, and with neither at the 175 px starting reach; the reach and the cap
// survive a new player instance (target). The install: with a character loaded,
// `jumpscenery 1` installs at once (baseline); at character select it only
// arms, the tick installs once the player resolves and tries once per session,
// and a refusal turns the switch off (target).
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

// PRODUCTION_JUMPSCENERY_MOD

using namespace ForgePact::JumpSceneryMod;

// ---- the controlled world -------------------------------------------------
enum Obj : int {
    AvoidableParent = 1, CollisionParent = 957, CollisionProp = 959, Fence = 5001, Cart = 5002,
    WallParent = 5701, InvisibleWall = 2269, GateParent = 1881, GateChild = 5003, LockObj = 2509,
    LockChild = 5004, EnemyAggroable = 300, EnemyParent = 301, Skeleton = 5005,
};
static const std::map<int, int> kParent = {
    { CollisionParent, AvoidableParent }, { CollisionProp, CollisionParent }, { Fence, CollisionProp },
    { Cart, CollisionProp }, { WallParent, CollisionParent }, { InvisibleWall, WallParent },
    { GateParent, WallParent }, { GateChild, GateParent }, { LockObj, CollisionParent }, { LockChild, LockObj },
    { EnemyParent, EnemyAggroable }, { Skeleton, EnemyParent },
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

struct Box { double x0, y0, x1, y1; };

// A solid laid along a line: every point within `half` px of the line from
// (ax, ay) along the unit (ux, uy), between `from` and `to` px out.
struct Strip { double ax, ay, ux, uy, from, to, half; };

struct World {
    double px = 1000.0, py = 1000.0;
    bool posReadable = true;
    double roomW = 2000.0, roomH = 2000.0;
    bool roomReadable = true;
    // The mouse in room coordinates, for a Mod made with makeCursor.
    double cx = 1000.0, cy = 1000.0;
    bool cursorReadable = true;
    std::vector<Box> solids;
    std::vector<Strip> strips;
    std::vector<std::pair<double, double>> placeAsked;
    long posReads = 0;
    long roomReads = 0;

    bool blocked(double x, double y) const
    {
        for (const Box& b : solids)
            if (x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1) return true;
        for (const Strip& s : strips) {
            const double dx = x - s.ax, dy = y - s.ay;
            const double along = dx * s.ux + dy * s.uy;
            const double across = std::fabs(dx * s.uy - dy * s.ux);
            if (along >= s.from && along <= s.to && across <= s.half) return true;
        }
        return false;
    }
};

// ---- Live 1's two prop jumps (the shipped v2.2.0 plugin, slot 14) -----------
// Town (Town_01_rm, 2800x2400): the prop south of the take-off (1074.2, 1383.7)
// blocks the walk 22-40 px out and is free at 125 px. The reach Live 1 learned
// came from a clean jump from (912, 822) to (911.0, 910.8), its cursor at
// (912, 922): 88.8 px, 11 px short of a 100 px cursor, so no cap.
static constexpr double kTownX = 1074.2, kTownY = 1383.7;
static void townWorld(World& w)
{
    w.roomW = 2800.0;
    w.roomH = 2400.0;
    w.solids = { Box{ 1040.0, 1395.0, 1110.0, 1495.0 } };
}
// Outskirts (Act_01_01, 18272x8704): from (2031.8, 3966.0) toward a cursor at
// (1901.5, 3867.5), 163.3 px away, a large tree blocks every point within 20 px
// of that line from 10 to 135 px out. The reach came from a clean jump from the
// same take-off to (2129.8, 3909.7), its cursor at (2131.5, 3916.0): 113.0 px.
static constexpr double kOutX = 2031.8, kOutY = 3966.0, kOutCursorX = 1901.5, kOutCursorY = 3867.5;
static double outUx() { return (kOutCursorX - kOutX) / std::hypot(kOutCursorX - kOutX, kOutCursorY - kOutY); }
static double outUy() { return (kOutCursorY - kOutY) / std::hypot(kOutCursorX - kOutX, kOutCursorY - kOutY); }
static void outdoorWorld(World& w)
{
    w.roomW = 18272.0;
    w.roomH = 8704.0;
    w.solids.clear();
    w.strips = { Strip{ kOutX, kOutY, outUx(), outUy(), 10.0, 135.0, 20.0 } };
}

static Box around(double x, double y, double half) { return Box{ x - half, y - half, x + half, y + half }; }

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static Mod make(World& w)
{
    Mod m;
    m.SetIsAncestor(objectIsAncestor);
    m.SetFamily(CollisionParent, { GateParent, LockObj });
    m.SetPlaceMeeting([&w](double x, double y) { w.placeAsked.emplace_back(x, y); return w.blocked(x, y); });
    m.SetRoomSize([&w](double& rw, double& rh) {
        ++w.roomReads;
        if (!w.roomReadable) return false;
        rw = w.roomW;
        rh = w.roomH;
        return true;
    });
    m.SetPlayerPosition([&w](double& x, double& y) {
        ++w.posReads;
        if (!w.posReadable) return false;
        x = w.px;
        y = w.py;
        return true;
    });
    m.NotePlayer(1);
    return m;
}

// make() with a cursor provider: the world's (cx, cy), through route "test".
static Mod makeCursor(World& w)
{
    Mod m = make(w);
    m.SetCursor([&w](double& x, double& y, std::string& route) {
        if (!w.cursorReadable) return false;
        x = w.cx;
        y = w.cy;
        route = "test";
        return true;
    });
    return m;
}

static long g_ExcludedAsks = 0;
static Answer ask(Mod& m, Builtin b, int64_t frame, bool player, int object, bool blocked,
    bool excludedBlocks = false, double x = 0.0, double y = 0.0)
{
    Query q{ b, frame, player, object, blocked, x, y };
    return m.OnQuery(q, [excludedBlocks]() { ++g_ExcludedAsks; return excludedBlocks; });
}

// One of the take-off walk's circles: collision_circle(cx, cy, 15,
// Wall_Parent_obj, ...) from the player, free unless told otherwise.
static Answer circle(Mod& m, int64_t frame, double x, double y, bool blocked = false)
{
    return ask(m, Builtin::CollisionCircle, frame, true, WallParent, blocked, false, x, y);
}

// A skillsLeap entry at `frame` and the walk's first two circles along
// (ux, uy), the first 5 px below the player's origin as Live 1 measured.
static void takeoff(Mod& m, World& w, int64_t frame, double ux = 0.0, double uy = 1.0)
{
    m.OnLeapEntry(frame, true);
    circle(m, frame, w.px, w.py + 5.0);
    circle(m, frame, w.px + 4.0 * ux, w.py + 5.0 + 4.0 * uy);
}

// A clear jump south from (1000, 1000) that travels `dist` px: entries on
// frames f .. f+10, closed by the tick at f+12; the player is then put back.
static void clearJump(Mod& m, World& w, int64_t f, double dist)
{
    w.px = 1000.0;
    w.py = 1000.0;
    takeoff(m, w, f);
    for (int k = 1; k <= 10; ++k) m.OnLeapEntry(f + k, true);
    w.py += dist;
    m.Tick(f + 12);
    w.px = 1000.0;
    w.py = 1000.0;
}

// Ends the jump whose last entry was `last`, landing at (x, y), then puts the
// player back at (1000, 1000).
static void land(Mod& m, World& w, int64_t last, double x, double y)
{
    w.px = x;
    w.py = y;
    m.Tick(last + 2);
    w.px = 1000.0;
    w.py = 1000.0;
}

// A clean jump from (x0, y0) to (x1, y1): nothing blocked, nothing answered.
// The cursor is whatever the world holds when it starts.
static void cleanJump(Mod& m, World& w, int64_t f, double x0, double y0, double x1, double y1)
{
    w.px = x0;
    w.py = y0;
    takeoff(m, w, f);
    for (int k = 1; k <= 10; ++k) m.OnLeapEntry(f + k, true);
    w.px = x1;
    w.py = y1;
    m.Tick(f + 12);
}

// A jump from (x0, y0) along (ux, uy) whose walk meets scenery at once: the
// first blocked family query decides it. Returns that query's answer; the
// jump stays open.
static Answer propJump(Mod& m, World& w, int64_t f, double x0, double y0, double ux, double uy)
{
    w.px = x0;
    w.py = y0;
    takeoff(m, w, f, ux, uy);
    return ask(m, Builtin::PlaceMeeting, f, true, Fence, true);
}

static bool has(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }

static std::string counts(const Counters& c)
{
    return "jumps=" + std::to_string(c.jumps) + " granted=" + std::to_string(c.granted)
        + " answered=" + std::to_string(c.answered) + " refused-landing=" + std::to_string(c.refusedLanding)
        + " refused-room=" + std::to_string(c.refusedRoom) + " refused-no-reach=" + std::to_string(c.refusedNoReach)
        + " no-direction=" + std::to_string(c.noDirection) + " landed-inside=" + std::to_string(c.landedInside)
        + " before-open=" + std::to_string(c.beforeOpen) + " walk-before-open=" + std::to_string(c.walkBeforeOpen)
        + " walk-in-window=" + std::to_string(c.walkInWindow) + " excluded=" + std::to_string(c.excluded);
}

static bool allZero(const Counters& c)
{
    return c.jumps == 0 && c.granted == 0 && c.answered == 0 && c.refusedLanding == 0 && c.refusedRoom == 0
        && c.refusedNoReach == 0 && c.noDirection == 0 && c.landedInside == 0 && c.beforeOpen == 0
        && c.walkBeforeOpen == 0 && c.walkInWindow == 0 && c.excluded == 0;
}

static std::string text(Answer a) { return std::string(AnswerName(a)); }

static const Builtin kRows[] = {
    Builtin::PositionMeeting, Builtin::PlaceMeeting, Builtin::InstancePosition, Builtin::CollisionLine,
    Builtin::CollisionCircle,
};

static bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

static bool asked(const World& w, double x, double y)
{
    for (const auto& p : w.placeAsked)
        if (near(p.first, x) && near(p.second, y)) return true;
    return false;
}

int main()
{
    // ---- the table itself ----------------------------------------------
    {
        const bool rows = kBuiltinCount == 5
            && kBuiltins[(int)Builtin::PositionMeeting].name == "position_meeting" && kBuiltins[(int)Builtin::PositionMeeting].objectArg == 2
            && kBuiltins[(int)Builtin::PlaceMeeting].name == "place_meeting" && kBuiltins[(int)Builtin::PlaceMeeting].objectArg == 2
            && kBuiltins[(int)Builtin::InstancePosition].name == "instance_position" && kBuiltins[(int)Builtin::InstancePosition].objectArg == 2
            && kBuiltins[(int)Builtin::CollisionLine].name == "collision_line" && kBuiltins[(int)Builtin::CollisionLine].objectArg == 4
            && kBuiltins[(int)Builtin::CollisionCircle].name == "collision_circle" && kBuiltins[(int)Builtin::CollisionCircle].objectArg == 3;
        check("table/five_rows_and_object_arguments", rows);
        const bool answers = kBuiltins[(int)Builtin::PositionMeeting].answer == Answer::False
            && kBuiltins[(int)Builtin::PlaceMeeting].answer == Answer::False
            && kBuiltins[(int)Builtin::InstancePosition].answer == Answer::Noone
            && kBuiltins[(int)Builtin::CollisionLine].answer == Answer::Noone
            && kBuiltins[(int)Builtin::CollisionCircle].answer == Answer::Noone && kNoone == -4.0;
        check("table/measured_answers", answers);
        check("table/named_constants", kMinReachPx == 32.0 && kLandingBandPx == 16.0 && kWindowFrames == 2);
    }

    // ---- baseline: the mod off -------------------------------------------
    {
        World w;
        Mod m = make(w);
        const long excludedBefore = g_ExcludedAsks;
        const bool opened = m.OnLeapEntry(10, true);
        bool real = true;
        std::string bad;
        for (int k = 0; k < 3; ++k) {
            for (Builtin b : kRows) {
                if (ask(m, b, 10 + k, true, CollisionParent, true) != Answer::Real) { real = false; bad += std::string(kBuiltins[(int)b].name) + " "; }
                if (ask(m, b, 10 + k, true, Fence, true) != Answer::Real) { real = false; bad += std::string(kBuiltins[(int)b].name) + " "; }
            }
        }
        check("baseline/off_every_query_real",
            !m.Enabled() && real && allZero(m.Stats()) && w.placeAsked.empty() && g_ExcludedAsks == excludedBefore,
            bad + counts(m.Stats()));
        check("baseline/off_opens_no_window", !opened && !m.WindowOpen(10) && !m.WindowOpen(11));
        m.Tick(12);
        m.Tick(40);
        check("baseline/off_tick_reads_nothing", w.posReads == 0 && w.roomReads == 0 && !m.HasReach(),
            "pos=" + std::to_string(w.posReads) + " room=" + std::to_string(w.roomReads));
    }

    // ---- baseline: the mod on, queries it does not own -------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        bool real = true;
        for (Builtin b : kRows) real = real && ask(m, b, 100, true, CollisionParent, true) == Answer::Real;
        check("baseline/outside_window_real", m.HasReach() && real && m.Stats().granted == 0 && m.Stats().answered == 0,
            counts(m.Stats()));

        takeoff(m, w, 200);
        const Answer mine = ask(m, Builtin::PlaceMeeting, 200, true, Fence, true);
        const uint64_t answered = m.Stats().answered;
        bool other = true;
        for (Builtin b : kRows) other = other && ask(m, b, 200, false, CollisionParent, true) == Answer::Real;
        check("baseline/other_self_real", mine == Answer::False && other && m.Stats().answered == answered,
            counts(m.Stats()));

        bool enemy = true;
        for (Builtin b : kRows) {
            enemy = enemy && ask(m, b, 200, true, EnemyParent, true) == Answer::Real;
            enemy = enemy && ask(m, b, 200, true, Skeleton, true) == Answer::Real;
        }
        check("baseline/enemy_family_real", enemy && m.Stats().answered == answered, counts(m.Stats()));

        const bool nonObject = ask(m, Builtin::InstancePosition, 200, true, -1, true) == Answer::Real
            && ask(m, Builtin::InstancePosition, 200, true, -3, true) == Answer::Real;
        check("baseline/non_object_argument_real", nonObject && m.Stats().answered == answered, counts(m.Stats()));

        land(m, w, 200, 1000.0, 1100.0);
        const uint64_t jumps = m.Stats().jumps;
        const bool otherEntry = m.OnLeapEntry(300, false);
        check("baseline/other_self_opens_no_window", !otherEntry && !m.WindowOpen(300) && m.Stats().jumps == jumps);
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        takeoff(m, w, 100);
        bool real = true;
        for (Builtin b : kRows) real = real && ask(m, b, 100, true, CollisionParent, false) == Answer::Real;
        const bool undecided = m.JumpDecision() == Decision::Undecided && w.placeAsked.empty() && m.Stats().granted == 0;
        const Answer blocked = ask(m, Builtin::PositionMeeting, 100, true, CollisionParent, true);
        bool stillReal = true;
        for (Builtin b : kRows) stillReal = stillReal && ask(m, b, 100, true, CollisionParent, false) == Answer::Real;
        check("baseline/free_answer_never_rewritten",
            real && undecided && blocked == Answer::False && stillReal && m.Stats().answered == 1, counts(m.Stats()));
    }

    // ---- baseline: a refused jump keeps its real answers ------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.solids.push_back(around(1000.0, 1100.0, 4.0));
        takeoff(m, w, 100);
        const Answer first = ask(m, Builtin::InstancePosition, 100, true, CollisionParent, true);
        const size_t asks = w.placeAsked.size();
        bool real = first == Answer::Real;
        for (int64_t f = 101; f <= 105; ++f) {
            m.OnLeapEntry(f, true);
            for (Builtin b : kRows) real = real && ask(m, b, f, true, CollisionParent, true) == Answer::Real;
        }
        real = real && ask(m, Builtin::PlaceMeeting, 106, true, Fence, true) == Answer::Real;
        check("baseline/refused_jump_real_whole_window",
            real && m.JumpDecision() == Decision::RefusedLanding && m.Stats().refusedLanding == 1
                && m.Stats().answered == 0 && w.placeAsked.size() == asks,
            counts(m.Stats()));
    }

    // ---- baseline: the window is the jump --------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        takeoff(m, w, 100);
        const bool takeoffOpen = m.WindowOpen(100) && m.WindowOpen(101) && !m.WindowOpen(102);
        for (int64_t f = 101; f <= 105; ++f) m.OnLeapEntry(f, true);
        const bool open = takeoffOpen && m.WindowOpen(105) && m.WindowOpen(106) && !m.WindowOpen(107);
        const Answer at106 = ask(m, Builtin::PlaceMeeting, 106, true, Fence, true);
        const Answer at107 = ask(m, Builtin::PlaceMeeting, 107, true, Fence, true);
        check("baseline/window_closes_two_frames_after_last_entry",
            open && at106 == Answer::False && at107 == Answer::Real, text(at106) + " " + text(at107));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        m.OnLeapEntry(100, true);
        m.OnLeapEntry(101, true);
        m.OnLeapEntry(102, true);
        const uint64_t one = m.Stats().jumps;
        m.OnLeapEntry(104, true);
        const uint64_t two = m.Stats().jumps;
        m.OnLeapEntry(105, true);
        check("baseline/entry_after_a_gap_starts_a_new_jump", one == 1 && two == 2 && m.Stats().jumps == 2,
            counts(m.Stats()));
    }

    // ---- target: a granted jump ------------------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        const long roomBefore = w.roomReads;
        takeoff(m, w, 100);
        bool each = true;
        std::string got;
        for (Builtin b : kRows) {
            const Answer a = ask(m, b, 100, true, CollisionParent, true);
            got += std::string(kBuiltins[(int)b].name) + "=" + text(a) + " ";
            each = each && a == kBuiltins[(int)b].answer;
        }
        check("target/granted_answers_each_builtin",
            each && m.JumpDecision() == Decision::Granted && m.Stats().granted == 1 && m.Stats().answered == 5,
            got + counts(m.Stats()));
        for (int64_t f = 101; f <= 103; ++f) {
            m.OnLeapEntry(f, true);
            for (Builtin b : kRows) ask(m, b, f, true, Fence, true);
        }
        check("target/decided_once_per_jump",
            w.placeAsked.size() == 3 && w.roomReads - roomBefore == 1 && m.Stats().granted == 1 && m.Stats().answered == 20,
            "place=" + std::to_string(w.placeAsked.size()) + " room=" + std::to_string(w.roomReads - roomBefore) + " " + counts(m.Stats()));

        land(m, w, 103, 1000.0, 1100.0);
        w.solids.push_back(around(1000.0, 1100.0, 4.0));
        takeoff(m, w, 200);
        const Answer second = ask(m, Builtin::PlaceMeeting, 200, true, Fence, true);
        check("target/next_jump_decides_anew",
            second == Answer::Real && m.Stats().granted == 1 && m.Stats().refusedLanding == 1 && m.Stats().jumps == 3,
            counts(m.Stats()));
    }
    {
        // Diagonal: the walk's first two centres (1000,1005) -> (1003,1009)
        // give (0.6, 0.8); a third circle elsewhere changes nothing, and a
        // solid where a straight-south landing would be does not matter.
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.solids.push_back(around(1000.0, 1100.0, 4.0));
        m.OnLeapEntry(100, true);
        circle(m, 100, 1000.0, 1005.0);
        circle(m, 100, 1003.0, 1009.0);
        circle(m, 100, 900.0, 1005.0);
        const Answer a = ask(m, Builtin::InstancePosition, 100, true, Cart, true);
        const bool points = w.placeAsked.size() == 3 && asked(w, 1060.0, 1080.0)
            && asked(w, 1060.0 - 16.0 * 0.6, 1080.0 - 16.0 * 0.8) && asked(w, 1060.0 + 16.0 * 0.6, 1080.0 + 16.0 * 0.8);
        check("target/landing_from_reach_and_walk_direction", a == Answer::Noone && points && m.Stats().granted == 1,
            counts(m.Stats()));
    }

    // ---- target: the landing guard's refusals -----------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.solids.push_back(around(1000.0, 1100.0, 4.0));
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::CollisionCircle, 100, true, CollisionParent, true);
        check("target/refused_landing", a == Answer::Real && m.Stats().refusedLanding == 1 && m.Stats().granted == 0
            && m.JumpDecision() == Decision::RefusedLanding, counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.solids.push_back(around(1000.0, 1116.0, 2.0));
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::CollisionCircle, 100, true, CollisionParent, true);
        check("target/refused_landing_band", a == Answer::Real && m.Stats().refusedLanding == 1 && m.Stats().granted == 0,
            counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.roomH = 1050.0;
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::PlaceMeeting, 100, true, CollisionParent, true);
        check("target/refused_room", a == Answer::Real && m.Stats().refusedRoom == 1 && m.Stats().refusedLanding == 0
            && m.Stats().granted == 0 && w.placeAsked.empty(), counts(m.Stats()));
    }
    {
        // Take-off at y=1890: L=1990 and the near end 1974 are inside a
        // 2000-px room, only the far end 2006 is not.
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.py = 1890.0;
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::PlaceMeeting, 100, true, CollisionParent, true);
        check("target/refused_room_far_band_end_only", a == Answer::Real && m.Stats().refusedRoom == 1
            && m.Stats().granted == 0, counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.roomReadable = false;
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::PlaceMeeting, 100, true, CollisionParent, true);
        check("target/refused_room_unreadable", a == Answer::Real && m.Stats().refusedRoom == 1 && m.Stats().granted == 0,
            counts(m.Stats()));
    }
    {
        // No reach learned and no cursor: the starting reach is checked, so a
        // solid there refuses the landing; refused-no-reach no longer occurs.
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        w.solids.push_back(around(1000.0, 1000.0 + kStartReachPx, 4.0));
        takeoff(m, w, 100);
        bool real = true;
        for (Builtin b : kRows) real = real && ask(m, b, 100, true, CollisionParent, true) == Answer::Real;
        m.OnLeapEntry(101, true);
        real = real && ask(m, Builtin::PlaceMeeting, 101, true, Fence, true) == Answer::Real;
        check("target/no_reach_refused_at_the_starting_reach", real && !m.HasReach() && m.Stats().refusedNoReach == 0
            && m.Stats().refusedLanding == 1 && m.Stats().granted == 0 && asked(w, 1000.0, 1000.0 + kStartReachPx)
            && m.JumpDecision() == Decision::RefusedLanding, counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        m.OnLeapEntry(100, true);
        const Answer before = ask(m, Builtin::InstancePosition, 100, true, CollisionParent, true);
        const bool undecided = m.JumpDecision() == Decision::Undecided && m.Stats().noDirection == 1;
        circle(m, 100, 1000.0, 1005.0);
        const Answer one = ask(m, Builtin::InstancePosition, 100, true, CollisionParent, true);
        circle(m, 100, 1000.0, 1009.0);
        const Answer two = ask(m, Builtin::InstancePosition, 100, true, CollisionParent, true);
        check("target/no_direction", before == Answer::Real && undecided && one == Answer::Real
            && two == Answer::Noone && m.Stats().noDirection == 2 && m.Stats().granted == 1, counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        m.OnLeapEntry(100, true);
        circle(m, 100, 1000.0, 1005.0);
        circle(m, 100, 1000.0, 1005.0);
        const Answer a = ask(m, Builtin::InstancePosition, 100, true, CollisionParent, true);
        check("target/no_direction_zero_length", a == Answer::Real && m.Stats().noDirection == 1 && m.Stats().granted == 0
            && m.JumpDecision() == Decision::Undecided, counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        w.posReadable = false;
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::PlaceMeeting, 100, true, CollisionParent, true);
        check("target/unreadable_takeoff_refused", a == Answer::Real && m.Stats().granted == 0
            && m.Stats().refusedLanding == 1, counts(m.Stats()));
    }

    // ---- baseline: Live 1's prop jumps with no cursor ------------------------
    // No cursor provider: the learned reach decides, as in v2.2.0, and puts the
    // checked band inside the prop (town, reach 89) and the tree (outskirts,
    // reach 113). Both refused, which is what Live 1 measured.
    {
        World w;
        Mod m = make(w);
        townWorld(w);
        m.SetEnabled(true);
        cleanJump(m, w, 10, 912.0, 822.0, 911.0, 910.8);
        const bool learned = m.HasReach() && std::llround(m.Reach()) == 89;
        const Answer a = propJump(m, w, 100, kTownX, kTownY, 0.0, 1.0);
        check("baseline/no_cursor_learned_reach_decides_town", learned && a == Answer::Real
            && m.JumpDecision() == Decision::RefusedLanding && m.Stats().refusedLanding == 1 && m.Stats().granted == 0
            && asked(w, kTownX, kTownY + m.Reach() - kLandingBandPx), m.StatLine());
    }
    {
        World w;
        Mod m = make(w);
        outdoorWorld(w);
        m.SetEnabled(true);
        cleanJump(m, w, 10, kOutX, kOutY, 2129.8, 3909.7);
        const bool learned = m.HasReach() && std::llround(m.Reach()) == 113;
        const Answer a = propJump(m, w, 100, kOutX, kOutY, outUx(), outUy());
        check("baseline/no_cursor_learned_reach_decides_outdoor", learned && a == Answer::Real
            && m.JumpDecision() == Decision::RefusedLanding && m.Stats().refusedLanding == 1 && m.Stats().granted == 0,
            m.StatLine());
    }

    // ---- target: the landing checked where the jump goes ----------------------
    // Live 1: a jump ends near its cursor (88.8 px for a cursor 100 px away,
    // 113.0 for 111.5), so the landing is checked at the cursor's distance, no
    // further than the cap. The learned reach decides only without a cursor.
    {
        World w;
        Mod m = makeCursor(w);
        townWorld(w);
        m.SetEnabled(true);
        w.cx = 912.0;
        w.cy = 922.0;
        cleanJump(m, w, 10, 912.0, 822.0, 911.0, 910.8);
        w.cx = kTownX;
        w.cy = kTownY + 140.0;
        const Answer a = propJump(m, w, 100, kTownX, kTownY, 0.0, 1.0);
        const std::string s = m.StatLine();
        check("target/cursor_town_live1", a == Answer::False && m.JumpDecision() == Decision::Granted
            && m.Stats().granted == 1 && m.Stats().refusedLanding == 0 && !m.HasCap() && has(s, " reach=89 jumps=")
            && has(s, " cap=none cursor=1074,1524@test last-target=cursor last-check=140 ")
            && asked(w, kTownX, kTownY + 140.0), s);
    }
    {
        World w;
        Mod m = makeCursor(w);
        outdoorWorld(w);
        m.SetEnabled(true);
        w.cx = 2131.5;
        w.cy = 3916.0;
        cleanJump(m, w, 10, kOutX, kOutY, 2129.8, 3909.7);
        w.cx = kOutCursorX;
        w.cy = kOutCursorY;
        const Answer a = propJump(m, w, 100, kOutX, kOutY, outUx(), outUy());
        const std::string s = m.StatLine();
        check("target/cursor_outdoor_live1", a == Answer::False && m.JumpDecision() == Decision::Granted
            && m.Stats().granted == 1 && !m.HasCap() && has(s, " reach=113 jumps=")
            && has(s, " last-target=cursor last-check=163 "), s);
    }
    {
        // Live 1's own town aim: the cursor 100 px south, inside the prop. A
        // jump there ends inside it, so it is refused under any rule.
        World w;
        Mod m = makeCursor(w);
        townWorld(w);
        m.SetEnabled(true);
        w.cx = 912.0;
        w.cy = 922.0;
        cleanJump(m, w, 10, 912.0, 822.0, 911.0, 910.8);
        w.cx = kTownX;
        w.cy = kTownY + 100.0;
        const Answer a = propJump(m, w, 100, kTownX, kTownY, 0.0, 1.0);
        const std::string s = m.StatLine();
        check("target/cursor_inside_the_prop_refused", a == Answer::Real
            && m.JumpDecision() == Decision::RefusedLanding && m.Stats().refusedLanding == 1 && m.Stats().granted == 0
            && has(s, " last-target=cursor last-check=100 "), s);
    }
    {
        // A clean jump of 175 px toward a cursor 300 px away sets the cap: a
        // cursor further out is checked at 175, a nearer one at its own distance.
        World w;
        Mod m = makeCursor(w);
        townWorld(w);
        m.SetEnabled(true);
        w.cx = 912.0;
        w.cy = 822.0 + 300.0;
        cleanJump(m, w, 10, 912.0, 822.0, 912.0, 822.0 + 175.0);
        const bool capped = m.HasCap() && near(m.Cap(), 175.0) && near(m.Reach(), 175.0);
        w.cx = kTownX;
        w.cy = kTownY + 300.0;
        const Answer far = propJump(m, w, 100, kTownX, kTownY, 0.0, 1.0);
        const std::string s1 = m.StatLine();
        const bool atCap = far == Answer::False && m.JumpDecision() == Decision::Granted
            && has(s1, " cap=175 ") && has(s1, " last-target=cap last-check=175 ") && asked(w, kTownX, kTownY + 175.0);
        land(m, w, 100, kTownX, kTownY + 175.0);
        w.cx = kTownX;
        w.cy = kTownY + 140.0;
        const Answer nearer = propJump(m, w, 200, kTownX, kTownY, 0.0, 1.0);
        const std::string s2 = m.StatLine();
        check("target/cursor_up_to_the_cap", capped && atCap && nearer == Answer::False && m.Stats().granted == 2
            && has(s2, " cap=175 ") && has(s2, " last-target=cursor last-check=140 "), s1 + " | " + s2);
    }

    // ---- the cap: learned only from a clean jump well short of its cursor -----
    {
        World w;
        Mod m = makeCursor(w);
        m.SetEnabled(true);
        w.cx = 1000.0;
        w.cy = 1120.0;                       // lands 20 px short of its cursor
        clearJump(m, w, 10, 100.0);
        const bool within = !m.HasCap() && near(m.Reach(), 100.0);
        w.cy = 1124.0;                       // 24 px short: still within the slack
        clearJump(m, w, 30, 100.0);
        const bool at24 = !m.HasCap();
        w.cy = 1125.0;                       // 25 px short: the cap
        clearJump(m, w, 50, 100.0);
        const bool set = m.HasCap() && near(m.Cap(), 100.0);
        // An answered jump teaches nothing: granted at the cap, answered, and
        // ending 250 px short of its cursor.
        w.cy = 1300.0;
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        land(m, w, 100, 1000.0, 1050.0);
        const bool answeredNothing = a == Answer::False && m.HasCap() && near(m.Cap(), 100.0) && near(m.Reach(), 100.0);
        // A clean jump longer than the cap clears it; it ends 10 px short of
        // its own cursor, so it sets none.
        w.cy = 1150.0;
        clearJump(m, w, 200, 140.0);
        check("cap/learned_only_short_of_its_cursor", within && at24 && set && answeredNothing && !m.HasCap()
            && near(m.Reach(), 140.0) && kCursorSlackPx == 24.0, m.StatLine());
    }

    // ---- the first jump after loading a character ------------------------------
    {
        // With a cursor, the first jump needs nothing learned.
        World w;
        Mod m = makeCursor(w);
        townWorld(w);
        m.SetEnabled(true);
        m.NotePlayer(261723);
        w.cx = kTownX;
        w.cy = kTownY + 140.0;
        const Answer a = propJump(m, w, 100, kTownX, kTownY, 0.0, 1.0);
        const std::string s = m.StatLine();
        check("first/lands_at_the_cursor_with_no_learned_reach", a == Answer::False
            && m.JumpDecision() == Decision::Granted && !m.HasReach() && m.Stats().refusedNoReach == 0
            && has(s, " reach=175 (start) jumps=") && has(s, " last-target=cursor last-check=140 "), s);
    }
    {
        // No cursor and nothing learned: the starting reach (175 px, slot 14's
        // open-ground jump) until a clean jump replaces it.
        World w;
        Mod m = make(w);
        townWorld(w);
        m.SetEnabled(true);
        const std::string fresh = m.StatLine();
        const Answer a = propJump(m, w, 100, kTownX, kTownY, 0.0, 1.0);
        const std::string s = m.StatLine();
        const bool start = a == Answer::False && m.JumpDecision() == Decision::Granted
            && m.Stats().refusedNoReach == 0 && asked(w, kTownX, kTownY + kStartReachPx)
            && has(fresh, " reach=175 (start) jumps=") && has(s, " cursor=unreadable last-target=start last-check=175 ");
        land(m, w, 100, kTownX, kTownY + kStartReachPx);
        const bool stillStart = !m.HasReach();
        cleanJump(m, w, 200, 912.0, 822.0, 911.0, 910.8);
        const std::string taught = m.StatLine();
        check("first/no_cursor_uses_the_starting_reach", start && stillStart && m.HasReach() && kStartReachPx == 175.0
            && has(taught, " reach=89 jumps="), s + " | " + taught);
    }
    {
        // A cursor provider that cannot read, or reads a non-number, is no
        // cursor: the starting reach decides.
        World w;
        Mod m = makeCursor(w);
        townWorld(w);
        m.SetEnabled(true);
        w.cursorReadable = false;
        const Answer unread = propJump(m, w, 100, kTownX, kTownY, 0.0, 1.0);
        const std::string s1 = m.StatLine();
        land(m, w, 100, kTownX, kTownY + kStartReachPx);
        w.cursorReadable = true;
        w.cx = std::nan("");
        w.cy = kTownY + 140.0;
        const Answer nan = propJump(m, w, 200, kTownX, kTownY, 0.0, 1.0);
        const std::string s2 = m.StatLine();
        check("first/unreadable_cursor_uses_the_starting_reach", unread == Answer::False && nan == Answer::False
            && m.Stats().granted == 2 && has(s1, " cursor=unreadable last-target=start last-check=175 ")
            && has(s2, " cursor=unreadable last-target=start last-check=175 "), s1 + " | " + s2);
    }

    // ---- the reach ---------------------------------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        const bool none = !m.HasReach();
        clearJump(m, w, 10, 100.0);
        check("reach/learned_from_clear_jump", none && m.HasReach() && near(m.Reach(), 100.0));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 0.0);
        const bool still = !m.HasReach();
        clearJump(m, w, 30, 20.0);
        const bool short20 = !m.HasReach();
        clearJump(m, w, 50, 31.9);
        check("reach/not_from_stationary_hop", still && short20 && !m.HasReach() && m.Stats().jumps == 3,
            counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 32.0);
        check("reach/thirty_two_px_is_enough", m.HasReach() && near(m.Reach(), 32.0));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        takeoff(m, w, 100);
        const Answer a = ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        land(m, w, 100, 1000.0, 1150.0);
        check("reach/not_from_answered_jump", a == Answer::False && m.Stats().answered == 1 && near(m.Reach(), 100.0),
            "reach=" + std::to_string(m.Reach()));
    }
    {
        // No reach yet: a take-off whose own frame saw a blocked family query
        // (refused at the starting reach) is not learned from, however far it
        // went. With a reach: a refused take-off with a blocked query does not
        // replace it.
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        w.solids.push_back(around(1000.0, 1000.0 + kStartReachPx, 4.0));
        takeoff(m, w, 100);
        ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        for (int64_t f = 101; f <= 105; ++f) m.OnLeapEntry(f, true);
        land(m, w, 105, 1000.0, 1120.0);
        const bool none = !m.HasReach() && m.Stats().refusedLanding == 1 && m.Stats().answered == 0;
        clearJump(m, w, 200, 100.0);
        w.solids.push_back(around(1000.0, 1100.0, 4.0));
        takeoff(m, w, 300);
        ask(m, Builtin::PlaceMeeting, 300, true, Fence, true);
        land(m, w, 300, 1000.0, 1150.0);
        check("reach/not_from_blocked_takeoff", none && near(m.Reach(), 100.0) && m.Stats().refusedLanding == 2,
            counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        ask(m, Builtin::CollisionCircle, 100, true, WallParent, true, false, 1000.0, 1005.0);
        takeoff(m, w, 100);
        for (int64_t f = 101; f <= 105; ++f) m.OnLeapEntry(f, true);
        land(m, w, 105, 1000.0, 1120.0);
        check("reach/not_from_blocked_before_open", !m.HasReach() && m.Stats().beforeOpen == 1, counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        m.NotePlayer(1);
        const bool kept = m.HasReach();
        m.NotePlayer(2);
        check("reach/kept_on_new_player", kept && m.HasReach() && near(m.Reach(), 100.0));
    }
    {
        // Live 1: Player_obj's instance id changes with the room (261723 in
        // Town_01_rm, 297089 in Act_01_01). The reach and the cap stay; only the
        // jump in progress is dropped.
        World w;
        Mod m = makeCursor(w);
        m.SetEnabled(true);
        m.NotePlayer(261723);
        w.cx = 1000.0;
        w.cy = 1300.0;
        clearJump(m, w, 10, 100.0);          // 200 px short of its cursor: cap 100
        const bool learned = m.HasReach() && m.HasCap();
        takeoff(m, w, 100);
        const bool active = m.JumpActive();
        m.NotePlayer(297089);
        const std::string s = m.StatLine();
        check("reach/survives_a_new_player_instance", learned && active && !m.JumpActive() && m.HasReach()
            && near(m.Reach(), 100.0) && m.HasCap() && near(m.Cap(), 100.0) && has(s, " reach=100 jumps=")
            && has(s, " cap=100 "), s);
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        m.SetEnabled(false);
        const bool offKept = m.HasReach();
        m.SetEnabled(true);
        check("reach/kept_across_off_on", offKept && m.HasReach() && near(m.Reach(), 100.0));
    }

    // ---- landed-inside= ----------------------------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        takeoff(m, w, 100);
        ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        land(m, w, 100, 1000.0, 1100.0);
        const bool clean = m.Stats().landedInside == 0 && m.Stats().granted == 1;
        w.solids.push_back(around(1000.0, 1130.0, 3.0));
        takeoff(m, w, 200);
        ask(m, Builtin::PlaceMeeting, 200, true, Fence, true);
        land(m, w, 200, 1000.0, 1130.0);
        const bool inside = m.Stats().landedInside == 1 && m.Stats().granted == 2;
        w.solids.push_back(around(1000.0, 1100.0, 4.0));
        takeoff(m, w, 300);
        ask(m, Builtin::PlaceMeeting, 300, true, Fence, true);
        land(m, w, 300, 1000.0, 1100.0);
        check("target/landed_inside", clean && inside && m.Stats().refusedLanding == 1 && m.Stats().landedInside == 1,
            counts(m.Stats()));
    }

    // ---- before-open= ------------------------------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        ask(m, Builtin::CollisionCircle, 100, true, WallParent, true);
        ask(m, Builtin::CollisionCircle, 100, true, WallParent, false);
        ask(m, Builtin::CollisionCircle, 100, false, WallParent, true);
        ask(m, Builtin::PlaceMeeting, 100, true, Skeleton, true);
        m.OnLeapEntry(100, true);
        const bool one = m.Stats().beforeOpen == 1;
        ask(m, Builtin::InstancePosition, 199, true, Fence, true);
        m.OnLeapEntry(200, true);
        const bool otherFrame = m.Stats().beforeOpen == 1;
        ask(m, Builtin::InstancePosition, 300, true, Fence, true);
        ask(m, Builtin::CollisionLine, 300, true, CollisionParent, true);
        m.OnLeapEntry(300, true);
        check("target/before_open", one && otherFrame && m.Stats().beforeOpen == 3 && m.Stats().jumps == 3,
            counts(m.Stats()));
    }

    // ---- walk-before-open= and walk-in-window= ------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        // A walk that ran before the window opened: the player's family
        // circles in the take-off frame count, blocked or not; one in an
        // earlier frame, another self's, another family's and a non-circle
        // query do not.
        circle(m, 99, 1000.0, 1005.0);
        circle(m, 100, 1000.0, 1005.0);
        circle(m, 100, 1000.0, 1009.0, true);
        ask(m, Builtin::CollisionCircle, 100, false, WallParent, true, false, 1000.0, 1013.0);
        ask(m, Builtin::CollisionCircle, 100, true, Skeleton, true, false, 1000.0, 1013.0);
        ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        m.OnLeapEntry(100, true);
        const bool before = m.Stats().walkBeforeOpen == 2 && m.Stats().beforeOpen == 2 && m.Stats().walkInWindow == 0;
        for (int64_t f = 101; f <= 103; ++f) m.OnLeapEntry(f, true);
        land(m, w, 103, 1000.0, 1100.0);
        const bool noneInside = m.Stats().walkInWindow == 0;
        // A walk inside the window: two take-off-frame circles, counted once
        // the window closes.
        takeoff(m, w, 200);
        const bool notYet = m.Stats().walkInWindow == 0;
        land(m, w, 200, 1000.0, 1100.0);
        const bool inside = m.Stats().walkInWindow == 1 && m.Stats().walkBeforeOpen == 2;
        // One take-off-frame circle, then circles a frame later: no walk.
        m.OnLeapEntry(300, true);
        circle(m, 300, 1000.0, 1005.0);
        m.OnLeapEntry(301, true);
        circle(m, 301, 1000.0, 1009.0);
        circle(m, 301, 1000.0, 1013.0);
        land(m, w, 301, 1000.0, 1100.0);
        check("target/walk_before_open_and_in_window", before && noneInside && notYet && inside
            && m.Stats().walkInWindow == 1 && m.Stats().walkBeforeOpen == 2 && m.Stats().jumps == 3,
            counts(m.Stats()));
    }

    // ---- the family through its ancestry -----------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        const long beforeFirst = g_AncestorCalls;
        ask(m, Builtin::InstancePosition, 5, true, InvisibleWall, false);
        const bool askedAncestry = g_AncestorCalls > beforeFirst;
        clearJump(m, w, 10, 100.0);
        takeoff(m, w, 100);
        const Answer wall = ask(m, Builtin::CollisionCircle, 100, true, WallParent, true, false, 1000.0, 1013.0);
        const Answer invisible = ask(m, Builtin::InstancePosition, 100, true, InvisibleWall, true);
        check("target/wall_parent_in_family", askedAncestry && wall == Answer::Noone && invisible == Answer::Noone,
            text(wall) + " " + text(invisible));
        ask(m, Builtin::PlaceMeeting, 100, true, Skeleton, true);
        ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        const long seen = g_AncestorCalls;
        for (int k = 0; k < 100; ++k) {
            ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
            ask(m, Builtin::PlaceMeeting, 100, true, Skeleton, true);
            ask(m, Builtin::CollisionCircle, 100, true, WallParent, false);
            ask(m, Builtin::InstancePosition, 100, true, InvisibleWall, true);
        }
        check("target/family_table_built_once_per_object", g_AncestorCalls == seen,
            "calls=" + std::to_string(g_AncestorCalls - seen));
    }

    // ---- gates and locks keep blocking --------------------------------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        takeoff(m, w, 100);
        const Answer granted = ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        const Answer parent = ask(m, Builtin::PlaceMeeting, 100, true, CollisionParent, true, true);
        const Answer wall = ask(m, Builtin::CollisionCircle, 100, true, WallParent, true, true, 1000.0, 1013.0);
        const bool counted = m.Stats().excluded == 2 && m.Stats().answered == 1;
        const Answer clear = ask(m, Builtin::CollisionCircle, 100, true, WallParent, true, false, 1000.0, 1013.0);
        check("excluded/blocked_by_gate_or_lock", granted == Answer::False && parent == Answer::Real
            && wall == Answer::Real && counted && clear == Answer::Noone && m.Stats().answered == 2, counts(m.Stats()));

        const long asks = g_ExcludedAsks;
        const uint64_t excluded = m.Stats().excluded;
        bool real = true;
        for (int object : { (int)GateParent, (int)GateChild, (int)LockObj, (int)LockChild })
            real = real && ask(m, Builtin::InstancePosition, 100, true, object, true) == Answer::Real;
        const bool inGranted = real && m.Stats().excluded == excluded + 4 && g_ExcludedAsks == asks;
        land(m, w, 100, 1000.0, 1100.0);
        takeoff(m, w, 200);
        const size_t place = w.placeAsked.size();
        const Answer undecided = ask(m, Builtin::InstancePosition, 200, true, GateParent, true);
        check("excluded/query_naming_gate_or_lock", inGranted && undecided == Answer::Real
            && m.JumpDecision() == Decision::Undecided && w.placeAsked.size() == place
            && m.Stats().excluded == excluded + 4 && m.Stats().granted == 1, counts(m.Stats()));
    }
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        const long start = g_ExcludedAsks;
        w.solids.push_back(around(1000.0, 1000.0 + kStartReachPx, 4.0));
        takeoff(m, w, 50);                                                    // no reach yet: refused at the starting reach
        ask(m, Builtin::PlaceMeeting, 50, true, Fence, true);
        ask(m, Builtin::PlaceMeeting, 50, true, Fence, true);
        land(m, w, 50, 1000.0, 1000.0);
        clearJump(m, w, 60, 100.0);
        ask(m, Builtin::PlaceMeeting, 90, true, Fence, true);                 // outside the window
        w.solids.push_back(around(1000.0, 1100.0, 4.0));
        takeoff(m, w, 100);                                                   // refused landing
        ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        land(m, w, 100, 1000.0, 1000.0);
        w.solids.clear();
        takeoff(m, w, 200);                                                   // granted
        ask(m, Builtin::PlaceMeeting, 200, true, Fence, false);               // free
        const long beforeGranted = g_ExcludedAsks;
        ask(m, Builtin::PlaceMeeting, 200, true, Fence, true);                // blocked: asked once
        check("excluded/asked_only_for_blocked_queries_in_a_granted_window",
            beforeGranted == start && g_ExcludedAsks == start + 1 && m.Stats().granted == 1, counts(m.Stats()));
    }

    // ---- the install: armed at launch, hooked once a character exists --------
    // The adapter feeds the core g_Setup, whether HhResolveLocalPlayer found
    // the local player, and whether the try comes from the tick; it reports
    // what JumpSceneryInstall returned with NoteInstall. The tick looks for the
    // player only on the frames LooksForPlayer picks while the switch is on.
    {
        // Baseline: `jumpscenery 1` in game, with the player there, installs at
        // once, as before. Off and on again keeps the one install.
        World w;
        Mod m = make(w);
        const std::string never = std::string(m.InstallStateName());
        m.SetEnabled(true);
        const bool now = m.ShouldInstall(true, true, false);
        m.NoteInstall(true, false);
        const bool on = m.Enabled() && m.Installed();
        m.SetEnabled(false);
        const std::string offAfter = std::string(m.InstallStateName());
        m.SetEnabled(true);
        check("baseline/install_is_immediate_with_a_character",
              never == "not-armed" && now && on && offAfter == "installed" && !m.ShouldInstall(true, true, false)
                  && !m.ShouldInstall(true, true, true) && std::string(m.InstallStateName()) == "installed",
              "never=" + never + " now=" + std::to_string(now) + " off=" + offAfter);
    }
    {
        // Target: `jumpscenery 1` from the launch commands at character select.
        // The switch is on, nothing installs; the tick looks every
        // kInstallPollFrames frames and installs nothing until the player
        // resolves, then installs once.
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        const bool atSelect = m.ShouldInstall(true, false, false);
        const std::string armed = std::string(m.InstallStateName());
        int looks = 0;
        bool installed = false;
        for (unsigned long long frame = 0; frame < 600; ++frame) {
            if (!Mod::LooksForPlayer(frame)) continue;
            ++looks;
            if (m.ShouldInstall(true, false, true)) installed = true;
        }
        const bool beforeSetup = m.ShouldInstall(false, true, true);
        const std::string waiting = std::string(m.InstallStateName());
        const bool now = m.ShouldInstall(true, true, true);
        if (now) m.NoteInstall(true, true);
        check("target/waits_for_a_character_before_installing",
              Mod::kInstallPollFrames == 60 && looks == 10 && !atSelect && !installed && !beforeSetup
                  && m.Enabled() && armed == "waiting-for-character" && waiting == "waiting-for-character" && now
                  && m.Installed() && !m.ShouldInstall(true, true, true) && !m.ShouldInstall(true, true, false)
                  && std::string(m.InstallStateName()) == "installed",
              "looks=" + std::to_string(looks) + " armed=" + armed + " waiting=" + waiting + " now=" + std::to_string(now));
    }
    {
        // A refusal, from the tick or the command, turns the switch off and
        // says so. The tick tries once per session; `jumpscenery 1` in game may
        // try again, and a success then clears the refusal.
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        m.NoteInstall(false, true);
        const bool offAfterTick = !m.Enabled() && !m.Installed();
        const std::string refused = std::string(m.InstallStateName());
        const bool offAsks = m.ShouldInstall(true, true, false);
        m.SetEnabled(true);
        const bool tickAgain = m.ShouldInstall(true, true, true);
        const bool commandAgain = m.ShouldInstall(true, true, false);
        m.NoteInstall(false, false);
        const bool offAfterCommand = !m.Enabled() && std::string(m.InstallStateName()) == "refused";
        m.SetEnabled(true);
        m.NoteInstall(true, false);
        check("target/a_refusal_turns_the_switch_off_and_the_tick_tries_once",
              offAfterTick && refused == "refused" && !offAsks && !tickAgain && commandAgain && offAfterCommand
                  && m.Enabled() && m.Installed() && std::string(m.InstallStateName()) == "installed",
              "refused=" + refused + " tickAgain=" + std::to_string(tickAgain)
                  + " commandAgain=" + std::to_string(commandAgain));
    }
    {
        // Negative control: off, nothing installs whatever else holds, and the
        // state says it was never armed.
        World w;
        Mod m = make(w);
        bool any = false;
        for (int bits = 0; bits < 8; ++bits)
            if (m.ShouldInstall((bits & 1) != 0, (bits & 2) != 0, (bits & 4) != 0)) any = true;
        m.SetEnabled(true);
        m.SetEnabled(false);
        check("target/switched_off_never_installs_the_hooks",
              !any && !m.ShouldInstall(true, true, false) && !m.Installed()
                  && std::string(m.InstallStateName()) == "not-armed",
              std::string(m.InstallStateName()));
    }

    // ---- the stat line ------------------------------------------------------
    {
        World w;
        Mod m = make(w);
        const std::string off = m.StatusLine();
        m.SetEnabled(true);
        check("stat/on_off_lines", off == "jumpscenery: off" && m.StatusLine() == "jumpscenery: on",
            off + " | " + m.StatusLine());
        const std::string fresh = m.StatLine();
        clearJump(m, w, 10, 100.0);
        takeoff(m, w, 100);
        ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        ask(m, Builtin::InstancePosition, 100, true, Fence, true);
        const std::string after = m.StatLine();
        check("stat/line_names_every_counter",
            fresh == "jumpscenery: on reach=175 (start) jumps=0 granted=0 answered=0 refused-landing=0 refused-room=0 "
                     "refused-no-reach=0 no-direction=0 landed-inside=0 before-open=0 walk-before-open=0 "
                     "walk-in-window=0 excluded=0 cap=none cursor=unreadable last-target=none last-check=none "
                     "room=2000x2000"
                && after == "jumpscenery: on reach=100 jumps=2 granted=1 answered=2 refused-landing=0 refused-room=0 "
                            "refused-no-reach=0 no-direction=0 landed-inside=0 before-open=0 walk-before-open=0 "
                            "walk-in-window=1 excluded=0 cap=none cursor=unreadable last-target=reach last-check=100 "
                            "room=2000x2000",
            fresh + " | " + after);
        w.roomReadable = false;
        m.SetEnabled(false);
        const std::string unknown = m.StatLine();
        check("stat/room_unknown", unknown.rfind("jumpscenery: off reach=100 ", 0) == 0
            && unknown.size() > 13 && unknown.substr(unknown.size() - 13) == " room=unknown", unknown);
    }
    {
        // cap=, cursor= (read when the line is built, with the route that
        // answered), last-target= and last-check=.
        World w;
        Mod m = makeCursor(w);
        m.SetEnabled(true);
        w.cx = 1000.0;
        w.cy = 1300.0;
        clearJump(m, w, 10, 100.0);          // cap 100
        takeoff(m, w, 100);
        ask(m, Builtin::PlaceMeeting, 100, true, Fence, true);
        w.cx = 1234.4;
        w.cy = 987.6;
        const std::string s = m.StatLine();
        w.cursorReadable = false;
        const std::string u = m.StatLine();
        check("stat/cursor_cap_and_last_check_fields",
            has(s, " excluded=0 cap=100 cursor=1234,988@test last-target=cap last-check=100 room=2000x2000")
                && has(u, " cap=100 cursor=unreadable last-target=cap "), s + " | " + u);
    }

    // ---- an unresolved family or exclusion answers for nothing ---------------
    {
        World w;
        Mod m = make(w);
        m.SetEnabled(true);
        clearJump(m, w, 10, 100.0);
        m.SetFamily(-1, { GateParent, LockObj });
        takeoff(m, w, 100);
        const bool noFamily = ask(m, Builtin::PlaceMeeting, 100, true, Fence, true) == Answer::Real
            && ask(m, Builtin::PlaceMeeting, 100, true, -1, true) == Answer::Real;
        land(m, w, 100, 1000.0, 1100.0);
        m.SetFamily(CollisionParent, { -1, LockObj });
        takeoff(m, w, 200);
        const bool noExclusion = ask(m, Builtin::PlaceMeeting, 200, true, Fence, true) == Answer::Real
            && ask(m, Builtin::PlaceMeeting, 200, true, CollisionParent, true) == Answer::Real;
        land(m, w, 200, 1000.0, 1100.0);
        m.SetFamily(CollisionParent, { GateParent, LockObj });
        takeoff(m, w, 300);
        const bool restored = ask(m, Builtin::PlaceMeeting, 300, true, Fence, true) == Answer::False;
        check("failclosed/unresolved_family_or_exclusion", noFamily && noExclusion && restored
            && m.Stats().granted == 1 && m.Stats().answered == 1, counts(m.Stats()));
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
