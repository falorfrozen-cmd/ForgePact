// Behavioral regression harness for gambapity's decision core
// (GambaPity.hpp, ForgePact #134 phase 6, player build).
//
// The Python runner injects the REAL ForgePact::GambaPity header below. No
// game is touched: a machine is an id and whether its sprite is the destroyed
// one, a ground head is an instance id, and the charm identifier lives in
// ModuleMain.cpp's adapter, never here.
//
// Baseline: off, nothing counts and nothing forces; `off` keeps the count; a
// natural head resets it. Target: an explosion is a live-to-destroyed sprite
// change that, while on, adds one to the count the moment it is seen, and is
// decided once its settle span has passed; the explosion whose own addition
// reached the threshold, with no head signal, forces, a confirmed force resets
// and a refused one keeps the count; below the threshold, or abandoned by a
// room change, it keeps the count; a new ground head, a head build in the
// look-back or settle span, or a machine-self (0, 98) build makes it natural
// at any count; two machines in one span force at most once; the counter file
// is version 2 and an older spin file reads as 0; every line is fixed text.
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// PRODUCTION_GAMBAPITY

using namespace ForgePact::GambaPity;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static const std::vector<int64_t> kNoHeads;

// An armed core in room 7 with a count of `count` explosions and machine 100
// seen live at frame 0, with an empty ground baseline.
static Pity Armed(int threshold, int count)
{
    Pity p;
    p.SetThreshold(threshold);
    p.SetEnabled(true);
    p.SetCount(count);
    p.OnRoom(7);
    p.ObserveMachine(100, false, 0, 320.0, 480.0);
    p.SetBaseline(100, kNoHeads);
    return p;
}

// Explode machine `id` at `frame` and return the one due explosion at its
// deadline (an empty id when none was due).
static Explosion ExplodeAndWait(Pity& p, int64_t id, int64_t frame)
{
    p.ObserveMachine(id, true, frame, 320.0, 480.0);
    std::vector<Explosion> due = p.TakeDue(frame + kSettleFrames);
    return due.size() == 1 ? due[0] : Explosion();
}

int main()
{
    // ---- the constants -------------------------------------------------------
    {
        check("table/settle_lookback_radius", kSettleFrames == 60 && kLookBackFrames == 30 && kGroundRadius == 256.0);
        check("table/counter_file_version", kCounterFileVersion == 2);
    }

    // ---- baseline: off, nothing happens -------------------------------------
    {
        Pity p;
        p.SetThreshold(10);
        p.OnNaturalDrop();
        check("baseline/off_a_natural_drop_leaves_the_count", p.Count() == 0);
        check("baseline/off_status", p.StatusLine() == "gambapity: off count=0 threshold=10 explosions=0 forced=0"
            " natural=0 below=0 refused=0 abandoned=0 own-head-builds=0"
              " machines=0 unread=0 ground-unread=0 below-ground-unread=0", p.StatusLine());
    }
    {
        // Off at the deadline: the explosion is never forced, even past the
        // threshold (it counted, since it was seen while on).
        Pity p = Armed(10, 12);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        p.SetEnabled(false);
        std::vector<Explosion> due = p.TakeDue(100 + kSettleFrames);
        const Decision d = due.size() == 1 ? p.Decide(due[0], 7, kNoHeads) : Decision{ Outcome::Force, "" };
        check("baseline/off_an_explosion_never_forces", d.outcome == Outcome::Below && p.Count() == 13, p.StatusLine());
    }
    {
        // `off` drops the machine records and any pending explosion.
        Pity p = Armed(10, 12);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        p.Off();
        check("baseline/off_clears_pending_explosions", p.Pending() == 0 && p.TakeDue(1000).empty() && p.Count() == 13);
    }
    {
        // An explosion while off does not count, and never forces later.
        Pity p = Armed(1, 3);
        p.SetEnabled(false);
        const Sighting s = p.ObserveMachine(100, true, 100, 320.0, 480.0);
        const bool kept = p.Count() == 3;
        p.SetEnabled(true);
        std::vector<Explosion> due = p.TakeDue(100 + kSettleFrames);
        const Decision d = due.size() == 1 ? p.Decide(due[0], 7, kNoHeads) : Decision{ Outcome::Force, "" };
        check("baseline/an_explosion_while_off_does_not_count", s == Sighting::Exploded && kept
              && d.outcome == Outcome::Below && p.Count() == 3, p.StatusLine());
    }

    // ---- the counter: an explosion counts one, at detection -----------------
    {
        Pity p = Armed(10, 3);
        const Sighting s = p.ObserveMachine(100, true, 100, 320.0, 480.0);
        check("counter/an_explosion_counts_one_at_detection", s == Sighting::Exploded && p.Count() == 4 && p.Pending() == 1
              && p.ExplosionLine(100, 100) == "gambapity: explosion id=100 count=4 threshold=10 frame=100",
              p.ExplosionLine(100, 100));
        p.OnNaturalDrop();
        check("counter/a_natural_drop_resets", p.Count() == 0);
        p.SetCount(2);
        p.Off();
        check("counter/off_keeps_the_counter", p.Count() == 2 && !p.Enabled(), p.StatusLine());
    }
    {
        // First sight of a destroyed machine, a machine seen again live, and a
        // machine that vanishes count nothing.
        Pity p = Armed(10, 3);
        p.ObserveMachine(200, true, 1, 0.0, 0.0);
        p.ObserveMachine(200, true, 2, 0.0, 0.0);
        p.ObserveMachine(100, false, 3, 320.0, 480.0);
        check("counter/first_sight_and_a_vanished_machine_count_nothing", p.Count() == 3 && p.Explosions() == 0
              && p.TakeDue(10000).empty(), p.StatusLine());
    }

    // ---- threshold 2 from 0: the second explosion forces ---------------------
    {
        Pity p = Armed(2, 0);
        p.ObserveMachine(101, false, 0, 900.0, 480.0);
        p.SetBaseline(101, kNoHeads);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        const std::string firstLine = p.ExplosionLine(100, 100);
        std::vector<Explosion> due = p.TakeDue(100 + kSettleFrames);
        const Decision first = due.size() == 1 ? p.Decide(due[0], 7, kNoHeads) : Decision{ Outcome::Force, "" };
        const std::string belowLine = p.BelowLine();
        const bool below = first.outcome == Outcome::Below && p.Count() == 1
            && firstLine == "gambapity: explosion id=100 count=1 threshold=2 frame=100"
            && belowLine == "gambapity: explosion below the threshold (count=1 threshold=2); counter kept";
        check("target/threshold_2_from_0_the_first_explosion_is_below_at_count_1", below, firstLine + " | " + belowLine);
        p.ObserveMachine(101, true, 300, 900.0, 480.0);
        const std::string secondLine = p.ExplosionLine(101, 300);
        due = p.TakeDue(300 + kSettleFrames);
        const Decision second = due.size() == 1 ? p.Decide(due[0], 7, kNoHeads) : Decision();
        const bool forces = second.outcome == Outcome::Force && p.Count() == 2
            && secondLine == "gambapity: explosion id=101 count=2 threshold=2 frame=300";
        p.ForceConfirmed(due[0], 9001);
        check("target/threshold_2_from_0_the_second_explosion_forces_and_a_confirmed_force_leaves_0",
              below && forces && p.Count() == 0 && p.Forced() == 1 && p.Below() == 1 && p.Explosions() == 2,
              secondLine + " | " + p.StatusLine());
    }
    {
        // Threshold 1: every explosion without a head forces.
        Pity p = Armed(1, 0);
        int forces = 0;
        for (int64_t id = 100; id < 103; ++id) {
            if (id != 100) {
                p.ObserveMachine(id, false, id * 1000 - 50, 0.0, 0.0);
                p.SetBaseline(id, kNoHeads);
            }
            const Explosion e = ExplodeAndWait(p, id, id * 1000);
            if (p.Decide(e, 7, kNoHeads).outcome == Outcome::Force) {
                ++forces;
                p.ForceConfirmed(e, -1);
            }
        }
        check("target/threshold_1_forces_every_explosion_without_a_head", forces == 3 && p.Forced() == 3 && p.Count() == 0
              && p.Below() == 0, p.StatusLine());
    }

    // ---- the machine watch --------------------------------------------------
    {
        Pity p;
        p.SetEnabled(true);
        p.OnRoom(7);
        const Sighting first = p.ObserveMachine(200, true, 0, 0.0, 0.0);
        const Sighting again = p.ObserveMachine(200, true, 1, 0.0, 0.0);
        check("target/first_sight_destroyed_is_not_an_explosion",
              first == Sighting::FirstSeen && again == Sighting::None && p.Explosions() == 0 && p.Pending() == 0);
    }
    {
        Pity p = Armed(10, 3);
        const Sighting live = p.ObserveMachine(100, false, 50, 320.0, 480.0);
        const Sighting boom = p.ObserveMachine(100, true, 100, 320.0, 480.0);
        const Sighting still = p.ObserveMachine(100, true, 101, 320.0, 480.0);
        const bool early = p.TakeDue(100 + kSettleFrames - 1).empty();
        std::vector<Explosion> due = p.TakeDue(100 + kSettleFrames);
        check("target/live_to_destroyed_is_one_explosion_decided_after_the_settle_span",
              live == Sighting::None && boom == Sighting::Exploded && still == Sighting::None && early
              && due.size() == 1 && due[0].id == 100 && due[0].frame == 100 && due[0].x == 320.0 && due[0].y == 480.0
              && p.Explosions() == 1 && p.Pending() == 0);
    }
    {
        // A machine that vanishes is never read again: no explosion.
        Pity p = Armed(10, 12);
        check("target/a_vanished_machine_is_not_an_explosion", p.TakeDue(10000).empty() && p.Explosions() == 0);
    }

    // ---- the decision -------------------------------------------------------
    {
        // The explosion that brings the count from 9 to 10 forces.
        Pity p = Armed(10, 9);
        const Explosion e = ExplodeAndWait(p, 100, 100);
        const Decision d = p.Decide(e, 7, kNoHeads);
        check("target/at_the_threshold_with_no_signal_the_decision_is_force", e.id == 100 && e.countAfter == 10
              && d.outcome == Outcome::Force && p.Count() == 10, p.StatusLine());
        p.ForceConfirmed(e, 9001);
        check("target/a_confirmed_force_resets_the_counter", p.Count() == 0 && p.Forced() == 1, p.StatusLine());
    }
    {
        Pity p = Armed(10, 9);
        const Explosion e = ExplodeAndWait(p, 100, 100);
        const Decision d = p.Decide(e, 7, kNoHeads);
        p.ForceRefused();
        bool kept = d.outcome == Outcome::Force && p.Count() == 10 && p.Refused() == 1 && p.Forced() == 0;
        // The next explosion, on another machine, forces again.
        p.ObserveMachine(101, false, 200, 0.0, 0.0);
        p.SetBaseline(101, kNoHeads);
        const Explosion next = ExplodeAndWait(p, 101, 300);
        const Decision d2 = p.Decide(next, 7, kNoHeads);
        check("target/a_refused_force_keeps_the_counter_and_the_next_explosion_forces",
              kept && next.id == 101 && next.countAfter == 11 && d2.outcome == Outcome::Force && p.Count() == 11, p.StatusLine());
    }
    {
        Pity p = Armed(10, 8);
        const Explosion e = ExplodeAndWait(p, 100, 100);
        const Decision d = p.Decide(e, 7, kNoHeads);
        check("target/below_the_threshold_the_counter_is_kept",
              d.outcome == Outcome::Below && p.Count() == 9 && p.Below() == 1, p.StatusLine());
    }
    {
        // A zero threshold is the unset state: never a force.
        Pity p = Armed(0, 12);
        const Explosion e = ExplodeAndWait(p, 100, 100);
        check("target/a_zero_threshold_never_forces", p.Decide(e, 7, kNoHeads).outcome == Outcome::Below);
    }

    // ---- the natural-head signals ------------------------------------------
    {
        // A ground head not in the baseline: natural, at any count, and the
        // count goes to 0 with this explosion's own addition.
        Pity above = Armed(10, 12);
        const Decision a = above.Decide(ExplodeAndWait(above, 100, 100), 7, { 555 });
        Pity below = Armed(10, 3);
        const Decision b = below.Decide(ExplodeAndWait(below, 100, 100), 7, { 555 });
        Pity reached = Armed(2, 1);
        const Explosion r = ExplodeAndWait(reached, 100, 100);
        const Decision c = reached.Decide(r, 7, { 555 });
        check("target/a_new_ground_head_is_natural_at_any_count",
              a.outcome == Outcome::Natural && a.signal == "ground" && above.Count() == 0 && above.Natural() == 1
              && b.outcome == Outcome::Natural && below.Count() == 0 && below.Forced() == 0
              && r.countAfter == 2 && c.outcome == Outcome::Natural && reached.Count() == 0 && reached.Forced() == 0);
    }
    {
        // A head already lying near the machine at first sight is not the explosion's.
        Pity p;
        p.SetThreshold(10);
        p.SetEnabled(true);
        p.SetCount(12);
        p.OnRoom(7);
        p.ObserveMachine(100, false, 0, 320.0, 480.0);
        p.SetBaseline(100, { 555 });
        const bool counted = p.NewHeads(100, { 555 }) == 0 && p.NewHeads(100, { 555, 556 }) == 1;
        const Decision d = p.Decide(ExplodeAndWait(p, 100, 100), 7, { 555 });
        check("target/a_baseline_head_is_not_natural", counted && d.outcome == Outcome::Force && p.Natural() == 0);
    }
    {
        // A head build in the look-back (before the sprite change) is the explosion's.
        Pity p = Armed(10, 12);
        p.OnHeadBuild(100 - kLookBackFrames);
        const Decision d = p.Decide(ExplodeAndWait(p, 100, 100), 7, kNoHeads);
        check("target/a_head_build_in_the_look_back_is_natural",
              d.outcome == Outcome::Natural && d.signal == "build" && p.Count() == 0, p.StatusLine());
    }
    {
        // A head build inside the settle span is the explosion's, at any count.
        Pity p = Armed(10, 4);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        p.OnHeadBuild(100 + kSettleFrames);
        std::vector<Explosion> due = p.TakeDue(100 + kSettleFrames);
        const Decision d = due.size() == 1 ? p.Decide(due[0], 7, kNoHeads) : Decision();
        check("target/a_head_build_in_the_settle_span_is_natural",
              d.outcome == Outcome::Natural && d.signal == "build" && p.Count() == 0, p.StatusLine());
    }
    {
        // A head build outside every explosion span changes nothing.
        Pity p = Armed(10, 9);
        p.OnHeadBuild(10);                                   // long before
        const Decision d = p.Decide(ExplodeAndWait(p, 100, 100), 7, kNoHeads);
        p.OnHeadBuild(500);                                  // long after
        check("target/a_head_build_outside_every_span_does_not_reset",
              d.outcome == Outcome::Force && p.Count() == 10 && p.Natural() == 0, p.StatusLine());
    }
    {
        // A machine-self (0, 98) build resets at once and makes the explosion natural.
        Pity p = Armed(10, 12);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        const bool counted = p.Count() == 13;
        p.OnMachineCharmBuild(110);
        const bool reset = counted && p.Count() == 0;
        std::vector<Explosion> due = p.TakeDue(100 + kSettleFrames);
        const Decision d = due.size() == 1 ? p.Decide(due[0], 7, kNoHeads) : Decision();
        check("target/a_machine_build_resets_at_once_and_makes_the_explosion_natural",
              reset && d.outcome == Outcome::Natural && d.signal == "machine-build" && p.Natural() == 1);
    }
    {
        // Our own forced drop's build is counted, never a signal; and our own
        // head on the ground is not natural for a later explosion beside it.
        Pity p = Armed(10, 12);
        const Explosion e = ExplodeAndWait(p, 100, 100);
        const Decision d = p.Decide(e, 7, kNoHeads);
        p.BeginOwnDrop();
        const bool own = p.OnHeadBuild(160);
        p.EndOwnDrop();
        p.ForceConfirmed(e, 9001);
        const bool counted = d.outcome == Outcome::Force && own && p.OwnHeadBuilds() == 1 && !p.InOwnDrop();
        p.SetCount(12);
        p.ObserveMachine(101, false, 170, 330.0, 480.0);
        p.SetBaseline(101, kNoHeads);
        const Decision d2 = p.Decide(ExplodeAndWait(p, 101, 200), 7, { 9001 });
        check("target/an_own_drop_build_counts_as_own_head_builds_not_a_signal",
              counted && d2.outcome == Outcome::Force && p.Natural() == 0, p.StatusLine());
    }

    {
        // Our own earlier forced head, lying near a second machine that was
        // seen before it landed, is not that machine's natural head; a head
        // that is not ours there still is (the negative control).
        Pity p = Armed(10, 12);
        p.ObserveMachine(101, false, 50, 400.0, 480.0);
        p.SetBaseline(101, kNoHeads);
        const Explosion pe = ExplodeAndWait(p, 100, 100);
        p.Decide(pe, 7, kNoHeads);
        p.ForceConfirmed(pe, 9001);
        p.SetCount(12);
        const Decision ours = p.Decide(ExplodeAndWait(p, 101, 300), 7, { 9001 });
        Pity q = Armed(10, 12);
        q.ObserveMachine(101, false, 50, 400.0, 480.0);
        q.SetBaseline(101, kNoHeads);
        const Explosion qe = ExplodeAndWait(q, 100, 100);
        q.Decide(qe, 7, kNoHeads);
        q.ForceConfirmed(qe, 9001);
        const Decision theirs = q.Decide(ExplodeAndWait(q, 101, 300), 7, { 9001, 9002 });
        check("target/our_earlier_forced_head_near_a_second_machine_is_not_natural",
              ours.outcome == Outcome::Force && p.Natural() == 0
              && theirs.outcome == Outcome::Natural && theirs.signal == "ground" && q.Natural() == 1, p.StatusLine());
    }

    // ---- an unread ground never forces --------------------------------------
    {
        // The scan at the deadline did not read: refused, counter kept, counted;
        // the next explosion, read, forces.
        Pity p = Armed(10, 9);
        const Decision d = p.Decide(ExplodeAndWait(p, 100, 100), 7, kNoHeads, false);
        const bool refused = d.outcome == Outcome::GroundUnread && p.Count() == 10 && p.Refused() == 1
            && p.GroundUnread() == 1 && p.Forced() == 0;
        p.ObserveMachine(101, false, 150, 0.0, 0.0);
        p.SetBaseline(101, kNoHeads);
        const Decision next = p.Decide(ExplodeAndWait(p, 101, 300), 7, kNoHeads, true);
        check("target/an_unread_ground_scan_refuses_the_force_and_keeps_the_counter",
              refused && next.outcome == Outcome::Force && p.Count() == 11, p.StatusLine());
    }
    {
        // The machine's first-sight scan did not read: its baseline is unknown,
        // so its explosion is never forced, even with the deadline's scan read.
        Pity p;
        p.SetThreshold(10);
        p.SetEnabled(true);
        p.SetCount(9);
        p.OnRoom(7);
        p.ObserveMachine(100, false, 0, 320.0, 480.0);
        p.SetBaseline(100, kNoHeads, false);
        const Decision d = p.Decide(ExplodeAndWait(p, 100, 100), 7, { 555 }, true);
        check("target/an_unread_baseline_refuses_the_force", d.outcome == Outcome::GroundUnread && p.Count() == 10
              && p.Natural() == 0 && p.GroundUnread() == 1, p.StatusLine());
    }
    {
        // Below the threshold an unread ground changes nothing; a head build in
        // the window is still natural with the ground unread.
        Pity below = Armed(10, 3);
        const Decision b = below.Decide(ExplodeAndWait(below, 100, 100), 7, kNoHeads, false);
        Pity built = Armed(10, 12);
        built.OnHeadBuild(95);
        const Decision n = built.Decide(ExplodeAndWait(built, 100, 100), 7, kNoHeads, false);
        check("target/an_unread_ground_below_the_threshold_is_below_and_a_build_is_still_natural",
              b.outcome == Outcome::Below && below.GroundUnread() == 0 && b.groundUnread && below.BelowGroundUnread() == 1
              && n.outcome == Outcome::Natural && n.signal == "build" && built.Count() == 0);
    }
    {
        // A first-sight scan that did not read is retried while the machine is
        // live, once per kBaselineRetryFrames; the first retry that reads sets
        // the baseline, and the explosion then forces.
        Pity p;
        p.SetThreshold(10);
        p.SetEnabled(true);
        p.SetCount(12);
        p.OnRoom(7);
        p.ObserveMachine(100, false, 0, 320.0, 480.0);
        p.SetBaseline(100, kNoHeads, false);
        const bool tooSoon = !p.NeedsBaseline(100, kBaselineRetryFrames - 1);
        const bool first = p.NeedsBaseline(100, kBaselineRetryFrames);
        p.SetBaseline(100, kNoHeads, false);                      // that retry did not read either
        const bool throttled = !p.NeedsBaseline(100, kBaselineRetryFrames + 1);
        const bool second = p.NeedsBaseline(100, 2 * kBaselineRetryFrames);
        p.SetBaseline(100, { 555 }, true);                        // this one read
        const bool done = p.BaselineRead(100) && !p.NeedsBaseline(100, 10 * kBaselineRetryFrames);
        p.SetBaseline(100, kNoHeads, false);                      // a later unread scan never undoes it
        const Decision d = p.Decide(ExplodeAndWait(p, 100, 400), 7, { 555 }, true);
        check("target/an_unread_first_sight_then_a_retry_that_reads_then_a_force",
              tooSoon && first && throttled && second && done && p.BaselineRead(100) && d.outcome == Outcome::Force
              && p.GroundUnread() == 0, p.StatusLine());
        // Never after the explosion: a scan then could take the explosion's
        // own head for the baseline.
        Pity q;
        q.SetEnabled(true);
        q.OnRoom(7);
        q.ObserveMachine(200, false, 0, 0.0, 0.0);
        q.SetBaseline(200, kNoHeads, false);
        q.ObserveMachine(200, true, 10, 0.0, 0.0);
        check("target/no_baseline_retry_after_the_explosion", !q.NeedsBaseline(200, 10 * kBaselineRetryFrames)
              && !q.NeedsBaseline(999, 10 * kBaselineRetryFrames));
    }
    {
        // Machines seen and machine reads skipped are counted in status.
        Pity p = Armed(10, 0);
        p.ObserveMachine(101, false, 1, 0.0, 0.0);
        p.ObserveMachine(101, false, 2, 0.0, 0.0);
        p.NoteMachineUnread();
        p.NoteMachineUnread();
        check("counter/machines_seen_and_unread_reads_are_counted", p.MachinesSeen() == 2 && p.MachinesUnread() == 2
              && p.StatusLine().find(" machines=2 unread=2 ground-unread=0 below-ground-unread=0") != std::string::npos, p.StatusLine());
    }

    // ---- the room -----------------------------------------------------------
    {
        // An abandoned explosion still counts; it had reached the threshold,
        // so the next explosion, in the new room, forces.
        Pity p = Armed(10, 9);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        const bool changed = p.OnRoom(8);
        std::vector<Explosion> due = p.TakeDue(100 + kSettleFrames);
        const Decision d = due.size() == 1 ? p.Decide(due[0], 8, kNoHeads) : Decision();
        check("target/a_room_change_abandons_a_pending_explosion_and_keeps_the_counter",
              changed && d.outcome == Outcome::Abandoned && p.Count() == 10 && p.Abandoned() == 1 && p.Forced() == 0);
        // The machine records went with the room: a destroyed machine seen
        // now is a first sight, not a second explosion.
        check("target/a_room_change_clears_the_machine_records",
              p.ObserveMachine(100, true, 200, 0.0, 0.0) == Sighting::FirstSeen && p.Explosions() == 1 && p.Count() == 10);
        p.ObserveMachine(300, false, 210, 0.0, 0.0);
        p.SetBaseline(300, kNoHeads);
        const Decision next = p.Decide(ExplodeAndWait(p, 300, 400), 8, kNoHeads);
        check("target/an_abandoned_explosion_still_counts_and_the_next_one_forces",
              next.outcome == Outcome::Force && p.Count() == 11, p.StatusLine());
        // Below the threshold, the abandoned explosion's addition is kept too.
        Pity q = Armed(3, 0);
        q.ObserveMachine(100, true, 100, 320.0, 480.0);
        q.OnRoom(8);
        due = q.TakeDue(100 + kSettleFrames);
        const Decision b = due.size() == 1 ? q.Decide(due[0], 8, kNoHeads) : Decision();
        check("target/an_abandoned_explosion_below_the_threshold_still_counts",
              b.outcome == Outcome::Abandoned && q.Count() == 1, q.StatusLine());
    }

    // ---- two machines, one span ---------------------------------------------
    {
        // Threshold 2 from 0: the first explosion brings the count to 1, the
        // second to 2. The second forces; the first, still pending when the
        // second raised the count, is below.
        Pity p = Armed(2, 0);
        p.ObserveMachine(101, false, 0, 900.0, 480.0);
        p.SetBaseline(101, kNoHeads);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        p.ObserveMachine(101, true, 110, 900.0, 480.0);
        std::vector<Explosion> due = p.TakeDue(110 + kSettleFrames);
        int forces = 0;
        int64_t forcedId = -1;
        for (const Explosion& e : due) {
            if (p.Decide(e, 7, kNoHeads).outcome == Outcome::Force) {
                ++forces;
                forcedId = e.id;
                p.ForceConfirmed(e, -1);
            }
        }
        check("target/two_machines_in_one_settle_span_force_at_most_once",
              due.size() == 2 && forces == 1 && forcedId == 101 && p.Forced() == 1 && p.Below() == 1 && p.Count() == 0,
              p.StatusLine());
        // From 1: the first brings the count to 2 and forces; its reset takes
        // its own addition and the one before it, and keeps the second's, so
        // the count is 1, and the second, pending when the force was
        // confirmed, is below.
        Pity q = Armed(2, 1);
        q.ObserveMachine(101, false, 0, 900.0, 480.0);
        q.SetBaseline(101, kNoHeads);
        q.ObserveMachine(100, true, 100, 320.0, 480.0);
        q.ObserveMachine(101, true, 110, 900.0, 480.0);
        due = q.TakeDue(110 + kSettleFrames);
        forces = 0;
        forcedId = -1;
        for (const Explosion& e : due) {
            if (q.Decide(e, 7, kNoHeads).outcome == Outcome::Force) {
                ++forces;
                forcedId = e.id;
                q.ForceConfirmed(e, -1);
            }
        }
        check("target/the_explosion_that_reached_the_threshold_forces_and_the_other_is_below",
              due.size() == 2 && forces == 1 && forcedId == 100 && q.Forced() == 1 && q.Below() == 1 && q.Count() == 1,
              q.StatusLine());
    }

    {
        // Threshold 1, three explosions in one span: one head, and the two
        // later additions stay, so the count is 2. Explosions pending when a
        // force was confirmed never force; the next explosion, added after
        // it, does, and its reset takes every addition before it.
        Pity p = Armed(1, 0);
        p.ObserveMachine(101, false, 0, 600.0, 480.0);
        p.SetBaseline(101, kNoHeads);
        p.ObserveMachine(102, false, 0, 900.0, 480.0);
        p.SetBaseline(102, kNoHeads);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        p.ObserveMachine(101, true, 105, 600.0, 480.0);
        p.ObserveMachine(102, true, 110, 900.0, 480.0);
        std::vector<Explosion> due = p.TakeDue(110 + kSettleFrames);
        int forces = 0;
        int64_t forcedId = -1;
        for (const Explosion& e : due) {
            if (p.Decide(e, 7, kNoHeads).outcome == Outcome::Force) {
                ++forces;
                forcedId = e.id;
                p.ForceConfirmed(e, -1);
            }
        }
        check("target/three_explosions_in_one_span_at_threshold_1_make_one_head_and_leave_2",
              due.size() == 3 && forces == 1 && forcedId == 100 && p.Forced() == 1 && p.Below() == 2 && p.Count() == 2,
              p.StatusLine());
        p.ObserveMachine(103, false, 300, 0.0, 0.0);
        p.SetBaseline(103, kNoHeads);
        const Explosion next = ExplodeAndWait(p, 103, 400);
        const Decision d = p.Decide(next, 7, kNoHeads);
        p.ForceConfirmed(next, -1);
        check("target/explosions_pending_together_force_at_most_once_and_a_later_one_forces",
              d.outcome == Outcome::Force && p.Forced() == 2 && p.Count() == 0, p.StatusLine());
    }
    {
        // A natural head at an earlier explosion takes its addition and every
        // one before it, and keeps a later pending explosion's.
        Pity p = Armed(10, 3);
        p.ObserveMachine(101, false, 0, 2000.0, 480.0);
        p.SetBaseline(101, kNoHeads);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        p.ObserveMachine(101, true, 110, 2000.0, 480.0);
        std::vector<Explosion> due = p.TakeDue(110 + kSettleFrames);
        const bool counted = p.Count() == 5;
        const Decision a = due.size() == 2 ? p.Decide(due[0], 7, { 555 }) : Decision();
        const bool kept = p.Count() == 1;
        const Decision b = due.size() == 2 ? p.Decide(due[1], 7, kNoHeads) : Decision();
        check("target/a_natural_head_keeps_a_later_pending_explosions_addition",
              counted && a.outcome == Outcome::Natural && kept && b.outcome == Outcome::Below && p.Count() == 1, p.StatusLine());
        // A machine-self (0, 98) build has no explosion of its own: every
        // addition goes, a pending one's included.
        Pity q = Armed(10, 3);
        q.ObserveMachine(100, true, 100, 320.0, 480.0);
        q.OnMachineCharmBuild(500);
        due = q.TakeDue(100 + kSettleFrames);
        const Decision c = due.size() == 1 ? q.Decide(due[0], 7, kNoHeads) : Decision();
        check("target/a_machine_build_resets_every_addition_to_0",
              c.outcome == Outcome::Below && q.Count() == 0, q.StatusLine());
    }

    // ---- the counter file ---------------------------------------------------
    {
        const std::string text = CounterFileText(7);
        const CounterFile seven = ParseCounterFile(text);
        const CounterFile zero = ParseCounterFile(CounterFileText(0));
        check("file/version_2_round_trips_byte_for_byte", text == "{\"version\":2,\"count\":7}"
              && CounterFileText(0) == "{\"version\":2,\"count\":0}" && CounterFileText(seven.count) == text
              && seven.count == 7 && !seven.legacy && !seven.unknown && zero.count == 0 && !zero.legacy && !zero.unknown, text);
        const CounterFile legacy = ParseCounterFile("{\"count\":12}");
        check("file/a_legacy_spin_file_parses_to_0_flagged_legacy", legacy.count == 0 && legacy.legacy && !legacy.unknown);
        const CounterFile v3 = ParseCounterFile("{\"version\":3,\"count\":5}");
        check("file/an_unknown_version_parses_to_0_flagged_with_its_number",
              v3.count == 0 && v3.unknown && v3.version == 3 && !v3.legacy);
        bool quiet = true;
        for (const char* bad : { "", "   ", "garbage", "{", "{}", "{\"version\":2,\"count\":}", "{\"count\":\"x\"}",
                                 "{\"version\":\"two\",\"count\":4}", "[\"count\",3]", "{\"count\":12" }) {
            const CounterFile f = ParseCounterFile(bad);
            if (f.count != 0 || f.legacy || f.unknown) quiet = false;
        }
        check("file/empty_or_malformed_parses_to_0_unflagged", quiet);
        check("lines/migration", Pity::MigrationLine()
              == "gambapity: the counter file held a spin count from an older version; the explosion count starts at 0");
        check("lines/version_error", Pity::VersionErrorText(3)
              == "the gambapity counter file has version 3, which this build does not read; the explosion count starts at 0");
    }

    // ---- the lines, byte for byte -------------------------------------------
    {
        Pity p = Armed(10, 0);
        p.ObserveMachine(100, true, 100, 320.0, 480.0);
        check("status/line_names_every_counter",
              p.StatusLine() == "gambapity: on count=1 threshold=10 explosions=1 forced=0 natural=0"
              " below=0 refused=0 abandoned=0 own-head-builds=0"
              " machines=1 unread=0 ground-unread=0 below-ground-unread=0", p.StatusLine());
        p.Off();
        check("status/off_keeps_the_count_in_the_line",
              p.StatusLine() == "gambapity: off count=1 threshold=10 explosions=1 forced=0 natural=0"
              " below=0 refused=0 abandoned=0 own-head-builds=0"
              " machines=1 unread=0 ground-unread=0 below-ground-unread=0", p.StatusLine());
    }
    {
        Pity p = Armed(10, 11);
        p.ObserveMachine(100, true, 4321, 320.0, 480.0);
        std::vector<Explosion> due = p.TakeDue(4321 + kSettleFrames);
        const Explosion e = due.size() == 1 ? due[0] : Explosion();
        check("lines/machine_seen", Pity::MachineSeenLine(100, "Slot_Machine_01_spr", "0")
              == "gambapity: machine id=100 seen sprite=Slot_Machine_01_spr heads-nearby=0");
        check("lines/explosion", p.ExplosionLine(e.id, e.frame) == "gambapity: explosion id=100 count=12 threshold=10 frame=4321",
              p.ExplosionLine(e.id, e.frame));
        check("lines/forced", Pity::ForcedLine(320.4, 479.6, 6, 1)
              == "gambapity: forced Goburin's Head at 320,480 (rarity 6, attempt 1) and reset the counter",
              Pity::ForcedLine(320.4, 479.6, 6, 1));
        check("lines/ground_after_drop", Pity::GroundAfterDropLine(1) == "gambapity: ground check after the drop: heads=1");
        check("lines/below_ground_unread", p.BelowLine("the scan threw")
              == "gambapity: explosion below the threshold (count=12 threshold=10); counter kept; ground unread (the scan threw)",
              p.BelowLine("the scan threw"));
        check("lines/baseline_read", Pity::BaselineReadLine(7, 1) == "gambapity: machine id=7 baseline read heads-nearby=1");
        check("lines/ground_after_drop_unread", Pity::GroundAfterDropUnreadLine("the scan threw")
              == "gambapity: ground check after the drop: unread (the scan threw)");
        check("lines/machine_seen_unread", Pity::MachineSeenLine(7, "Slot_Machine_01_spr", "unread (the scan threw)")
              == "gambapity: machine id=7 seen sprite=Slot_Machine_01_spr heads-nearby=unread (the scan threw)");
        check("lines/natural_seen", Pity::NaturalSeenLine("ground")
              == "gambapity: the explosion's own Goburin's Head was seen (ground); no force, counter reset");
        p.SetCount(3);
        check("lines/below", p.BelowLine() == "gambapity: explosion below the threshold (count=3 threshold=10); counter kept");
        check("lines/refused", Pity::RefusedLine("no local player")
              == "gambapity: forced drop refused - no local player; counter kept");
        check("lines/abandoned", Pity::AbandonedLine(e.id) == "gambapity: explosion id=100 abandoned (room changed); counter kept");
        check("lines/natural_build", Pity::NaturalBuildLine() == "gambapity: a natural Goburin's Head build reset the counter");
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
