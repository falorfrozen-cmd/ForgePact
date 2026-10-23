// Behavioral regression harness for the Mark 1 set's jump core
// (Mark1Jump.hpp, ForgePact #166).
//
// The Python runner injects the REAL ForgePact::Mark1Jump header below. No
// game is touched: the controlled world is a player position, a room size, a
// mouse position in room coordinates, a list of solid boxes the original
// place_meeting against the family answers from, a path grid with its own
// blocked boxes (or no grid at all), and gate or lock boxes that the original
// collision_line answers from. A jump is a run of skillsLeap calls, one a
// frame, each moving the player a fixed step toward the cursor the way the
// game's own jump does; whatever OnLeapExit asks to write is written.
//
// Baseline: with the bonus inactive nothing is read, decided or rewritten;
// another self's jump is not the player's; no cursor, or a cursor within the
// near-zero threshold, is a vanilla jump. Target: the landing is always 3M
// along the cursor's direction, for a cursor inside M (100 px still lands at
// 3M), between M and 3M and beyond 3M, with f = L / min(d, M) and M from the
// cap, the reach or the start value. walkable: the landing and the points
// 16 px either side need room, no family and a free grid cell (no grid counts
// free), and a gate or lock on the segment refuses. back_off: a refused 3M
// point backs off in 16 px steps to the farthest candidate past M that
// passes, never one whose gate check did not run, and with none past M the
// jump is vanilla; backed-off= is counted apart from the refusals.
// refused_unreadable: every read the decision makes (take-off, cursor, room,
// place_meeting, grid, gate line) failing by being unbound, throwing, false
// or non-finite is counted under its kind, never passes a candidate, and a
// jump ending vanilla on it is refused-unreadable; refusals on successful
// reads are counted by reason. The clamp at L, the natural length handed to
// the learner, the window, and the granted jump's answers.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// PRODUCTION_MARK1JUMP

using namespace ForgePact::Mark1Jump;

// ---- the controlled world -------------------------------------------------
struct Box { double x0, y0, x1, y1; };

static bool inside(const std::vector<Box>& boxes, double x, double y)
{
    for (const Box& b : boxes)
        if (x >= b.x0 && x <= b.x1 && y >= b.y0 && y <= b.y1) return true;
    return false;
}

static Box around(double x, double y, double half) { return Box{ x - half, y - half, x + half, y + half }; }

struct World {
    double px = 1000.0, py = 1000.0;
    bool posReadable = true;
    double roomW = 4000.0, roomH = 4000.0;
    bool roomReadable = true;
    double cx = 1000.0, cy = 1300.0;
    bool cursorReadable = true;
    Max max;                          // what jumpscenery's core would give
    std::vector<Box> solids;          // the Collision_Parent_obj family
    bool hasGrid = true;
    bool gridThrows = false;
    std::vector<Box> gridBlocked;
    std::vector<Box> gates;           // Gate_Parent_obj and Lock_obj
    bool gateThrows = false;
    bool placeThrows = false;
    // Failed reads the real adapter can meet: a read that throws, and the
    // grid's own Unreadable answer, everywhere or only at some points.
    bool posThrows = false, cursorThrows = false, roomThrows = false;
    bool gridUnreadable = false;
    std::vector<Box> placeThrowAt;    // place_meeting throws at these points
    std::vector<Box> gridUnreadAt;    // the grid cell reads Unreadable here
    double gateThrowBeyondY = std::numeric_limits<double>::infinity();   // collision_line throws past this end
    long posReads = 0, cursorReads = 0, roomReads = 0, placeAsks = 0, gridAsks = 0, maxAsks = 0;
    std::vector<std::pair<double, double>> placeAsked;
    std::vector<std::pair<double, double>> gateAskedEnds;
    std::vector<Learned> learned;
    long writes = 0;

    // The segment crosses a gate box: sampled every half pixel.
    bool gateOnLine(double x0, double y0, double x1, double y1) const
    {
        const double len = std::hypot(x1 - x0, y1 - y0);
        const int n = static_cast<int>(len * 2.0) + 1;
        for (int k = 0; k <= n; ++k) {
            const double t = static_cast<double>(k) / n;
            if (inside(gates, x0 + t * (x1 - x0), y0 + t * (y1 - y0))) return true;
        }
        return false;
    }
};

// Which callbacks make() leaves unwired.
enum Skip : unsigned { NoGate = 1, NoGrid = 2, NoPlace = 4, NoLearn = 8, NoCursor = 16, NoPosition = 32, NoRoom = 64 };

static Core make(World& w, unsigned skip = 0)
{
    Core c;
    if (!(skip & NoPosition))
        c.SetPlayerPosition([&w](double& x, double& y) {
            ++w.posReads;
            if (w.posThrows) throw std::runtime_error("position");
            if (!w.posReadable) return false;
            x = w.px;
            y = w.py;
            return true;
        });
    if (!(skip & NoCursor))
        c.SetCursor([&w](double& x, double& y) {
            ++w.cursorReads;
            if (w.cursorThrows) throw std::runtime_error("cursor");
            if (!w.cursorReadable) return false;
            x = w.cx;
            y = w.cy;
            return true;
        });
    c.SetMax([&w]() { ++w.maxAsks; return w.max; });
    if (!(skip & NoRoom))
        c.SetRoomSize([&w](double& rw, double& rh) {
            ++w.roomReads;
            if (w.roomThrows) throw std::runtime_error("room");
            if (!w.roomReadable) return false;
            rw = w.roomW;
            rh = w.roomH;
            return true;
        });
    if (!(skip & NoPlace))
        c.SetPlaceMeeting([&w](double x, double y) {
            ++w.placeAsks;
            w.placeAsked.emplace_back(x, y);
            if (w.placeThrows || inside(w.placeThrowAt, x, y)) throw std::runtime_error("place_meeting");
            return inside(w.solids, x, y);
        });
    if (!(skip & NoGrid))
        c.SetGrid([&w](double x, double y) {
            ++w.gridAsks;
            if (w.gridThrows) throw std::runtime_error("grid");
            if (w.gridUnreadable || inside(w.gridUnreadAt, x, y)) return Cell::Unreadable;
            if (!w.hasGrid) return Cell::NoGrid;
            return inside(w.gridBlocked, x, y) ? Cell::Blocked : Cell::Free;
        });
    if (!(skip & NoGate))
        c.SetGateOnLine([&w](double x0, double y0, double x1, double y1) {
            w.gateAskedEnds.emplace_back(x1, y1);
            if (w.gateThrows || y1 > w.gateThrowBeyondY) throw std::runtime_error("collision_line");
            return w.gateOnLine(x0, y0, x1, y1);
        });
    if (!(skip & NoLearn))
        c.SetLearner([&w](const Learned& l) { w.learned.push_back(l); });
    return c;
}

// The game's own jump from the player's position: `calls` skillsLeap calls on
// frames f .. f + calls - 1, each moving the player natural / calls along
// (ux, uy), with whatever the core asks written back; the tick at
// f + calls + 1 closes the window. Returns the core's write count.
static long fly(Core& c, World& w, int64_t f, double ux, double uy, double natural, int calls = 6, bool player = true)
{
    long writes = 0;
    const double step = natural / calls;
    for (int k = 0; k < calls; ++k) {
        c.OnLeapEntry(f + k, player);
        w.px += step * ux;
        w.py += step * uy;
        const Move m = c.OnLeapExit(f + k, player);
        if (m.write) {
            ++writes;
            w.px = m.x;
            w.py = m.y;
        }
    }
    c.Tick(f + calls + 1);
    w.writes += writes;
    return writes;
}

// A jump toward the world's cursor whose natural length is min(d, trueMax):
// the vanilla jump ends at a cursor inside its maximum.
static long jumpToCursor(Core& c, World& w, int64_t f, double trueMax, int calls = 6)
{
    const double dx = w.cx - w.px, dy = w.cy - w.py;
    const double d = std::hypot(dx, dy);
    if (d <= 0.0) return fly(c, w, f, 0.0, 1.0, 0.0, calls);
    return fly(c, w, f, dx / d, dy / d, std::min(d, trueMax), calls);
}

static void at(World& w, double x, double y) { w.px = x; w.py = y; }

static bool near(double a, double b, double eps = 1e-6) { return std::fabs(a - b) <= eps; }

static double from(const World& w, double x, double y) { return std::hypot(w.px - x, w.py - y); }

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string counts(const Counters& c)
{
    return "jumps=" + std::to_string(c.jumps) + " granted=" + std::to_string(c.granted)
        + " backed-off=" + std::to_string(c.backedOff) + " refused-cursor=" + std::to_string(c.refusedCursor)
        + " refused-landing=" + std::to_string(c.refusedLanding) + " refused-gate=" + std::to_string(c.refusedGate)
        + " scaled=" + std::to_string(c.scaled) + " clamped=" + std::to_string(c.clamped)
        + " answered=" + std::to_string(c.answered) + " excluded=" + std::to_string(c.excluded)
        + " refused-unreadable=" + std::to_string(c.refusedUnreadable)
        + " rejected=" + std::to_string(c.rejectedRoom) + "/" + std::to_string(c.rejectedFamily) + "/"
        + std::to_string(c.rejectedGrid) + "/" + std::to_string(c.rejectedGate)
        + " unread=" + std::to_string(c.unreadTakeoff) + "/" + std::to_string(c.unreadCursor) + "/"
        + std::to_string(c.unreadRoom) + "/" + std::to_string(c.unreadFamily) + "/" + std::to_string(c.unreadGrid) + "/"
        + std::to_string(c.unreadGate)
        + " grid-reads=" + std::to_string(c.gridFree) + "/" + std::to_string(c.gridBlocked) + "/"
        + std::to_string(c.gridNoGrid);
}

// No failed read of any kind was counted.
static bool noUnread(const Counters& c)
{
    return c.refusedUnreadable == 0 && c.unreadTakeoff == 0 && c.unreadCursor == 0 && c.unreadRoom == 0
        && c.unreadFamily == 0 && c.unreadGrid == 0 && c.unreadGate == 0;
}

// The jump ended vanilla on a failed read: refused-unreadable, never one of
// the refusals that mean the game answered, and nothing granted.
static bool refusedUnreadableOnly(const Core& c)
{
    const Counters& k = c.Stats();
    return c.LastDecision() == Decision::RefusedUnreadable && k.refusedUnreadable == 1 && k.granted == 0
        && k.refusedLanding == 0 && k.refusedGate == 0 && k.refusedCursor == 0;
}

static std::string pos(const World& w) { return "pos=" + std::to_string(w.px) + "," + std::to_string(w.py); }

static bool noRefusals(const Counters& c) { return c.refusedCursor == 0 && c.refusedLanding == 0 && c.refusedGate == 0; }

static Answer ask(Core& c, int64_t frame, bool player, bool family, bool excluded, bool blocked, bool gateThere = false,
    Builtin b = Builtin::PlaceMeeting)
{
    return c.OnQuery(b, frame, player, family, excluded, blocked, [gateThere]() { return gateThere; });
}

// fly() along (ux, uy) for the player, calling `between(frame, k)` after each
// skillsLeap entry and before its call moves the player: the queries the
// game's jump asks in that frame.
template <class BetweenFn>
static void flyAsking(Core& c, World& w, int64_t f, double ux, double uy, double natural, BetweenFn&& between,
    int calls = 6)
{
    const double step = natural / calls;
    for (int k = 0; k < calls; ++k) {
        c.OnLeapEntry(f + k, true);
        between(f + k, k);
        w.px += step * ux;
        w.py += step * uy;
        const Move m = c.OnLeapExit(f + k, true);
        if (m.write) {
            w.px = m.x;
            w.py = m.y;
        }
    }
    c.Tick(f + calls + 1);
}

// ---- the object table the classifier asks (object_is_ancestor's answers) ----
// Numbered as the game numbers them where the guide names them; an object is
// not its own ancestor.
namespace objs {
constexpr int AvoidableParent = 1, CollisionParent = 957, CollisionProp = 959, Fence = 5001, WallParent = 5701,
              InvisibleWall = 2269, GateParent = 1881, GateChild = 5003, LockObj = 2509, LockChild = 5004,
              EnemyParent = 301, Skeleton = 5005;
}
static long g_AncestorCalls = 0;
static bool objectIsAncestor(int object, int ancestor)
{
    using namespace objs;
    static const std::pair<int, int> kParent[] = {
        { CollisionParent, AvoidableParent }, { CollisionProp, CollisionParent }, { Fence, CollisionProp },
        { WallParent, CollisionParent }, { InvisibleWall, WallParent }, { GateParent, WallParent },
        { GateChild, GateParent }, { LockObj, CollisionParent }, { LockChild, LockObj }, { Skeleton, EnemyParent },
    };
    ++g_AncestorCalls;
    for (int depth = 0; depth < 64; ++depth) {
        int parent = -1;
        for (const auto& p : kParent)
            if (p.first == object) parent = p.second;
        if (parent < 0) return false;
        if (parent == ancestor) return true;
        object = parent;
    }
    return false;
}

static Classifier makeClassifier()
{
    Classifier k;
    k.SetIsAncestor(objectIsAncestor);
    k.SetObjects(objs::CollisionParent, objs::GateParent, objs::LockObj);
    return k;
}

int main()
{
    // ---- the named constants ------------------------------------------------
    check("table/named_constants", kScale == 3.0 && kStepPx == 16.0 && kBandPx == 16.0 && kMinCursorPx == 16.0
        && kWindowFrames == 2 && kStartMaxPx == 175.0 && kMinMaxPx == 32.0
        && MaxSourceName(MaxSource::Cap) == "cap" && MaxSourceName(MaxSource::Reach) == "reach"
        && MaxSourceName(MaxSource::Start) == "start" && DecisionName(Decision::BackedOff) == "backed-off"
        && DecisionName(Decision::RefusedGate) == "refused-gate" && DecisionName(Decision::RefusedCursor) == "refused-cursor"
        && DecisionName(Decision::RefusedLanding) == "refused-landing" && LandingName(Landing::Grid) == "grid"
        && DecisionName(Decision::RefusedUnreadable) == "refused-unreadable" && LandingName(Landing::Unread) == "unread"
        && ReadKindName(ReadKind::Takeoff) == "takeoff" && ReadKindName(ReadKind::Cursor) == "cursor"
        && ReadKindName(ReadKind::Room) == "room" && ReadKindName(ReadKind::Family) == "family"
        && ReadKindName(ReadKind::Grid) == "grid" && ReadKindName(ReadKind::Gate) == "gate" && kReadKinds == 6);

    // ---- baseline: what the game does without the bonus -------------------
    {
        World w;
        Core c = make(w);
        const long writes = jumpToCursor(c, w, 10, 175.0);
        check("baseline/inactive_no_decision", c.Stats().jumps == 0 && c.LastDecision() == Decision::None
            && w.cursorReads == 0 && w.placeAsks == 0 && w.maxAsks == 0 && w.posReads == 0 && !c.JumpActive(),
            counts(c.Stats()));
        check("baseline/inactive_no_scaling", writes == 0 && near(w.px, 1000.0) && near(w.py, 1175.0)
            && w.learned.empty(), pos(w));
        check("baseline/inactive_answers_real", ask(c, 11, true, true, false, true) == Answer::Real
            && c.Stats().answered == 0);
    }
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        const long writes = jumpToCursor(c, w, 10, 175.0, 6);
        at(w, 1000.0, 1000.0);
        fly(c, w, 30, 0.0, 1.0, 175.0, 6, false);
        check("baseline/other_self_is_not_the_players_jump", c.Stats().jumps == 1 && writes > 0
            && near(w.py, 1175.0) && w.learned.size() == 1, counts(c.Stats()) + " " + pos(w));
    }
    {
        World w;
        w.cursorReadable = false;
        Core c = make(w);
        c.SetActive(true);
        const long writes = fly(c, w, 10, 0.0, 1.0, 175.0);
        // A cursor that could not be read is a failed read, not a cursor too
        // close: still a vanilla jump, counted as refused-unreadable.
        check("baseline/no_cursor_is_vanilla", refusedUnreadableOnly(c) && c.Stats().unreadCursor == 1
            && writes == 0 && near(w.py, 1175.0) && w.placeAsks == 0, counts(c.Stats()) + " " + pos(w));
    }
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        w.cx = 1000.0;
        w.cy = 1010.0;   // 10 px: inside the near-zero threshold
        const long a = jumpToCursor(c, w, 10, 175.0);
        at(w, 1000.0, 1000.0);
        w.cx = 1000.0;
        w.cy = 1000.0;   // on the take-off
        const long b = fly(c, w, 20, 0.0, 1.0, 0.0);
        at(w, 1000.0, 1000.0);
        w.cx = std::nan("");
        const long nanWrites = fly(c, w, 30, 0.0, 1.0, 50.0);
        // Two readable cursors too close: refused-cursor. The non-finite one
        // is a failed read: refused-unreadable, a vanilla jump all the same.
        check("baseline/near_cursor_is_vanilla", a == 0 && b == 0 && nanWrites == 0 && c.Stats().refusedCursor == 2
            && c.Stats().refusedUnreadable == 1 && c.Stats().unreadCursor == 1
            && c.Stats().granted == 0 && w.placeAsks == 0, counts(c.Stats()));
    }
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        w.cy = 1016.0;   // exactly the threshold: a direction, so decided
        jumpToCursor(c, w, 10, 175.0);
        check("baseline/cursor_at_the_threshold_is_decided", c.LastDecision() == Decision::Granted
            && near(c.LastFactor(), 525.0 / 16.0) && near(w.py, 1525.0), counts(c.Stats()) + " " + pos(w));
    }
    {
        World w;
        w.solids = { Box{ 0.0, 1100.0, 4000.0, 3999.0 } };   // nothing past M walkable
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        const bool real = ask(c, 10, true, true, false, true) == Answer::Real;
        check("baseline/refused_jump_keeps_real_answers", real && c.LastDecision() == Decision::RefusedLanding
            && c.Stats().answered == 0, counts(c.Stats()));
    }

    // ---- target: the landing is always 3M toward the cursor -----------------
    {
        World w;
        w.cy = 1100.0;   // 100 px, well inside M = 175
        Core c = make(w);
        c.SetActive(true);
        const long writes = jumpToCursor(c, w, 10, 175.0);
        check("target/cursor_near_full_distance", c.LastDecision() == Decision::Granted
            && near(c.LastLanding(), 525.0) && near(c.LastFactor(), 5.25) && near(w.px, 1000.0)
            && near(w.py, 1525.0, 1e-6) && writes == 6, counts(c.Stats()) + " " + pos(w));
    }
    {
        World w;
        w.cy = 1300.0;   // 300 px: between M and 3M
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        check("target/cursor_between_m_and_3m", c.LastDecision() == Decision::Granted
            && near(c.LastLanding(), 525.0) && near(c.LastFactor(), 3.0) && near(w.py, 1525.0, 1e-6),
            counts(c.Stats()) + " " + pos(w));
    }
    {
        World w;
        w.cy = 1900.0;   // 900 px: beyond 3M
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        check("target/cursor_beyond_3m", c.LastDecision() == Decision::Granted && near(c.LastLanding(), 525.0)
            && near(c.LastFactor(), 3.0) && near(w.py, 1525.0, 1e-6), counts(c.Stats()) + " " + pos(w));
    }
    {
        World w;
        w.cx = 1300.0;
        w.cy = 1400.0;   // 500 px along (0.6, 0.8)
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        check("target/direction_follows_the_cursor", near(w.px, 1000.0 + 0.6 * 525.0, 1e-6)
            && near(w.py, 1000.0 + 0.8 * 525.0, 1e-6), pos(w));
    }
    {
        const Max cap = ChooseMax(true, 150.0, true, 120.0);
        const Max reach = ChooseMax(false, 0.0, true, 120.0);
        const Max start = ChooseMax(false, 0.0, false, 0.0);
        const Max shortCap = ChooseMax(true, 10.0, true, 120.0);
        const Max nanReach = ChooseMax(false, 0.0, true, std::nan(""));
        check("target/max_from_cap_then_reach_then_start", cap.source == MaxSource::Cap && cap.value == 150.0
            && reach.source == MaxSource::Reach && reach.value == 120.0 && start.source == MaxSource::Start
            && start.value == 175.0 && shortCap.source == MaxSource::Reach && nanReach.source == MaxSource::Start);
    }
    {
        // The factor for each cursor distance, with M from each source.
        struct Case { Max max; double cy; double landing; double factor; };
        const Case cases[] = {
            { Max{ 150.0, MaxSource::Cap }, 1100.0, 450.0, 4.5 },
            { Max{ 150.0, MaxSource::Cap }, 1300.0, 450.0, 3.0 },
            { Max{ 150.0, MaxSource::Cap }, 1900.0, 450.0, 3.0 },
            { Max{ 120.0, MaxSource::Reach }, 1060.0, 360.0, 6.0 },
            { Max{ 120.0, MaxSource::Reach }, 1200.0, 360.0, 3.0 },
            { Max{ 120.0, MaxSource::Reach }, 1500.0, 360.0, 3.0 },
            { Max{}, 1050.0, 525.0, 10.5 },
            { Max{}, 1175.0, 525.0, 3.0 },
            { Max{}, 1700.0, 525.0, 3.0 },
        };
        bool ok = true;
        std::string detail;
        int64_t f = 10;
        for (const Case& k : cases) {
            World w;
            w.max = k.max;
            w.cy = k.cy;
            Core c = make(w);
            c.SetActive(true);
            jumpToCursor(c, w, f, k.max.value);
            const bool one = c.LastDecision() == Decision::Granted && c.LastMax().source == k.max.source
                && near(c.LastMax().value, k.max.value) && near(c.LastLanding(), k.landing)
                && near(c.LastFactor(), k.factor) && near(w.py - 1000.0, k.landing, 1e-6);
            if (!one) detail += " [" + std::string(MaxSourceName(k.max.source)) + " cy=" + std::to_string(k.cy)
                + " f=" + std::to_string(c.LastFactor()) + " " + pos(w) + "]";
            ok = ok && one;
            f += 100;
        }
        check("target/factor_for_each_cursor_and_max_source", ok, detail);
    }
    {
        World w;
        w.max = Max{ 5.0, MaxSource::Cap };   // a value that is no maximum
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        check("target/an_implausible_max_falls_back_to_start", c.LastMax().source == MaxSource::Start
            && near(c.LastLanding(), 525.0), counts(c.Stats()));
    }

    // ---- walkable: the landing test --------------------------------------
    {
        World w;
        Core c = make(w);
        const Landing open = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        // The asked points: the landing and 16 px before and after it.
        bool band = w.placeAsked.size() == 3 && near(w.placeAsked[0].second, 1509.0) && near(w.placeAsked[1].second, 1525.0)
            && near(w.placeAsked[2].second, 1541.0);
        check("walkable/open_floor_passes_and_asks_the_band", open == Landing::Ok && band
            && w.gateAskedEnds.size() == 1 && near(w.gateAskedEnds[0].second, 1525.0));
    }
    {
        World w;
        Core c = make(w);
        const Landing farEnd = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, 4000.0, 1535.0);   // only P + 16 out
        const Landing nearEnd = c.Check(1000.0, 1000.0, 0.0, -1.0, 990.0, 4000.0, 4000.0); // only P + 16 below 0
        const Landing landing = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, 4000.0, 1520.0);
        const Landing nan = c.Check(1000.0, 1000.0, std::nan(""), 1.0, 525.0, 4000.0, 4000.0);
        check("walkable/landing_and_band_need_room", farEnd == Landing::Room && nearEnd == Landing::Room
            && landing == Landing::Room && nan == Landing::Room && w.placeAsks == 0,
            std::string(LandingName(farEnd)) + " " + std::string(LandingName(nearEnd)));
    }
    {
        World w;
        Core c = make(w);
        w.solids = { around(1000.0, 1541.0, 2.0) };
        const Landing after = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.solids = { around(1000.0, 1509.0, 2.0) };
        const Landing before = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.solids = { around(1000.0, 1525.0, 2.0) };
        const Landing on = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.solids = { around(1000.0, 1300.0, 30.0) };   // scenery on the way is crossed, not refused
        const Landing onTheWay = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        check("walkable/landing_and_band_need_no_family", after == Landing::Family && before == Landing::Family
            && on == Landing::Family && onTheWay == Landing::Ok);
    }
    {
        World w;
        Core c = make(w);
        w.gridBlocked = { around(1000.0, 1541.0, 2.0) };
        const Landing band = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.gridBlocked = { around(1000.0, 1525.0, 2.0) };
        const Landing on = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.hasGrid = false;   // a room with no grid: the same cells count free
        const Landing noGrid = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.hasGrid = true;
        w.gridThrows = true;
        ReadKind thrownKind = ReadKind::Takeoff;
        const Landing unread = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH, &thrownKind);
        w.gridThrows = false;
        w.gridUnreadable = true;
        ReadKind answeredKind = ReadKind::Takeoff;
        const Landing unreadable = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH, &answeredKind);
        // A cell that could not be read is never free: the candidate fails,
        // as a failed grid read rather than a blocked cell.
        check("walkable/landing_and_band_need_a_free_grid_cell_and_a_missing_grid_counts_free",
            band == Landing::Grid && on == Landing::Grid && noGrid == Landing::Ok && unread == Landing::Unread
            && thrownKind == ReadKind::Grid && unreadable == Landing::Unread && answeredKind == ReadKind::Grid,
            std::string(LandingName(unread)) + " " + std::string(LandingName(unreadable)));
    }
    {
        World w;
        Core c = make(w);
        w.gates = { Box{ 900.0, 1200.0, 1100.0, 1210.0 } };   // a gate across the segment
        const Landing gate = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.gates = { Box{ 900.0, 800.0, 1100.0, 810.0 } };     // behind the take-off
        const Landing behind = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        w.gates = { Box{ 1100.0, 1200.0, 1200.0, 1210.0 } };  // beside the segment
        const Landing beside = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH);
        check("walkable/a_gate_or_lock_on_the_segment_refuses", gate == Landing::Gate && behind == Landing::Ok
            && beside == Landing::Ok);
    }
    {
        World w;
        Core c = make(w);
        // A read that fails never passes the candidate (fail closed), and it is
        // reported as the read that failed, not as the family or a gate found.
        w.placeThrows = true;
        ReadKind placeKind = ReadKind::Takeoff;
        const Landing place = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH, &placeKind);
        w.placeThrows = false;
        w.gateThrows = true;
        ReadKind gateKind = ReadKind::Takeoff;
        const Landing gate = c.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, w.roomW, w.roomH, &gateKind);
        World u;
        Core bare = make(u, NoPlace | NoGrid | NoGate);
        ReadKind unwiredKind = ReadKind::Takeoff;
        const Landing unwired = bare.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, u.roomW, u.roomH, &unwiredKind);
        const Landing noOut = bare.Check(1000.0, 1000.0, 0.0, 1.0, 525.0, u.roomW, u.roomH);   // the kind is optional
        check("walkable/an_unread_answer_counts_as_blocked", place == Landing::Unread && placeKind == ReadKind::Family
            && gate == Landing::Unread && gateKind == ReadKind::Gate && unwired == Landing::Unread
            && unwiredKind == ReadKind::Family && noOut == Landing::Unread,
            std::string(LandingName(place)) + " " + std::string(LandingName(gate)) + " " + std::string(LandingName(unwired)));
    }
    {
        World w;
        w.roomReadable = false;
        Core c = make(w);
        c.SetActive(true);
        const long writes = jumpToCursor(c, w, 10, 175.0);
        check("walkable/an_unreadable_room_is_a_vanilla_jump", refusedUnreadableOnly(c) && c.Stats().unreadRoom == 1
            && writes == 0 && near(w.py, 1175.0) && w.placeAsks == 0, counts(c.Stats()));
    }
    {
        World w;
        w.posReadable = false;
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        const Move m = c.OnLeapExit(10, true);
        check("walkable/an_unreadable_take_off_is_a_vanilla_jump", refusedUnreadableOnly(c)
            && c.Stats().unreadTakeoff == 1 && !m.write && w.placeAsks == 0, counts(c.Stats()));
    }

    // ---- back_off: a refused 3M point --------------------------------------
    {
        World w;
        w.solids = { Box{ 900.0, 1480.0, 1100.0, 1700.0 } };   // the 3M point and its band are in a wall
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        // The farthest candidate 525 - 16k whose band clears y = 1480: 525 - 16k + 16 < 480, k = 4: 461.
        check("back_off/steps_to_the_farthest_walkable_candidate", c.LastDecision() == Decision::BackedOff
            && near(c.LastLanding(), 461.0) && near(c.LastFactor(), 461.0 / 175.0) && near(w.py, 1461.0, 1e-6)
            && c.LastCandidates() == 5 && noRefusals(c.Stats()) && c.Stats().backedOff == 1, counts(c.Stats()) + " " + pos(w) + " L=" + std::to_string(c.LastLanding()));
    }
    {
        World w;
        w.gates = { Box{ 900.0, 1400.0, 1100.0, 1405.0 } };   // a gate at 400 px: the far candidates are behind it
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        // Every candidate past the gate fails on it; 525 - 16k <= 399 first at k = 8: 397.
        bool ownGateAsked = false;
        for (const auto& e : w.gateAskedEnds) ownGateAsked = ownGateAsked || near(e.second, 1397.0);
        check("back_off/a_gate_beyond_backs_off_to_its_near_side", c.LastDecision() == Decision::BackedOff
            && near(c.LastLanding(), 397.0) && ownGateAsked && near(w.py, 1397.0, 1e-6),
            counts(c.Stats()) + " L=" + std::to_string(c.LastLanding()));
    }
    {
        // A candidate that would pass were its gate check skipped is not accepted:
        // with no gate check wired, or one that cannot answer, nothing is.
        World w;
        Core c = make(w, NoGate);
        c.SetActive(true);
        const long unwired = jumpToCursor(c, w, 10, 175.0);
        const Decision first = c.LastDecision();
        World t;
        t.gateThrows = true;
        Core d = make(t);
        d.SetActive(true);
        const long throwing = jumpToCursor(d, t, 10, 175.0);
        // Every candidate skipped on its failed gate read: refused-unreadable,
        // never refused-gate (no gate was found), and nothing granted.
        check("back_off/a_skipped_gate_check_is_not_accepted", first == Decision::RefusedUnreadable
            && refusedUnreadableOnly(c) && refusedUnreadableOnly(d) && unwired == 0 && throwing == 0
            && c.Stats().unreadGate == static_cast<uint64_t>(c.LastCandidates())
            && d.Stats().unreadGate == static_cast<uint64_t>(d.LastCandidates()) && d.Stats().rejectedGate == 0
            && near(w.py, 1175.0) && near(t.py, 1175.0)
            && t.gateAskedEnds.size() == static_cast<size_t>(d.LastCandidates()),
            counts(c.Stats()) + " | " + counts(d.Stats()));
    }
    {
        World w;
        w.solids = { Box{ 900.0, 1180.0, 1100.0, 1700.0 } };   // every candidate past M blocked
        Core c = make(w);
        c.SetActive(true);
        const long writes = jumpToCursor(c, w, 10, 175.0);
        double nearest = 1e9;
        for (const auto& e : w.gateAskedEnds) nearest = std::min(nearest, e.second - 1000.0);
        double nearestPlace = 1e9;
        for (const auto& p : w.placeAsked) nearestPlace = std::min(nearestPlace, p.second - 1000.0);
        // Candidates 525, 509, ... down to the last one still greater than M = 175: 189.
        check("back_off/none_past_m_is_a_vanilla_jump", c.LastDecision() == Decision::RefusedLanding
            && writes == 0 && near(w.py, 1175.0) && c.LastCandidates() == 22 && near(nearestPlace, 189.0 - 16.0)
            && w.gateAskedEnds.empty() && c.Stats().refusedLanding == 1 && c.Stats().granted == 0,
            counts(c.Stats()) + " candidates=" + std::to_string(c.LastCandidates()) + " nearest=" + std::to_string(nearestPlace)
                + " gate-nearest=" + std::to_string(nearest));
    }
    {
        World w;
        w.gates = { Box{ 900.0, 1100.0, 1100.0, 1105.0 } };   // a lock inside M: every candidate is behind it
        Core c = make(w);
        c.SetActive(true);
        const long writes = jumpToCursor(c, w, 10, 175.0);
        check("back_off/every_candidate_behind_a_gate_is_refused_gate", c.LastDecision() == Decision::RefusedGate
            && c.Stats().refusedGate == 1 && c.Stats().refusedLanding == 0 && writes == 0 && near(w.py, 1175.0),
            counts(c.Stats()));
    }
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        w.solids = { Box{ 900.0, 1480.0, 1100.0, 1700.0 } };
        jumpToCursor(c, w, 10, 175.0);                        // backed off
        at(w, 1000.0, 1000.0);
        w.solids = { Box{ 900.0, 1180.0, 1100.0, 1700.0 } };
        jumpToCursor(c, w, 30, 175.0);                        // refused
        at(w, 1000.0, 1000.0);
        w.solids.clear();
        jumpToCursor(c, w, 50, 175.0);                        // granted at 3M
        const Counters& k = c.Stats();
        check("back_off/counted_apart_from_a_refusal", k.jumps == 3 && k.granted == 2 && k.backedOff == 1
            && k.refusedLanding == 1 && k.refusedGate == 0 && k.refusedCursor == 0, counts(k));
    }

    // ---- refused_unreadable: a failed read is counted as a failed read ------
    // Failing closed stays: no candidate is accepted on a read that failed.
    // But a read that is unbound, throws, answers false or gives a non-finite
    // value is counted under its own kind, and a jump that ends vanilla on it
    // is refused-unreadable, never refused-landing, -gate or -cursor.
    {
        // The take-off: unbound, throwing, false, non-finite.
        bool ok = true;
        std::string detail;
        for (int v = 0; v < 4; ++v) {
            World w;
            if (v == 1) w.posThrows = true;
            if (v == 2) w.posReadable = false;
            if (v == 3) w.px = std::nan("");
            Core c = make(w, v == 0 ? NoPosition : 0u);
            c.SetActive(true);
            c.OnLeapEntry(10, true);
            const Move m = c.OnLeapExit(10, true);
            const bool one = refusedUnreadableOnly(c) && c.Stats().unreadTakeoff == 1 && c.Stats().unreadCursor == 0
                && !m.write && w.placeAsks == 0 && w.roomReads == 0;
            if (!one) detail += " [v" + std::to_string(v) + " " + counts(c.Stats()) + "]";
            ok = ok && one;
        }
        check("refused_unreadable/takeoff", ok, detail);
    }
    {
        // The cursor: unbound, throwing, false, NaN, infinite.
        bool ok = true;
        std::string detail;
        for (int v = 0; v < 5; ++v) {
            World w;
            if (v == 1) w.cursorThrows = true;
            if (v == 2) w.cursorReadable = false;
            if (v == 3) w.cy = std::nan("");
            if (v == 4) w.cx = std::numeric_limits<double>::infinity();
            Core c = make(w, v == 0 ? NoCursor : 0u);
            c.SetActive(true);
            const long writes = fly(c, w, 10, 0.0, 1.0, 175.0);
            const bool one = refusedUnreadableOnly(c) && c.Stats().unreadCursor == 1 && c.Stats().unreadTakeoff == 0
                && writes == 0 && near(w.py, 1175.0) && w.placeAsks == 0;
            if (!one) detail += " [v" + std::to_string(v) + " " + counts(c.Stats()) + "]";
            ok = ok && one;
        }
        check("refused_unreadable/cursor", ok, detail);
    }
    {
        // A readable cursor within 16 px stays refused-cursor.
        World w;
        w.cy = 1010.0;
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        check("refused_unreadable/a_readable_near_cursor_stays_refused_cursor",
            c.LastDecision() == Decision::RefusedCursor && c.Stats().refusedCursor == 1 && noUnread(c.Stats()),
            counts(c.Stats()));
    }
    {
        // The room: unbound, throwing, false, NaN width, infinite height.
        bool ok = true;
        std::string detail;
        for (int v = 0; v < 5; ++v) {
            World w;
            if (v == 1) w.roomThrows = true;
            if (v == 2) w.roomReadable = false;
            if (v == 3) w.roomW = std::nan("");
            if (v == 4) w.roomH = std::numeric_limits<double>::infinity();
            Core c = make(w, v == 0 ? NoRoom : 0u);
            c.SetActive(true);
            const long writes = jumpToCursor(c, w, 10, 175.0);
            const bool one = refusedUnreadableOnly(c) && c.Stats().unreadRoom == 1 && writes == 0
                && near(w.py, 1175.0) && w.placeAsks == 0 && c.LastCandidates() == 0 && c.Stats().rejectedRoom == 0;
            if (!one) detail += " [v" + std::to_string(v) + " " + counts(c.Stats()) + "]";
            ok = ok && one;
        }
        check("refused_unreadable/room", ok, detail);
    }
    {
        // place_meeting: unbound, throwing. Every candidate skipped on it.
        bool ok = true;
        std::string detail;
        for (int v = 0; v < 2; ++v) {
            World w;
            if (v == 1) w.placeThrows = true;
            Core c = make(w, v == 0 ? NoPlace : 0u);
            c.SetActive(true);
            const long writes = jumpToCursor(c, w, 10, 175.0);
            const Counters& k = c.Stats();
            const bool one = refusedUnreadableOnly(c) && c.LastCandidates() == 22
                && k.unreadFamily == 22 && k.rejectedFamily == 0 && k.unreadGrid == 0 && k.unreadGate == 0
                && writes == 0 && near(w.py, 1175.0) && w.gateAskedEnds.empty();
            if (!one) detail += " [v" + std::to_string(v) + " " + counts(k) + "]";
            ok = ok && one;
        }
        check("refused_unreadable/place_meeting", ok, detail);
    }
    {
        // The grid: unbound, throwing, an Unreadable cell.
        bool ok = true;
        std::string detail;
        for (int v = 0; v < 3; ++v) {
            World w;
            if (v == 1) w.gridThrows = true;
            if (v == 2) w.gridUnreadable = true;
            Core c = make(w, v == 0 ? NoGrid : 0u);
            c.SetActive(true);
            const long writes = jumpToCursor(c, w, 10, 175.0);
            const Counters& k = c.Stats();
            const bool one = refusedUnreadableOnly(c) && k.unreadGrid == 22 && k.rejectedGrid == 0
                && k.unreadFamily == 0 && writes == 0 && near(w.py, 1175.0) && w.gateAskedEnds.empty();
            if (!one) detail += " [v" + std::to_string(v) + " " + counts(k) + "]";
            ok = ok && one;
        }
        check("refused_unreadable/grid", ok, detail);
    }
    {
        // The gate line: unbound, throwing.
        bool ok = true;
        std::string detail;
        for (int v = 0; v < 2; ++v) {
            World w;
            if (v == 1) w.gateThrows = true;
            Core c = make(w, v == 0 ? NoGate : 0u);
            c.SetActive(true);
            const long writes = jumpToCursor(c, w, 10, 175.0);
            const Counters& k = c.Stats();
            const bool one = refusedUnreadableOnly(c) && k.unreadGate == 22 && k.rejectedGate == 0
                && writes == 0 && near(w.py, 1175.0);
            if (!one) detail += " [v" + std::to_string(v) + " " + counts(k) + "]";
            ok = ok && one;
        }
        check("refused_unreadable/gate", ok, detail);
    }
    {
        // A failed read on one candidate, a later candidate passing: the later
        // one is granted, and the failed reads are counted.
        World f;
        f.placeThrowAt = { Box{ 900.0, 1520.0, 1100.0, 1545.0 } };   // the points at 1525 and 1541
        Core cf = make(f);
        cf.SetActive(true);
        jumpToCursor(cf, f, 10, 175.0);
        World g;
        g.gridUnreadAt = { Box{ 900.0, 1520.0, 1100.0, 1545.0 } };
        Core cg = make(g);
        cg.SetActive(true);
        jumpToCursor(cg, g, 10, 175.0);
        World t;
        t.gateThrowBeyondY = 1520.0;   // the 3M candidate's gate read fails, 509's does not
        Core ct = make(t);
        ct.SetActive(true);
        jumpToCursor(ct, t, 10, 175.0);
        const bool family = cf.LastDecision() == Decision::BackedOff && near(cf.LastLanding(), 493.0)
            && cf.Stats().unreadFamily == 2 && cf.Stats().refusedUnreadable == 0 && near(f.py, 1493.0, 1e-6);
        const bool grid = cg.LastDecision() == Decision::BackedOff && near(cg.LastLanding(), 493.0)
            && cg.Stats().unreadGrid == 2 && cg.Stats().refusedUnreadable == 0 && near(g.py, 1493.0, 1e-6);
        const bool gate = ct.LastDecision() == Decision::BackedOff && near(ct.LastLanding(), 509.0)
            && ct.Stats().unreadGate == 1 && ct.Stats().refusedUnreadable == 0 && near(t.py, 1509.0, 1e-6);
        check("refused_unreadable/a_failed_read_then_a_later_candidate_passing_is_granted", family && grid && gate,
            counts(cf.Stats()) + " | " + counts(cg.Stats()) + " | " + counts(ct.Stats()));
    }
    {
        // Candidates the game refused, mixed with one skipped on a failed read,
        // and none passing: refused-unreadable, not refused-landing.
        World w;
        w.solids = { Box{ 900.0, 1180.0, 1100.0, 1700.0 } };
        w.placeThrowAt = { Box{ 900.0, 1295.0, 1100.0, 1305.0 } };
        Core c = make(w);
        c.SetActive(true);
        const long writes = jumpToCursor(c, w, 10, 175.0);
        const Counters& k = c.Stats();
        check("refused_unreadable/game_refusals_mixed_with_a_failed_read", refusedUnreadableOnly(c)
            && k.unreadFamily > 0 && k.rejectedFamily > 0 && k.unreadFamily + k.rejectedFamily == 22
            && writes == 0 && near(w.py, 1175.0), counts(k));
    }
    {
        // Refusals on successful reads, counted by reason, per candidate.
        World fam;
        fam.solids = { Box{ 900.0, 1180.0, 1100.0, 1700.0 } };
        Core cf = make(fam);
        cf.SetActive(true);
        jumpToCursor(cf, fam, 10, 175.0);
        World grd;
        grd.gridBlocked = { Box{ 900.0, 1180.0, 1100.0, 1700.0 } };
        Core cg = make(grd);
        cg.SetActive(true);
        jumpToCursor(cg, grd, 10, 175.0);
        World gat;
        gat.gates = { Box{ 900.0, 1100.0, 1100.0, 1105.0 } };
        Core ct = make(gat);
        ct.SetActive(true);
        jumpToCursor(ct, gat, 10, 175.0);
        World rm;
        rm.roomH = 1150.0;   // every candidate past M is outside the room
        Core cr = make(rm);
        cr.SetActive(true);
        jumpToCursor(cr, rm, 10, 175.0);
        World wall;
        wall.solids = { Box{ 900.0, 1480.0, 1100.0, 1700.0 } };
        Core cw = make(wall);
        cw.SetActive(true);
        jumpToCursor(cw, wall, 10, 175.0);
        const Counters& f = cf.Stats();
        const Counters& g = cg.Stats();
        const Counters& t = ct.Stats();
        const Counters& r = cr.Stats();
        const Counters& b = cw.Stats();
        const bool ok = cf.LastDecision() == Decision::RefusedLanding && f.rejectedFamily == 22 && f.rejectedGrid == 0
                && f.rejectedRoom == 0 && f.rejectedGate == 0 && noUnread(f) && f.refusedLanding == 1
            && cg.LastDecision() == Decision::RefusedLanding && g.rejectedGrid == 22 && g.rejectedFamily == 0
                && noUnread(g) && g.refusedLanding == 1
                && g.gridBlocked == 22 && g.gridNoGrid == 0 && f.gridFree + f.gridBlocked + f.gridNoGrid == 0
            && ct.LastDecision() == Decision::RefusedGate && t.rejectedGate == 22 && t.rejectedFamily == 0
                && noUnread(t) && t.refusedGate == 1
            && cr.LastDecision() == Decision::RefusedLanding && r.rejectedRoom == 22 && r.rejectedFamily == 0
                && noUnread(r) && r.refusedLanding == 1
            && cw.LastDecision() == Decision::BackedOff && b.rejectedFamily == 4 && noUnread(b);
        check("refused_unreadable/refusals_on_successful_reads_counted_by_reason", ok,
            counts(f) + " | " + counts(g) + " | " + counts(t) + " | " + counts(r) + " | " + counts(b));
    }
    {
        // A room with no grid still counts free: granted, nothing unread, and
        // each of its three cells counted as nogrid, never as free, so a grid
        // read that does not attach is visible. Beside it, the same jump with
        // a grid counts the three cells free.
        World w;
        w.hasGrid = false;
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        World v;
        Core cv = make(v);
        cv.SetActive(true);
        jumpToCursor(cv, v, 10, 175.0);
        const Counters& k = c.Stats();
        const Counters& kv = cv.Stats();
        check("refused_unreadable/a_room_with_no_grid_is_still_free", c.LastDecision() == Decision::Granted
            && noUnread(k) && k.rejectedGrid == 0 && near(w.py, 1525.0, 1e-6)
            && k.gridNoGrid == 3 && k.gridFree == 0 && k.gridBlocked == 0
            && cv.LastDecision() == Decision::Granted && kv.gridFree == 3 && kv.gridNoGrid == 0 && kv.gridBlocked == 0,
            counts(k) + " | " + counts(kv));
    }

    // ---- the clamp at the landing distance ---------------------------------
    {
        World w;
        w.max = Max{ 100.0, MaxSource::Cap };   // M underestimates a 175 px jump
        w.cy = 1900.0;
        Core c = make(w);
        c.SetActive(true);
        // f = 300 / 100 = 3: the scaled jump would reach 525, so it ends at 300
        // and the calls after that leave the player there.
        std::vector<double> ys;
        const double step = 175.0 / 6.0;
        for (int k = 0; k < 6; ++k) {
            c.OnLeapEntry(10 + k, true);
            w.py += step;
            const Move m = c.OnLeapExit(10 + k, true);
            if (m.write) { w.px = m.x; w.py = m.y; }
            ys.push_back(w.py);
        }
        c.Tick(20);
        const bool held = near(ys[3], 1300.0, 1e-6) && near(ys[4], 1300.0, 1e-6) && near(ys[5], 1300.0, 1e-6)
            && ys[2] < 1300.0;
        check("clamp/ends_at_the_landing_distance_and_holds_there", held && c.Stats().clamped == 1
            && near(from(w, 1000.0, 1000.0), 300.0, 1e-6) && c.Stats().scaled == 6,
            "y=" + std::to_string(ys[0]) + "," + std::to_string(ys[2]) + "," + std::to_string(ys[3]) + "," + std::to_string(ys[5]));
    }
    {
        World w;
        w.cx = 1012.0;
        w.cy = 1016.0;   // 20 px: f = 525 / 20 = 26.25
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        w.px += 0.6 * 30.0;   // the game's first call already overshoots its cursor
        w.py += 0.8 * 30.0;
        const Move m = c.OnLeapExit(10, true);
        check("clamp/a_large_factor_reaches_the_landing_on_the_first_call", m.write
            && near(std::hypot(m.x - 1000.0, m.y - 1000.0), 525.0, 1e-6) && near(m.x, 1000.0 + 0.6 * 525.0, 1e-6)
            && c.Stats().clamped == 1, "move=" + std::to_string(m.x) + "," + std::to_string(m.y));
    }
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        const Move still = c.OnLeapExit(10, true);   // the call did not move the player
        const Move again = c.OnLeapExit(10, true);   // no entry before it
        c.OnLeapEntry(11, false);
        const Move other = c.OnLeapExit(11, false);
        check("clamp/a_call_that_did_not_move_is_left_alone", !still.write && !again.write && !other.write
            && c.Stats().scaled == 0 && c.LastDecision() == Decision::Granted, counts(c.Stats()));
    }

    // ---- the learner gets the natural length -------------------------------
    {
        World w;
        w.cy = 1100.0;
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);
        const bool one = w.learned.size() == 1;
        const Learned l = one ? w.learned[0] : Learned{};
        check("learner/natural_length_handed_while_scaled", one && near(l.naturalLength, 100.0, 1e-6)
            && near(l.naturalDy, 100.0, 1e-6) && near(l.takeoffY, 1000.0) && l.haveCursor && near(l.cursorY, 1100.0)
            && l.decision == Decision::Granted && near(w.py, 1525.0, 1e-6),
            "natural=" + std::to_string(l.naturalLength) + " " + pos(w));
    }
    {
        World w;
        w.max = Max{ 100.0, MaxSource::Cap };
        w.cy = 1900.0;
        Core c = make(w);
        c.SetActive(true);
        jumpToCursor(c, w, 10, 175.0);   // clamped at 300; the calls after still count
        at(w, 1000.0, 1000.0);
        w.cursorReadable = false;
        jumpToCursor(c, w, 30, 175.0);   // a vanilla jump is learned from as well
        check("learner/natural_length_includes_held_calls_and_vanilla_jumps", w.learned.size() == 2
            && near(w.learned[0].naturalLength, 175.0, 1e-6) && near(w.learned[1].naturalLength, 175.0, 1e-6)
            && !w.learned[1].haveCursor && w.learned[1].decision == Decision::RefusedUnreadable);
    }
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        w.py += 50.0;
        c.OnLeapExit(10, true);
        c.SetActive(false);   // switched off mid-jump: dropped, not learned
        c.Tick(20);
        check("learner/a_dropped_jump_is_not_learned", w.learned.empty() && !c.JumpActive());
    }

    // ---- the window: a new jump after a two-frame gap ------------------------
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        c.OnLeapEntry(11, true);   // same jump: decided once
        const long cursorAfterOne = w.cursorReads;
        c.OnLeapEntry(13, true);   // a gap of two frames: a new jump
        const long cursorAfterTwo = w.cursorReads;
        c.OnLeapEntry(14, true);
        c.OnLeapEntry(15, true);
        const bool open = c.WindowOpen(16) && !c.WindowOpen(17);
        c.Tick(16);
        const bool stillOpen = c.JumpActive();
        c.Tick(17);
        check("window/a_new_jump_after_a_two_frame_gap", c.Stats().jumps == 2 && cursorAfterOne == 1
            && cursorAfterTwo == 2 && open && stillOpen && !c.JumpActive() && w.learned.empty(),
            counts(c.Stats()));
    }

    // ---- a granted jump's answers ------------------------------------------
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        const bool granted = c.LastDecision() == Decision::Granted;
        const bool family = ask(c, 10, true, true, false, true) == Answer::False
            && ask(c, 11, true, true, false, true, false, Builtin::CollisionCircle) == Answer::Noone;
        const bool free = ask(c, 10, true, true, false, false) == Answer::Real;
        const bool other = ask(c, 10, false, true, false, true) == Answer::Real;
        const bool notFamily = ask(c, 10, true, false, false, true) == Answer::Real;
        const bool gateNamed = ask(c, 10, true, true, true, true) == Answer::Real;
        const bool gateThere = ask(c, 10, true, true, false, true, true) == Answer::Real;
        const bool closed = ask(c, 12, true, true, false, true) == Answer::Real;
        check("answers/a_granted_window_answers_the_family_only", granted && family && free && other && notFamily
            && gateNamed && gateThere && closed && c.Stats().answered == 2 && c.Stats().excluded == 2,
            counts(c.Stats()));
    }

    // ---- learner_clean: only a clean jump reaches the learner -----------------
    // jumpscenery's rule (Replan 2): its take-off read, none of its queries
    // answered, and no really-blocked player family query in its take-off
    // frame, before or after the skillsLeap entry that started it.
    {
        World w;
        w.cy = 1100.0;
        Core c = make(w);
        c.SetActive(true);
        // A free family query in the take-off frame is the walk finding open ground.
        flyAsking(c, w, 10, 0.0, 1.0, 100.0, [&](int64_t f, int k) { if (k == 0) ask(c, f, true, true, false, false); });
        const bool one = w.learned.size() == 1;
        const Learned l = one ? w.learned[0] : Learned{};
        check("learner_clean/a_granted_open_ground_jump_is_learned", one && l.decision == Decision::Granted
            && near(l.naturalLength, 100.0, 1e-6) && l.haveCursor && near(l.cursorY, 1100.0) && near(l.takeoffY, 1000.0)
            && c.Stats().answered == 0 && c.Stats().scaled > 0, counts(c.Stats()));
    }
    {
        World w;
        w.cy = 1100.0;
        Core c = make(w);
        c.SetActive(true);
        Answer a = Answer::Real;
        flyAsking(c, w, 10, 0.0, 1.0, 100.0, [&](int64_t f, int k) { if (k == 2) a = ask(c, f, true, true, false, true); });
        check("learner_clean/a_granted_jump_with_an_answered_query_is_not_learned", a == Answer::False
            && c.Stats().answered == 1 && c.Stats().granted == 1 && w.learned.empty() && !c.JumpActive(),
            counts(c.Stats()));
    }
    // A vanilla jump: every candidate past M lies in the family, so it is
    // refused-landing and the game's own jump runs, here 60 px south.
    auto vanillaWorld = [](World& w) {
        w.cy = 1300.0;
        w.solids = { Box{ 0.0, 1150.0, 4000.0, 4000.0 } };
    };
    {
        World w, v;
        vanillaWorld(w);
        vanillaWorld(v);
        Core c = make(w), control = make(v);
        c.SetActive(true);
        control.SetActive(true);
        Answer a = Answer::False;
        flyAsking(c, w, 10, 0.0, 1.0, 60.0, [&](int64_t f, int k) { if (k == 0) a = ask(c, f, true, true, false, true); });
        flyAsking(control, v, 10, 0.0, 1.0, 60.0, [](int64_t, int) {});
        check("learner_clean/a_vanilla_jump_blocked_in_its_take_off_frame_is_not_learned",
            c.LastDecision() == Decision::RefusedLanding && a == Answer::Real && w.learned.empty()
                && v.learned.size() == 1 && near(v.learned[0].naturalLength, 60.0, 1e-6)
                && v.learned[0].decision == Decision::RefusedLanding,
            counts(c.Stats()));
    }
    {
        // A blocked query in the take-off frame before the entry that started
        // the jump keeps it from the learner, as jumpscenery's before-open=
        // counts it. The controls are learned: one in an earlier frame,
        // another self's, one whose object is not the family, and a free one.
        auto learnedAfter = [&](int64_t frame, bool player, bool family, bool blocked) {
            World w;
            vanillaWorld(w);
            Core c = make(w);
            c.SetActive(true);
            const Answer a = ask(c, frame, player, family, false, blocked);
            flyAsking(c, w, 10, 0.0, 1.0, 60.0, [](int64_t, int) {});
            return a == Answer::Real && c.LastDecision() == Decision::RefusedLanding ? w.learned.size() : 99u;
        };
        const size_t before = learnedAfter(10, true, true, true);
        const size_t earlier = learnedAfter(9, true, true, true);
        const size_t other = learnedAfter(10, false, true, true);
        const size_t notFamily = learnedAfter(10, true, false, true);
        const size_t free = learnedAfter(10, true, true, false);
        check("learner_clean/a_blocked_query_before_the_entry_in_its_take_off_frame_is_not_learned",
            before == 0 && earlier == 1 && other == 1 && notFamily == 1 && free == 1,
            "before=" + std::to_string(before) + " earlier=" + std::to_string(earlier) + " other="
                + std::to_string(other) + " not-family=" + std::to_string(notFamily) + " free=" + std::to_string(free));
    }
    {
        World w;
        vanillaWorld(w);
        Core c = make(w);
        c.SetActive(true);
        Answer a = Answer::False;
        flyAsking(c, w, 10, 0.0, 1.0, 60.0, [&](int64_t f, int k) { if (k == 2) a = ask(c, f, true, true, false, true); });
        check("learner_clean/a_blocked_query_in_a_later_frame_of_a_vanilla_jump_is_still_learned",
            a == Answer::Real && w.learned.size() == 1 && near(w.learned[0].naturalLength, 60.0, 1e-6)
                && w.learned[0].decision == Decision::RefusedLanding,
            counts(c.Stats()));
    }
    {
        World w;
        Core c = make(w);
        c.SetActive(true);
        c.OnLeapEntry(10, true);
        w.py += 50.0;
        c.OnLeapExit(10, true);
        c.SetActive(false);   // switched off mid-jump: dropped, not learned
        c.Tick(20);
        check("learner_clean/a_dropped_jump_is_still_not_learned", w.learned.empty() && !c.JumpActive());
    }

    // ---- classify: Mark 1's own object table, jumpscenery's rule ---------------
    {
        Classifier k = makeClassifier();
        bool ok = k.Configured();
        for (int o : { objs::CollisionParent, objs::CollisionProp, objs::Fence, objs::WallParent, objs::InvisibleWall }) {
            const ObjectClass oc = k.Classify(o);
            ok = ok && oc.family && !oc.excluded;
        }
        check("classify/the_family_and_its_descendants_are_family", ok);
    }
    {
        Classifier k = makeClassifier();
        bool ok = true;
        for (int o : { objs::GateParent, objs::GateChild, objs::LockObj, objs::LockChild }) ok = ok && k.Classify(o).excluded;
        check("classify/gates_locks_and_their_descendants_are_excluded", ok);
    }
    {
        Classifier k = makeClassifier();
        bool ok = true;
        for (int o : { objs::Skeleton, objs::EnemyParent, objs::AvoidableParent, 12345 }) {
            const ObjectClass oc = k.Classify(o);
            ok = ok && !oc.family && !oc.excluded;
        }
        const long calls = g_AncestorCalls;
        for (int o : { -1, -4 }) {
            const ObjectClass oc = k.Classify(o);
            ok = ok && !oc.family && !oc.excluded;
        }
        check("classify/anything_else_and_a_negative_index_are_neither", ok && g_AncestorCalls == calls
            && k.Remembered() == 4);
    }
    {
        Classifier k;
        k.SetIsAncestor([](int, int) -> bool { throw std::runtime_error("object_is_ancestor"); });
        k.SetObjects(objs::CollisionParent, objs::GateParent, objs::LockObj);
        const ObjectClass fence = k.Classify(objs::Fence);
        const ObjectClass skeleton = k.Classify(objs::Skeleton);
        check("classify/a_failed_ancestry_question_is_not_family_and_excluded", !fence.family && fence.excluded
            && !skeleton.family && skeleton.excluded);
    }
    {
        bool ok = true;
        const int sets[3][3] = {
            { -1, objs::GateParent, objs::LockObj },
            { objs::CollisionParent, -1, objs::LockObj },
            { objs::CollisionParent, objs::GateParent, -1 },
        };
        const long calls = g_AncestorCalls;
        for (const auto& s : sets) {
            Classifier k;
            k.SetIsAncestor(objectIsAncestor);
            k.SetObjects(s[0], s[1], s[2]);
            ok = ok && !k.Configured();
            for (int o : { objs::CollisionParent, objs::Fence, objs::GateParent, objs::LockObj }) {
                const ObjectClass oc = k.Classify(o);
                ok = ok && !oc.family && !oc.excluded;
            }
            ok = ok && k.Remembered() == 0;
        }
        check("classify/an_unresolved_object_leaves_every_object_neither", ok && g_AncestorCalls == calls);
    }
    {
        Classifier k = makeClassifier();
        const long first = g_AncestorCalls;
        k.Classify(objs::Fence);
        const long once = g_AncestorCalls - first;
        k.Classify(objs::Fence);
        const bool askedOnce = once > 0 && g_AncestorCalls - first == once;
        Classifier t = makeClassifier();
        const int max = static_cast<int>(ForgePact::JumpSceneryMod::kFamilyTableMax);
        for (int o = 0; o < max; ++o) t.Classify(o);
        const bool full = t.Remembered() == ForgePact::JumpSceneryMod::kFamilyTableMax;
        const long beforeAgain = g_AncestorCalls;
        t.Classify(0);
        const bool remembered = g_AncestorCalls == beforeAgain;
        t.Classify(max);   // one more: the table forgets everything first
        const bool forgot = t.Remembered() == 1;
        const long beforeZero = g_AncestorCalls;
        t.Classify(0);
        check("classify/each_object_is_asked_once_and_the_table_forgets_at_its_max", askedOnce && full && remembered
            && forgot && g_AncestorCalls > beforeZero && t.Remembered() == 2 && kFamilyTableMax == ForgePact::JumpSceneryMod::kFamilyTableMax,
            "remembered=" + std::to_string(t.Remembered()));
    }

    // ---- grid_cell: the path-grid cell's index and answer --------------------
    {
        const bool ok = kGridCellPx == 16.0 && GridCellIndex(0.0) == 0.0 && GridCellIndex(15.99) == 0.0
            && GridCellIndex(16.0) == 1.0 && GridCellIndex(33.0) == 2.0 && GridCellIndex(-0.5) == -1.0
            && GridCellIndex(-16.0) == -1.0 && GridCellIndex(-16.5) == -2.0
            && std::isnan(GridCellIndex(std::numeric_limits<double>::quiet_NaN()));
        check("grid_cell/the_cell_index_is_floor_of_v_over_16_negatives_included", ok);
    }
    {
        long calls = 0;
        double gotGrid = -9.0, gotX = 0.0, gotY = 0.0;
        auto free = [&](double grid, double cx, double cy, ValueKind& kind, double& value) {
            ++calls;
            gotGrid = grid;
            gotX = cx;
            gotY = cy;
            kind = ValueKind::Number;
            value = 0.0;
            return true;
        };
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const bool notHandles = GridCellAt(ValueKind::Other, 5.0, 40.0, 40.0, free) == Cell::NoGrid
            && GridCellAt(ValueKind::Number, -1.0, 40.0, 40.0, free) == Cell::NoGrid
            && GridCellAt(ValueKind::Reference, -1.0, 40.0, 40.0, free) == Cell::NoGrid
            && GridCellAt(ValueKind::Number, nan, 40.0, 40.0, free) == Cell::NoGrid && calls == 0;
        const bool reference = GridCellAt(ValueKind::Reference, 0.0, -1.0, 40.0, free) == Cell::Free && calls == 1
            && gotGrid == 0.0 && gotX == -1.0 && gotY == 2.0;
        const bool number = GridCellAt(ValueKind::Number, 3.0, 100.0, 15.0, free) == Cell::Free && calls == 2
            && gotGrid == 3.0 && gotX == 6.0 && gotY == 0.0;
        check("grid_cell/not_a_grid_handle_reads_no_grid", notHandles && reference && number,
            "calls=" + std::to_string(calls));
    }
    {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        long calls = 0;
        auto answer = [&](bool ok, ValueKind kind, double value) {
            return [&calls, ok, kind, value](double, double, double, ValueKind& k, double& v) {
                ++calls;
                k = kind;
                v = value;
                return ok;
            };
        };
        const bool failed = GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answer(false, ValueKind::Number, 0.0)) == Cell::Unreadable;
        const bool threw = GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0,
            [](double, double, double, ValueKind&, double&) -> bool { throw std::runtime_error("mp_grid_get_cell"); })
            == Cell::Unreadable;
        const bool reference = GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answer(true, ValueKind::Reference, 0.0)) == Cell::Unreadable;
        const bool other = GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answer(true, ValueKind::Other, 0.0)) == Cell::Unreadable;
        const bool notFinite = GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answer(true, ValueKind::Number, nan)) == Cell::Unreadable;
        const long before = calls;
        const bool badPoint = GridCellAt(ValueKind::Number, 1.0, nan, 40.0, answer(true, ValueKind::Number, 0.0)) == Cell::Unreadable
            && calls == before;
        check("grid_cell/a_failed_or_throwing_call_or_a_non_number_reads_unreadable",
            failed && threw && reference && other && notFinite && badPoint);
    }
    {
        auto answering = [](double value) {
            return [value](double, double, double, ValueKind& k, double& v) {
                k = ValueKind::Number;
                v = value;
                return true;
            };
        };
        check("grid_cell/below_zero_is_blocked_zero_or_more_is_free",
            GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answering(-1.0)) == Cell::Blocked
                && GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answering(-0.5)) == Cell::Blocked
                && GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answering(0.0)) == Cell::Free
                && GridCellAt(ValueKind::Number, 1.0, 40.0, 40.0, answering(1.0)) == Cell::Free);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
