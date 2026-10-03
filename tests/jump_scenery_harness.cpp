// Behavioral regression harness for jumpprobe's decision core
// (JumpSceneryProbe.hpp, ForgePact #16, research build only).
//
// The Python runner injects the REAL ForgePact::JumpScenery header below. No
// game is touched: the controlled world is an object table with parents, and
// object_is_ancestor answers from it the way the runtime does (an object is
// not its own ancestor). Frames are plain numbers.
//
// Baseline: lever off, every query runs the game's own function whatever the
// window; lever on, a query outside the window does too; inside the window a
// query whose self is not the player, or whose object is another family
// (Enemy_Parent_obj), does too. Target: inside the window the player's query
// against a Collision_Prop_obj descendant is answered no-collision, counted
// once, with each builtin's own answer kind; `all` widens to the map-edge
// walls and answers the two object-less rows; the window closes at exactly
// `frames`; the `scripts` flag rewrites only its three rows; `hold` keeps the
// window open with no jump script at all, and a lever no window ever opened
// for is named inert rather than read as ON.
#include <cstdint>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// PRODUCTION_JUMPSCENERY

using namespace ForgePact::JumpScenery;

// ---- the controlled world -------------------------------------------------
enum Obj : int {
    AvoidableParent = 1, CollisionParent = 957, CollisionProp = 959, Fence = 5001, Crate = 5002,
    DestructibleParent = 1325, Barrel = 5003, WallParent = 2000, InvisibleWall = 2269, GateParent = 1881,
    BlockObj = 609, EnemyAggroable = 300, EnemyParent = 301, Skeleton = 5004, PlayerObj = 3553, BossBlock = 682,
};
static const std::map<int, int> kParent = {
    { CollisionParent, AvoidableParent }, { CollisionProp, CollisionParent }, { Fence, CollisionProp },
    { Crate, CollisionProp }, { DestructibleParent, CollisionProp }, { Barrel, DestructibleParent },
    { WallParent, CollisionParent }, { InvisibleWall, WallParent }, { GateParent, WallParent },
    { BlockObj, CollisionParent }, { EnemyParent, EnemyAggroable }, { Skeleton, EnemyParent },
    { PlayerObj, EnemyAggroable },
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

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static Probe make()
{
    Probe p;
    p.SetIsAncestor(objectIsAncestor);
    p.SetFamilies(CollisionProp, CollisionParent);
    return p;
}

static long g_ObjectReads = 0;
static Answer ask(Probe& p, Builtin b, int64_t frame, bool player, int object)
{
    return p.DecideBuiltin(b, frame, player, [object]() { ++g_ObjectReads; return object; });
}

static std::string name(Builtin b) { return std::string(kBuiltins[static_cast<int>(b)].name); }
static std::string text(Answer a) { return std::string(AnswerName(a)); }
static std::string counts(const Counters& c)
{
    return "passed=" + std::to_string(c.passed) + " passthrough=" + std::to_string(c.passthrough)
        + " outside-window=" + std::to_string(c.outsideWindow) + " other-self=" + std::to_string(c.otherSelf)
        + " other-family=" + std::to_string(c.otherFamily);
}

static const Builtin kObjectRows[] = {
    Builtin::PositionMeeting, Builtin::PlaceMeeting, Builtin::InstancePlace, Builtin::InstancePosition,
    Builtin::CollisionPoint, Builtin::CollisionLine, Builtin::CollisionRectangle, Builtin::CollisionCircle,
};
static const Builtin kObjectlessRows[] = { Builtin::PlaceFree, Builtin::TilemapGetAtPixel };

int main()
{
    // ---- the table itself ----------------------------------------------
    {
        bool ok = kBuiltinCount == 10;
        std::string bad;
        for (Builtin b : kObjectRows) if (kBuiltins[static_cast<int>(b)].objectArg < 0) { ok = false; bad += name(b) + " "; }
        for (Builtin b : kObjectlessRows) if (kBuiltins[static_cast<int>(b)].objectArg >= 0) { ok = false; bad += name(b) + " "; }
        check("table/object_arguments", ok, bad);
        const bool kinds =
            kBuiltins[(int)Builtin::PositionMeeting].answer == Answer::False && kBuiltins[(int)Builtin::PlaceMeeting].answer == Answer::False
            && kBuiltins[(int)Builtin::InstancePlace].answer == Answer::Noone && kBuiltins[(int)Builtin::InstancePosition].answer == Answer::Noone
            && kBuiltins[(int)Builtin::CollisionPoint].answer == Answer::Noone && kBuiltins[(int)Builtin::CollisionLine].answer == Answer::Noone
            && kBuiltins[(int)Builtin::CollisionRectangle].answer == Answer::Noone && kBuiltins[(int)Builtin::CollisionCircle].answer == Answer::Noone
            && kBuiltins[(int)Builtin::TilemapGetAtPixel].answer == Answer::Zero && kBuiltins[(int)Builtin::PlaceFree].answer == Answer::True;
        check("table/answer_kinds", kinds);
        check("table/noone_is_minus_four", kNoone == -4.0);
    }

    // ---- baseline: the lever off ----------------------------------------
    {
        Probe p = make();
        // A jump with the lever off opens nothing.
        const bool opened = p.OnJumpEntry(100, true);
        bool all = true;
        std::string bad;
        for (int i = 0; i < kBuiltinCount; ++i) {
            const Builtin b = static_cast<Builtin>(i);
            for (int64_t f : { int64_t(100), int64_t(120), int64_t(500) }) {
                const Answer a = ask(p, b, f, true, Fence);
                if (a != Answer::RunOriginal) { all = false; bad += name(b) + "@" + std::to_string(f) + "=" + text(a) + " "; }
            }
        }
        check("baseline/jump_with_lever_off_opens_nothing", !opened && !p.WindowOpen(100));
        check("baseline/lever_off_passes_through", all, bad);
        const Counters& c = p.BuiltinCounters(Builtin::PositionMeeting);
        check("baseline/lever_off_counted_as_passthrough", c.passthrough == 3 && c.passed == 0, counts(c));
        bool scriptsOff = true;
        for (int i = 0; i < kScriptLeverCount; ++i)
            if (p.DecideScript(static_cast<ScriptLever>(i), 100, true) != Answer::RunOriginal) scriptsOff = false;
        check("baseline/lever_off_scripts_untouched", scriptsOff);
        check("baseline/lever_off_reads_no_object", g_ObjectReads == 0, "reads=" + std::to_string(g_ObjectReads));
    }

    // ---- baseline: lever on, but outside the window ---------------------
    {
        Probe p = make();
        p.SetLever(90, FamilyRule::Props, false);
        const long readsBefore = g_ObjectReads;
        bool all = true;
        for (int i = 0; i < kBuiltinCount; ++i)
            if (ask(p, static_cast<Builtin>(i), 50, true, Fence) != Answer::RunOriginal) all = false;
        const Counters& c = p.BuiltinCounters(Builtin::InstancePlace);
        check("baseline/outside_window_passes_through", all && c.outsideWindow == 1 && c.passed == 0, counts(c));
        check("baseline/outside_window_reads_no_object", g_ObjectReads == readsBefore);
        // A jump whose self is not the local player opens no window.
        const bool opened = p.OnJumpEntry(60, false);
        check("baseline/other_self_jump_opens_nothing", !opened && !p.WindowOpen(60)
            && ask(p, Builtin::PositionMeeting, 61, true, Fence) == Answer::RunOriginal);
    }

    // ---- baseline: inside the window, another self or another family ----
    {
        Probe p = make();
        p.SetLever(90, FamilyRule::Props, false);
        p.OnJumpEntry(200, true);
        const long readsBefore = g_ObjectReads;
        bool all = true;
        for (int i = 0; i < kBuiltinCount; ++i)
            if (ask(p, static_cast<Builtin>(i), 210, false, Fence) != Answer::RunOriginal) all = false;
        const Counters& c = p.BuiltinCounters(Builtin::CollisionPoint);
        check("baseline/other_self_passes_through", all && c.otherSelf == 1 && c.passed == 0, counts(c));
        check("baseline/other_self_reads_no_object", g_ObjectReads == readsBefore);
        bool enemy = true;
        for (Builtin b : kObjectRows) {
            if (ask(p, b, 210, true, EnemyParent) != Answer::RunOriginal) enemy = false;
            if (ask(p, b, 210, true, Skeleton) != Answer::RunOriginal) enemy = false;
            if (ask(p, b, 210, true, PlayerObj) != Answer::RunOriginal) enemy = false;
        }
        const Counters& e = p.BuiltinCounters(Builtin::PlaceMeeting);
        check("baseline/enemy_family_passes_through", enemy && e.otherFamily == 3 && e.passed == 0, counts(e));
        // Outside both families, and not an object at all (`all`, an unresolved
        // argument): the original runs.
        const bool outsiders = ask(p, Builtin::InstancePlace, 210, true, BossBlock) == Answer::RunOriginal
            && ask(p, Builtin::InstancePlace, 210, true, -1) == Answer::RunOriginal
            && ask(p, Builtin::InstancePlace, 210, true, -3) == Answer::RunOriginal;
        check("baseline/unknown_object_passes_through", outsiders, counts(p.BuiltinCounters(Builtin::InstancePlace)));
    }

    // ---- target: inside the window, a prop, from the player --------------
    {
        Probe p = make();
        p.SetLever(90, FamilyRule::Props, false);
        const bool opened = p.OnJumpEntry(1000, true);
        check("target/player_jump_opens_window", opened && p.WindowOpen(1000) && p.WindowsOpened() == 1);
        bool kinds = true;
        bool once = true;
        std::string bad;
        for (Builtin b : kObjectRows) {
            const Answer want = kBuiltins[static_cast<int>(b)].answer;
            const Answer got = ask(p, b, 1010, true, Fence);
            if (got != want) { kinds = false; bad += name(b) + "=" + text(got) + " "; }
            const Counters& c = p.BuiltinCounters(b);
            if (c.passed != 1 || c.passthrough || c.outsideWindow || c.otherSelf || c.otherFamily) { once = false; bad += name(b) + "{" + counts(c) + "} "; }
        }
        check("target/prop_answered_per_builtin", kinds, bad);
        check("target/counted_once", once, bad);
        // A grandchild of Collision_Prop_obj, and the family's own object.
        const bool deep = ask(p, Builtin::PositionMeeting, 1011, true, Barrel) == Answer::False
            && ask(p, Builtin::InstancePlace, 1011, true, CollisionProp) == Answer::Noone;
        check("target/descendants_and_the_family_itself", deep);
        // Every other self still runs the original inside the same window.
        check("target/window_is_the_players_only", ask(p, Builtin::PositionMeeting, 1012, false, Fence) == Answer::RunOriginal);
        // Each object's family is asked of the game once, then remembered.
        const long before = g_AncestorCalls;
        for (int k = 0; k < 50; ++k) ask(p, Builtin::PositionMeeting, 1013, true, Crate);
        const long first = g_AncestorCalls - before;
        for (int k = 0; k < 50; ++k) ask(p, Builtin::PositionMeeting, 1014, true, Crate);
        check("target/ancestry_asked_once_per_object", first == 1 && g_AncestorCalls - before == 1,
            "calls=" + std::to_string(g_AncestorCalls - before));
    }

    // ---- target: `props` against `all` -----------------------------------
    {
        Probe props = make();
        props.SetLever(90, FamilyRule::Props, false);
        props.OnJumpEntry(10, true);
        const bool wallProps = ask(props, Builtin::PositionMeeting, 11, true, InvisibleWall) == Answer::RunOriginal
            && ask(props, Builtin::InstancePlace, 11, true, GateParent) == Answer::RunOriginal
            && ask(props, Builtin::InstancePlace, 11, true, BlockObj) == Answer::RunOriginal;
        check("target/props_leaves_invisible_wall", wallProps, counts(props.BuiltinCounters(Builtin::PositionMeeting)));
        bool objectless = true;
        for (Builtin b : kObjectlessRows) {
            if (ask(props, b, 12, true, -1) != Answer::RunOriginal) objectless = false;
            if (props.BuiltinCounters(b).otherFamily != 1 || props.BuiltinCounters(b).passed != 0) objectless = false;
        }
        check("target/props_objectless_rows_pass_through", objectless,
            counts(props.BuiltinCounters(Builtin::PlaceFree)) + " | " + counts(props.BuiltinCounters(Builtin::TilemapGetAtPixel)));

        Probe all = make();
        all.SetLever(90, FamilyRule::All, false);
        all.OnJumpEntry(10, true);
        const bool wallAll = ask(all, Builtin::PositionMeeting, 11, true, InvisibleWall) == Answer::False
            && ask(all, Builtin::InstancePlace, 11, true, GateParent) == Answer::Noone
            && ask(all, Builtin::CollisionLine, 11, true, BlockObj) == Answer::Noone
            && ask(all, Builtin::CollisionCircle, 11, true, Fence) == Answer::Noone;
        check("target/all_widens_to_invisible_wall", wallAll, counts(all.BuiltinCounters(Builtin::PositionMeeting)));
        const bool objectlessAll = ask(all, Builtin::PlaceFree, 12, true, -1) == Answer::True
            && ask(all, Builtin::TilemapGetAtPixel, 12, true, -1) == Answer::Zero
            && all.BuiltinCounters(Builtin::PlaceFree).passed == 1 && all.BuiltinCounters(Builtin::TilemapGetAtPixel).passed == 1;
        check("target/all_answers_objectless_rows", objectlessAll);
        // `all` still leaves the enemies and the objects outside Collision_Parent_obj.
        const bool stillOut = ask(all, Builtin::PositionMeeting, 13, true, EnemyParent) == Answer::RunOriginal
            && ask(all, Builtin::InstancePlace, 13, true, BossBlock) == Answer::RunOriginal
            && ask(all, Builtin::PlaceFree, 13, false, -1) == Answer::RunOriginal;
        check("target/all_leaves_enemies_and_other_selves", stillOut);
    }

    // ---- target: the window's length --------------------------------------
    {
        Probe p = make();
        p.SetLever(90, FamilyRule::Props, false);
        p.OnJumpEntry(500, true);
        const bool lastIn = ask(p, Builtin::PositionMeeting, 589, true, Fence) == Answer::False;
        const bool firstOut = ask(p, Builtin::PositionMeeting, 590, true, Fence) == Answer::RunOriginal;
        const bool before = ask(p, Builtin::PositionMeeting, 499, true, Fence) == Answer::RunOriginal;
        check("target/window_closes_at_frames", lastIn && firstOut && before,
            counts(p.BuiltinCounters(Builtin::PositionMeeting)));
        p.Tick(589);
        const bool openAt589 = p.WindowRecorded();
        p.Tick(590);
        check("target/tick_closes_the_window", openAt589 && !p.WindowRecorded() && !p.WindowOpen(590));
        // The next jump opens a fresh window.
        p.OnJumpEntry(700, true);
        check("target/next_jump_reopens", p.WindowOpen(700) && p.WindowOpen(789) && !p.WindowOpen(790) && p.WindowsOpened() == 2);
        // A jump inside an open window restarts it from the new entry.
        p.OnJumpEntry(750, true);
        check("target/jump_inside_window_restarts_it", p.WindowOpen(839) && !p.WindowOpen(840));
        // `pass 0` closes it at once.
        p.LeverOff();
        check("target/pass0_closes_window", !p.WindowOpen(760) && !p.WindowRecorded()
            && ask(p, Builtin::PositionMeeting, 760, true, Fence) == Answer::RunOriginal);
        // A window length of one frame, and the bounds `pass 1` refuses.
        Probe q = make();
        q.SetLever(1, FamilyRule::Props, false);
        q.OnJumpEntry(40, true);
        check("target/one_frame_window", q.WindowOpen(40) && !q.WindowOpen(41));
        Probe r = make();
        const bool refused = !r.SetLever(0, FamilyRule::Props, false) && !r.SetLever(kMaxFrames + 1, FamilyRule::Props, false) && !r.LeverOn();
        check("target/frames_out_of_range_refused", refused);
        // `pass 1` again zeroes the counters and closes any window.
        q.SetLever(90, FamilyRule::Props, false);
        check("target/pass1_starts_clean", !q.WindowOpen(40) && q.BuiltinCounters(Builtin::PositionMeeting).passed == 0
            && q.WindowsOpened() == 0);
    }

    // ---- target: the `scripts` flag rewrites only its three rows ----------
    {
        Probe off = make();
        off.SetLever(90, FamilyRule::Props, false);
        off.OnJumpEntry(10, true);
        bool untouched = true;
        for (int i = 0; i < kScriptLeverCount; ++i)
            if (off.DecideScript(static_cast<ScriptLever>(i), 20, true) != Answer::RunOriginal) untouched = false;
        check("scripts/flag_off_runs_the_scripts", untouched && off.ScriptCounters(ScriptLever::CanMove).passthrough == 1);

        Probe on = make();
        on.SetLever(90, FamilyRule::Props, true);
        on.OnJumpEntry(10, true);
        const bool answers = on.DecideScript(ScriptLever::CanMove, 20, true) == Answer::True
            && on.DecideScript(ScriptLever::InstancePlaceTallest, 20, true) == Answer::Noone
            && on.DecideScript(ScriptLever::TilePlaceMeeting, 20, true) == Answer::False;
        check("scripts/three_rows_answered", answers);
        const bool guarded = on.DecideScript(ScriptLever::CanMove, 20, false) == Answer::RunOriginal
            && on.DecideScript(ScriptLever::CanMove, 100, true) == Answer::RunOriginal
            && on.DecideScript(ScriptLever::CanMove, 5, true) == Answer::RunOriginal;
        const Counters& c = on.ScriptCounters(ScriptLever::CanMove);
        check("scripts/only_inside_the_window_for_the_player", guarded && c.passed == 1 && c.otherSelf == 1 && c.outsideWindow == 2, counts(c));
        // The flag changes no builtin answer: the family rule still decides them.
        const bool builtinsSame = ask(on, Builtin::PositionMeeting, 20, true, Fence) == Answer::False
            && ask(on, Builtin::PositionMeeting, 20, true, InvisibleWall) == Answer::RunOriginal
            && ask(on, Builtin::PlaceFree, 20, true, -1) == Answer::RunOriginal;
        check("scripts/builtins_unchanged", builtinsSame);
    }

    // ---- `hold`: the window held open while the lever is on --------------
    // The lever's own route: a lever whose window only a jump script can
    // open answers nothing if neither script fires on the local jump, so
    // `hold` opens it without one.
    {
        Probe p = make();
        p.SetLever(90, FamilyRule::Props, false, true);
        const bool held = p.Holding() && p.WindowOpen(5) && p.WindowOpen(100000) && p.WindowsOpened() == 0;
        const bool answered = ask(p, Builtin::PositionMeeting, 5, true, Fence) == Answer::False
            && p.BuiltinCounters(Builtin::PositionMeeting).passed == 1
            && p.BuiltinCounters(Builtin::PositionMeeting).outsideWindow == 0;
        check("hold/window_held_open_without_a_jump", held && answered, counts(p.BuiltinCounters(Builtin::PositionMeeting)));
        const bool rules = ask(p, Builtin::PositionMeeting, 6, false, Fence) == Answer::RunOriginal
            && ask(p, Builtin::InstancePlace, 6, true, EnemyParent) == Answer::RunOriginal
            && ask(p, Builtin::InstancePlace, 6, true, InvisibleWall) == Answer::RunOriginal
            && ask(p, Builtin::PlaceFree, 6, true, -1) == Answer::RunOriginal
            && p.DecideScript(ScriptLever::CanMove, 6, true) == Answer::RunOriginal;
        check("hold/still_the_players_family_only", rules);
        // A jump entry still counts - J1's evidence - and the window stays held.
        const bool entry = p.OnJumpEntry(50, true) && p.WindowsOpened() == 1;
        p.Tick(1000);
        check("hold/jump_entry_counted_window_stays_held", entry && p.WindowOpen(1000) && p.WindowOpen(50 + 90 + 10));
        p.LeverOff();
        const bool released = !p.Holding() && !p.WindowOpen(10)
            && ask(p, Builtin::PositionMeeting, 10, true, Fence) == Answer::RunOriginal;
        p.SetLever(90, FamilyRule::Props, false);
        check("hold/pass0_releases_it", released && !p.Holding() && !p.WindowOpen(10));
        Probe q = make();
        q.SetLever(90, FamilyRule::All, true);
        check("hold/default_is_not_held", !q.Holding() && !q.WindowOpen(5)
            && ask(q, Builtin::PositionMeeting, 5, true, Fence) == Answer::RunOriginal);
        Probe s = make();
        s.SetLever(90, FamilyRule::All, true, true);
        const bool all = ask(s, Builtin::PlaceFree, 7, true, -1) == Answer::True
            && s.DecideScript(ScriptLever::CanMove, 7, true) == Answer::True;
        check("hold/all_and_scripts_answer_without_a_jump", all);
    }

    // ---- an inert lever is named, not read as ON -----------------------
    {
        Probe p = make();
        p.SetLever(90, FamilyRule::Props, true);
        const bool quiet = !p.Inert();   // no call yet: nothing to name
        ask(p, Builtin::PositionMeeting, 5, true, Fence);
        p.DecideScript(ScriptLever::CanMove, 5, true);
        const bool inert = p.Inert() && p.WindowsOpened() == 0 && p.OutsideWindowTotal() == 2;
        check("inert/no_window_opened_is_named", quiet && inert, "outside=" + std::to_string(p.OutsideWindowTotal()));
        p.OnJumpEntry(6, true);
        check("inert/a_window_ends_it", !p.Inert());
        Probe h = make();
        h.SetLever(90, FamilyRule::Props, false, true);
        ask(h, Builtin::PositionMeeting, 5, true, Crate);
        Probe off = make();
        ask(off, Builtin::PositionMeeting, 5, true, Fence);
        Probe other = make();
        other.SetLever(90, FamilyRule::Props, false);
        ask(other, Builtin::PositionMeeting, 5, false, Fence);
        check("inert/hold_lever_off_and_other_selves_are_not", !h.Inert() && !off.Inert() && !other.Inert());
    }

    // ---- the probe's activity and the trace's cap ------------------------
    {
        Probe p = make();
        const bool idle = !p.Active();
        p.SetArmed(true);
        const bool armed = p.Active();
        p.SetArmed(false);
        p.SetTrace(true);
        const bool tracing = p.Active();
        p.SetTrace(false);
        p.SetLever(90, FamilyRule::Props, false);
        const bool lever = p.Active();
        p.LeverOff();
        check("active/only_when_armed_tracing_or_lever_on", idle && armed && tracing && lever && !p.Active());
        int lines = 0;
        for (int k = 0; k < 1000; ++k) if (p.TakeTraceLine()) ++lines;
        check("trace/at_most_600_lines", lines == kTraceMaxLines && kTraceMaxLines == 600 && !p.TakeTraceLine(),
            "lines=" + std::to_string(lines));
    }

    // ---- an unresolved family, or no callback, answers for nothing --------
    {
        Probe p;
        p.SetFamilies(-1, -1);
        p.SetLever(90, FamilyRule::Props, false);
        p.OnJumpEntry(10, true);
        const bool none = ask(p, Builtin::PositionMeeting, 11, true, Fence) == Answer::RunOriginal;
        Probe q;
        q.SetFamilies(CollisionProp, CollisionParent);
        q.SetLever(90, FamilyRule::Props, false);
        q.OnJumpEntry(10, true);
        const bool noCallback = ask(q, Builtin::PositionMeeting, 11, true, Fence) == Answer::RunOriginal
            && ask(q, Builtin::PositionMeeting, 11, true, CollisionProp) == Answer::False;
        check("failclosed/unresolved_family_or_no_callback", none && noCallback);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
