// Behavioral regression harness for the Mark 1 set's cast priority
// (Mark1Priority.hpp), and for the writes Mark1Automation.hpp makes from it.
//
// The Python runner injects the REAL ForgePact::Mark1PriorityCore and
// ForgePact::Mark1Automation code below, and the interval cases of the hub's
// hs-game-sdk/curated/mark1_automation_cases.json. Nothing of the game is
// touched: the core is asked directly with the columns a gate call would
// hand it, and the automation runs against a controlled bar (bar columns 3 to
// 6: talent id, aura, toggle, cooldown) and a controlled `inputState` array
// (the profile slot's entries), so every scenario sees which entries were
// written, with what, and that each was put back.
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// PRODUCTION_MARK1PRIORITY

using namespace ForgePact;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}
static std::string num(double v) { char b[64]; std::snprintf(b, sizeof(b), "%.6g", v); return b; }

// ---- the core alone ---------------------------------------------------------------
static Mark1PriorityColumn none() { return Mark1PriorityColumn{}; }
static Mark1PriorityColumn freeCol(double cd = 0.25) {
    Mark1PriorityColumn c; c.kind = Mark1ColumnKind::Free; c.cooldownRead = true; c.abilityCooldown = cd; return c;
}
static Mark1PriorityColumn cdCol(double cd = 14) {
    Mark1PriorityColumn c; c.kind = Mark1ColumnKind::Cooldown; c.cooldownRead = true; c.abilityCooldown = cd; return c;
}
static Mark1PriorityColumn unreadCdCol() {
    Mark1PriorityColumn c; c.kind = Mark1ColumnKind::Cooldown; return c;
}
static Mark1PriorityColumn toggleCol() {
    Mark1PriorityColumn c; c.kind = Mark1ColumnKind::Cooldown; c.toggle = true; c.cooldownRead = true; c.abilityCooldown = 0; return c;
}
struct Cols { Mark1PriorityColumn c[kMark1ColumnCount]; };
static Cols cols(Mark1PriorityColumn q, Mark1PriorityColumn e, Mark1PriorityColumn r, Mark1PriorityColumn y) {
    Cols out; out.c[0] = q; out.c[1] = e; out.c[2] = r; out.c[3] = y; return out;
}
// One gate call as the core sees it: decide, then report whether the press
// landed. Returns the pressed column (0: none).
template <class Lands>
static int step(Mark1PriorityCore& core, long long frame, const Cols& c, Lands&& lands, Mark1PriorityDecision* out = nullptr) {
    const Mark1PriorityDecision d = core.Decide(frame, c.c, Mark1GateClock{});
    if (out) *out = d;
    if (d.pressColumn) core.Landed(d.pressColumn, frame, lands(d.pressColumn, frame), d.pressFree);
    return d.pressColumn;
}
static bool never(int, long long) { return false; }
static std::string seq(const std::vector<int>& v) { std::string s; for (int c : v) s += std::to_string(c); return s; }

// ---- the controlled bar and profile slot (through Mark1Automation) -------------------
struct Column {
    bool readable = true;
    double talentId = 0;
    bool aura = false;
    bool toggle = false;
    Mark1ToggleState state = Mark1ToggleState::Off;
    bool cooldownRead = true;
    double cooldown = 14;
};
struct Bar { std::map<int, Column> columns; long reads = 0; };
struct Slot {
    double entries[16] = {};
    long reads = 0, writes = 0;
    bool writeFails[16] = {};
    std::vector<std::pair<int, double>> writeLog;
};
static Bar bar;
static Slot slot;
static void resetBar() { bar = Bar(); for (int c = 0; c < 10; ++c) bar.columns[c] = Column(); }
static void resetSlot() { slot = Slot(); for (double& e : slot.entries) e = kMark1InputResting; }
static Mark1ColumnRead readColumn(int column) {
    ++bar.reads;
    const Column& c = bar.columns[column];
    Mark1ColumnRead r;
    r.readable = c.readable; r.talentId = c.talentId; r.aura = c.aura; r.toggle = c.toggle;
    r.cooldownRead = c.cooldownRead; r.abilityCooldown = c.cooldown;
    return r;
}
static Mark1ToggleState readToggle(int column) { return bar.columns[column].state; }
static bool readEntry(int entry, double& out) {
    ++slot.reads;
    if (entry < 0 || entry >= 16) return false;
    out = slot.entries[entry];
    return true;
}
static bool writeEntry(int entry, const double& value) {
    ++slot.writes;
    slot.writeLog.push_back({ entry, value });
    if (entry < 0 || entry >= 16 || slot.writeFails[entry]) return false;
    slot.entries[entry] = value;
    return true;
}
// One `CheckTalentUse` call through the automation: plan, the writes around a
// stand-in original that records what it saw, the landing, and (on the proc
// route) the proc reported as returned.
struct Call {
    Mark1FramePlan plan;
    double seen[16] = {};
};
template <class Lands>
static Call gate(Mark1Automation& a, bool active, long long frame, const Mark1GateClock& clock, Lands&& lands) {
    Call c;
    c.plan = a.Plan(active, frame, clock, readColumn, readToggle);
    bool landed = false;
    a.PressAround<double>(c.plan, readEntry, writeEntry, [&]() -> double {
        for (int i = 0; i < 16; ++i) c.seen[i] = slot.entries[i];
        landed = lands(c.plan);
        return 7.0;
    });
    a.Landing(c.plan, landed);
    if (c.plan.procColumn) a.ProcDone(c.plan, Mark1ProcOutcome::Returned);
    return c;
}
static bool noLanding(const Mark1FramePlan&) { return false; }
static Mark1GateClock at(long long ms, double haste = 0, double fcr = 0) {
    Mark1GateClock k; k.haveMs = true; k.nowMs = ms; k.haste = haste; k.fcr = fcr; return k;
}

// ---- the shared interval cases (injected from the hub's cases file) -----------------
struct IntervalCase { double cooldown, haste, fcr, base, expectedMs; };
static const IntervalCase kIntervalCases[] = {
    // INTERVAL_CASES
};

int main() {
    // ---- constants ---------------------------------------------------------------
    check("constants/values", kMark1YieldFrames == 56 && kMark1ProbeRestFrames == 24 && kMark1FreePressValue == 4.0
        && kMark1FreeCooldownFloor == 0.25 && kMark1AutoRoute == Mark1Route::Press && std::string(kMark1ProcShape).empty()
        && kMark1BaseCastSeconds == 1.0 && kMark1StatRefreshMs == 1000 && kMark1TurnPatienceFrames == 90
        && kMark1TurnRestFrames == 288 && kMark1DefaultMaskEntryCount == 8);
    {
        Mark1PriorityCore core;
        check("constants/settings_start_at_the_constants", core.Settings().holdValue == 4.0 && core.Settings().yieldFrames == 56
            && core.Settings().restFrames == 24 && core.RouteInForce() == Mark1Route::Press && core.Settings().shape.empty()
            && core.Settings().lockout && core.Settings().baseCastSeconds == 1.0
            && Mark1FormatMaskSetting(core.Settings().mask) == "0,1,2,7,8,9,10,11");
    }

    // ---- baseline: the bonus inactive --------------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101; bar.columns[3].cooldown = 0.25;
        bar.columns[4].talentId = 102;
        slot.entries[1] = kMark1InputHeld;           // a held key
        slot.entries[3] = kMark1InputHeld;           // the player holds Q
        Mark1Automation a;
        a.Priority().SetShape(Mark1ParseProcShape("id,x,y"));
        a.Priority().SetRoute(Mark1Route::Proc);
        bool nothing = true;
        for (long long f = 0; f < 300; ++f) {
            const Call c = gate(a, false, f, at(f * 7), [](const Mark1FramePlan&) { return true; });
            if (c.plan.active || c.plan.PressCount() || c.plan.lockMask || c.plan.procColumn || c.plan.NeedsInput()) nothing = false;
            if (c.seen[1] != kMark1InputHeld || c.seen[3] != kMark1InputHeld) nothing = false;
        }
        const Mark1PriorityCounters k = a.Priority().Counters();
        check("baseline/inactive_reads_and_writes_nothing", nothing && bar.reads == 0 && slot.reads == 0 && slot.writes == 0,
            "reads=" + std::to_string(bar.reads) + " entryReads=" + std::to_string(slot.reads));
        check("baseline/inactive_counts_nothing", k.masked == 0 && k.heldSeen == 0 && k.locked == 0 && k.procs == 0
            && k.procFail == 0 && a.Counters(3).presses == 0 && a.Counters(3).landed == 0 && a.Counters(3).skipTurn == 0
            && a.Priority().ProcColumnCount() == 0);
        bool p0 = false, v0 = false;
        bool pressed[kMark1ColumnCount] = { true, false, false, false };
        double value[kMark1ColumnCount] = { 4, 4, 4, 4 };
        const Mark1InputPlan ip = Mark1PlanInputs(false, true, 0xFu, pressed, value, Mark1MaskSetting{});
        p0 = ip.lockCount == 0 && ip.pressCount == 0 && ip.maskCount == 0;
        v0 = a.GreyMask(false, readColumn) == 0u && bar.reads == 0;
        check("baseline/inactive_input_plan_is_empty", p0 && v0);
    }

    // ---- cooldown columns first, in Q, E, R, Y order ------------------------------
    {
        Mark1PriorityCore core;
        const Cols c = cols(freeCol(), cdCol(14), cdCol(8), freeCol(0));
        Mark1PriorityDecision d;
        const int first = step(core, 0, c, never, &d);
        const bool firstNotFree = !d.pressFree && d.why[0] == Mark1PriorityWhy::OtherPressing;
        const int second = step(core, 1, c, never);     // E rests after a press that did not land
        const int third = step(core, 2, c, never);      // E and R rest: a free column
        check("cooldown/first_cooldown_column_before_any_free_column", first == 4 && firstNotFree, "first=" + std::to_string(first));
        check("cooldown/next_cooldown_column_while_the_first_rests", second == 5, "second=" + std::to_string(second));
        check("cooldown/free_column_only_when_every_cooldown_column_rests", third == 3, "third=" + std::to_string(third));

        Mark1PriorityCore all;
        const Cols four = cols(cdCol(), cdCol(), cdCol(), cdCol());
        std::vector<int> order;
        for (long long f = 0; f < 5; ++f) order.push_back(step(all, f, four, never));
        check("cooldown/q_e_r_y_order", seq(order) == "34560", "order=" + seq(order));

        // A cooldown column back from cooldown takes the next call ahead of a
        // free column that is being pressed.
        Mark1PriorityCore back;
        const Cols qr = cols(freeCol(), none(), cdCol(), none());
        std::vector<int> o2;
        for (long long f = 0; f <= kMark1ProbeRestFrames + 1; ++f) o2.push_back(step(back, f, qr, never));
        check("cooldown/back_from_its_rest_before_the_free_column", o2[0] == 5 && o2[1] == 3
            && o2[(size_t)kMark1ProbeRestFrames - 1] == 3 && o2[(size_t)kMark1ProbeRestFrames] == 5, "order=" + seq(o2));
        // An unreadable cooldown is a cooldown column.
        check("cooldown/unreadable_cooldown_is_a_cooldown_column",
            Mark1ClassifyColumn(true, false, false, 0) == Mark1ColumnKind::Cooldown
            && Mark1ClassifyColumn(true, false, true, 0.25) == Mark1ColumnKind::Free
            && Mark1ClassifyColumn(true, false, true, 0.3) == Mark1ColumnKind::Cooldown
            && Mark1ClassifyColumn(true, true, true, 0) == Mark1ColumnKind::Cooldown
            && Mark1ClassifyColumn(false, false, true, 0) == Mark1ColumnKind::NotEligible);
    }

    // ---- a free column is pressed until it lands -----------------------------------
    {
        Mark1PriorityCore core;
        const Cols c = cols(freeCol(), none(), none(), none());
        long pressed = 0;
        for (long long f = 0; f < 60; ++f) if (step(core, f, c, never) == 3) ++pressed;
        const bool noRest = core.Yielded(0) == 0;
        const int landsAt60 = step(core, 60, c, [](int, long long) { return true; });
        const int after = step(core, 61, c, never);
        check("free/pressed_on_every_call_until_it_lands", pressed == 60 && noRest && landsAt60 == 3 && after == 0,
            "pressed=" + std::to_string(pressed) + " after=" + std::to_string(after));

        Mark1PriorityCore p;
        std::vector<int> v;
        for (long long f = 0; f < kMark1TurnPatienceFrames + kMark1TurnRestFrames + 1; ++f) v.push_back(step(p, f, c, never));
        bool restQuiet = true;
        for (long long f = kMark1TurnPatienceFrames; f < kMark1TurnPatienceFrames + kMark1TurnRestFrames; ++f)
            if (v[(size_t)f] != 0) restQuiet = false;
        check("free/patience_then_rest", v[(size_t)kMark1TurnPatienceFrames - 1] == 3 && restQuiet && p.Yielded(0) == 1
            && v[(size_t)(kMark1TurnPatienceFrames + kMark1TurnRestFrames)] == 3, "yielded=" + std::to_string(p.Yielded(0)));
        Mark1PriorityDecision d;
        Mark1PriorityCore hv;
        hv.SetHoldValue(3.0);
        step(hv, 0, c, never, &d);
        Mark1Automation a;
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101; bar.columns[3].cooldown = 0.25;
        const Call fc = gate(a, true, 0, Mark1GateClock{}, noLanding);
        a.Priority().SetHoldValue(3.0);
        const Call fc3 = gate(a, true, 1, Mark1GateClock{}, noLanding);
        check("free/pressed_with_the_hold_value", d.pressFree && fc.seen[3] == 4.0 && fc3.seen[3] == 3.0
            && slot.entries[3] == kMark1InputResting, "seen=" + num(fc.seen[3]) + "/" + num(fc3.seen[3]));
    }

    // ---- the yield after a landing ----------------------------------------------------
    {
        Mark1PriorityCore core;
        const Cols c = cols(cdCol(), none(), none(), freeCol());
        std::vector<int> v;
        for (long long f = 0; f < 10 + kMark1YieldFrames + 2; ++f)
            v.push_back(step(core, f, c, [](int, long long f2) { return f2 == 10; }));
        bool quiet = true;
        for (long long f = 10 + 1; f < 10 + kMark1YieldFrames; ++f) if (v[(size_t)f] != 0) quiet = false;
        check("yield/no_press_for_the_yield_window", v[10] == 6 && quiet && v[(size_t)(10 + kMark1YieldFrames)] != 0,
            "after=" + std::to_string(v[(size_t)(10 + kMark1YieldFrames)]));
        Mark1PriorityCore k2;
        step(k2, 0, c, [](int, long long) { return true; });
        const Mark1PriorityDecision d = k2.Decide(5, c.c, Mark1GateClock{});
        check("yield/decision_names_the_yield", d.yielding && d.pressColumn == 0 && d.why[0] == Mark1PriorityWhy::Yield
            && d.why[3] == Mark1PriorityWhy::Yield);

        // Nothing is masked during the yield: a held key carries on.
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101;
        Mark1Automation a;
        gate(a, true, 0, Mark1GateClock{}, [](const Mark1FramePlan&) { return true; });   // Q lands
        slot.entries[1] = kMark1InputHeld;
        const long writes = slot.writes;
        const Call y = gate(a, true, 1, Mark1GateClock{}, noLanding);
        check("yield/held_key_untouched_during_the_yield", y.plan.PressCount() == 0 && y.seen[1] == kMark1InputHeld
            && slot.writes == writes && a.Counters(3).skipTurn == 1 && a.Counters(3).landed == 1);
        Mark1PriorityCore zero;
        zero.SetYieldFrames(0);
        std::vector<int> z;
        for (long long f = 0; f < 3; ++f) z.push_back(step(zero, f, c, [](int, long long) { return true; }));
        check("yield/a_yield_of_0_presses_on", seq(z) == "333", "order=" + seq(z));
    }

    // ---- a cooldown press that does not land rests the column -------------------------
    {
        Mark1PriorityCore core;
        const Cols c = cols(freeCol(), cdCol(), none(), none());
        std::vector<int> v;
        for (long long f = 0; f <= kMark1ProbeRestFrames; ++f) v.push_back(step(core, f, c, never));
        bool freeBetween = true;
        for (long long f = 1; f < kMark1ProbeRestFrames; ++f) if (v[(size_t)f] != 3) freeBetween = false;
        check("rest/failed_press_rests_the_probe_rest", v[0] == 4 && freeBetween && v[(size_t)kMark1ProbeRestFrames] == 4,
            "order=" + seq(v));
        check("rest/counted_yielded", core.Yielded(1) == 2 && core.Yielded(0) == 0, "yieldedE=" + std::to_string(core.Yielded(1)));
        Mark1PriorityCore r;
        r.SetRestFrames(5);
        std::vector<int> w;
        for (long long f = 0; f < 7; ++f) w.push_back(step(r, f, c, never));
        check("rest/the_rest_in_force_is_used", seq(w) == "4333343", "order=" + seq(w));
        Mark1PriorityCore back;
        for (long long f = 0; f <= kMark1ProbeRestFrames; ++f) step(back, f, c, never);   // E's rest begins again at 24
        check("rest/frame_reset_ends_a_rest", back.Resting(1, kMark1ProbeRestFrames) && step(back, 5, c, never) == 4);
        // A cooldown column that lands is not rested.
        Mark1PriorityCore land;
        step(land, 0, c, [](int, long long) { return true; });
        check("rest/a_landing_is_no_rest", land.Yielded(1) == 0 && !land.Resting(1, 1));
    }

    // ---- free columns take turns -------------------------------------------------------
    {
        Mark1PriorityCore core;
        core.SetYieldFrames(0);
        const Cols c = cols(freeCol(), none(), freeCol(0), freeCol());
        std::vector<int> v;
        for (long long f = 0; f < 6; ++f) v.push_back(step(core, f, c, [](int, long long) { return true; }));
        check("turns/one_cast_each_in_q_e_r_y_order", seq(v) == "356356", "order=" + seq(v));
        Mark1PriorityCore p;
        const Cols qr = cols(freeCol(), none(), freeCol(), none());
        std::vector<int> w;
        for (long long f = 0; f <= kMark1TurnPatienceFrames + 1; ++f) w.push_back(step(p, f, qr, never));
        check("turns/patience_passes_the_turn", w[0] == 3 && w[(size_t)kMark1TurnPatienceFrames - 1] == 3
            && w[(size_t)kMark1TurnPatienceFrames] == 5 && p.Yielded(0) == 1, "at90=" + std::to_string(w[(size_t)kMark1TurnPatienceFrames]));
        // The turn skips a free column that rests or is not eligible.
        Mark1PriorityCore s;
        s.SetYieldFrames(0);
        std::vector<int> x;
        x.push_back(step(s, 0, qr, [](int, long long) { return true; }));                  // Q lands: R's turn
        x.push_back(step(s, 1, cols(freeCol(), none(), none(), none()), [](int, long long) { return true; }));   // R gone: Q
        check("turns/ineligible_free_column_skipped", seq(x) == "33", "order=" + seq(x));
    }

    // ---- the mask: only asking entries, only while pressing ----------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101;                               // Q, a cooldown column
        slot.entries[1] = kMark1InputHeld;                           // the right mouse button held (reading: entry 1)
        slot.entries[2] = kMark1InputPressed;                        // a key on its press frame
        slot.entries[7] = 2.0;                                       // a key on its release
        slot.entries[0] = kMark1InputResting;
        slot.entries[12] = kMark1InputHeld;                          // not a mask entry
        Mark1Automation a;
        const Call c = gate(a, true, 0, Mark1GateClock{}, noLanding);
        check("mask/asking_entries_rested_during_the_press", c.seen[3] == 4.0 && c.seen[1] == 1.0 && c.seen[2] == 1.0
            && c.seen[7] == 2.0 && c.seen[0] == 1.0 && c.seen[12] == kMark1InputHeld,
            "seen1=" + num(c.seen[1]) + " seen2=" + num(c.seen[2]) + " seen7=" + num(c.seen[7]));
        check("mask/put_back_after_the_call", slot.entries[1] == kMark1InputHeld && slot.entries[2] == kMark1InputPressed
            && slot.entries[7] == 2.0 && slot.entries[3] == kMark1InputResting);
        check("mask/counted", a.Priority().Counters().masked == 1 && a.Priority().Counters().heldSeen == 1);
        // Q rests after the press that did not land: nothing pressed, nothing masked.
        const long writes = slot.writes;
        const Call r = gate(a, true, 1, Mark1GateClock{}, noLanding);
        check("mask/nothing_masked_without_a_press", r.plan.PressCount() == 0 && r.seen[1] == kMark1InputHeld
            && slot.writes == writes && a.Priority().Counters().masked == 1);
        // Off: read (held-seen counts) but never written.
        resetSlot();
        slot.entries[1] = kMark1InputHeld;
        Mark1Automation off;
        Mark1MaskSetting m;
        std::string why;
        Mark1ParseMaskSetting("off", m, why);
        off.Priority().SetMask(m);
        const Call o = gate(off, true, 0, Mark1GateClock{}, noLanding);
        check("mask/off_reads_and_writes_nothing_but_the_press", o.seen[1] == kMark1InputHeld && o.seen[3] == 4.0
            && off.Priority().Counters().masked == 0 && off.Priority().Counters().heldSeen == 1
            && Mark1FormatMaskSetting(off.Priority().Settings().mask) == "off");
        // An entry list of its own.
        resetSlot();
        slot.entries[1] = kMark1InputHeld; slot.entries[9] = kMark1InputHeld;
        Mark1Automation own;
        Mark1ParseMaskSetting("9", m, why);
        own.Priority().SetMask(m);
        const Call w = gate(own, true, 0, Mark1GateClock{}, noLanding);
        check("mask/entry_list_in_force", w.seen[9] == 1.0 && w.seen[1] == kMark1InputHeld && slot.entries[9] == kMark1InputHeld);
        // Restored after a throw.
        resetSlot();
        slot.entries[1] = kMark1InputHeld;
        Mark1Automation t;
        const Mark1FramePlan tp = t.Plan(true, 0, readColumn, readToggle);
        double during = -1;
        bool threw = false;
        try {
            t.PressAround<double>(tp, readEntry, writeEntry, [&]() -> double { during = slot.entries[1]; throw std::runtime_error("gate"); });
        } catch (const std::runtime_error&) { threw = true; }
        check("mask/put_back_after_a_throw", threw && during == 1.0 && slot.entries[1] == kMark1InputHeld && slot.entries[3] == kMark1InputResting);
        // The list's parse refuses an automated column's entry, a repeat and junk.
        Mark1MaskSetting keep;
        check("mask/parse_refuses", !Mark1ParseMaskSetting("3", keep, why) && !Mark1ParseMaskSetting("1,1", keep, why)
            && !Mark1ParseMaskSetting("x", keep, why) && !Mark1ParseMaskSetting("64", keep, why)
            && Mark1ParseMaskSetting("default", keep, why) && Mark1FormatMaskSetting(keep) == "0,1,2,7,8,9,10,11");
        // The pure plan: mask entries only with a press.
        bool pressedNone[kMark1ColumnCount] = {};
        bool pressedQ[kMark1ColumnCount] = { true, false, false, false };
        double value[kMark1ColumnCount] = { 4, 4, 4, 4 };
        const Mark1InputPlan np = Mark1PlanInputs(true, true, 0u, pressedNone, value, Mark1MaskSetting{});
        const Mark1InputPlan qp = Mark1PlanInputs(true, true, 0u, pressedQ, value, Mark1MaskSetting{});
        const bool ok[4] = { true, true, true, false };
        const double vals[4] = { 1, 3, 4, 3 };
        check("mask/plan_needs_a_press", np.maskCount == 0 && qp.maskCount == 8 && qp.maskWrites
            && Mark1EntriesToRest(4, ok, vals) == 0x6u && Mark1EntriesToRest(4, ok, vals, false) == 0u);
    }

    // ---- baseline: the press route without a shape --------------------------------------
    {
        Mark1PriorityCore core;
        const Mark1RouteReply reply = core.SetRoute(Mark1Route::Proc);
        Mark1PriorityCore plain;
        const Cols c = cols(freeCol(), cdCol(), freeCol(0), none());
        bool same = true;
        for (long long f = 0; f < 600; ++f) {
            auto lands = [](int column, long long f2) { return (f2 % 37 == 0) || (column == 4 && f2 % 100 == 0); };
            Mark1PriorityDecision a, b;
            const int x = step(core, f, c, lands, &a);
            const int y = step(plain, f, c, lands, &b);
            if (x != y || a.procColumn != 0 || a.procMask != 0u) same = false;
        }
        check("route/proc_without_a_shape_refused", reply == Mark1RouteReply::ShapeUnset && core.RouteInForce() == Mark1Route::Press
            && core.Settings().route == Mark1Route::Press);
        check("route/press_decisions_unchanged", same);
        // A template: parsed by token, an unknown token refused by name.
        const Mark1ProcShape good = Mark1ParseProcShape("id, false, x,y, 0 , -1.5,undefined,true");
        const Mark1ProcShape bad = Mark1ParseProcShape("id,level,x");
        const Mark1ProcShape empty = Mark1ParseProcShape("");
        check("route/shape_parsed", good.ok && good.args.size() == 8 && Mark1FormatProcShape(good.args) == "id,false,x,y,0,-1.5,undefined,true",
            Mark1FormatProcShape(good.args));
        check("route/shape_unknown_token_refused_by_name", !bad.ok && bad.bad == "level" && bad.args.empty()
            && empty.ok && empty.args.empty() && Mark1FormatProcShape(empty.args) == "unset");
        // Clearing the shape returns the route to the press.
        Mark1PriorityCore cl;
        cl.SetShape(Mark1ParseProcShape("id"));
        const bool procOk = cl.SetRoute(Mark1Route::Proc) == Mark1RouteReply::Ok && cl.RouteInForce() == Mark1Route::Proc;
        cl.SetShape(Mark1ParseProcShape(""));
        check("route/clear_returns_to_the_press", procOk && cl.RouteInForce() == Mark1Route::Press);
    }

    // ---- the proc route bypasses the gate -------------------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 501; bar.columns[3].cooldown = 14;     // Q, a cooldown skill
        slot.entries[1] = kMark1InputHeld;                               // a held key
        Mark1Automation a;
        a.Priority().SetShape(Mark1ParseProcShape("id,x,y,false"));
        a.Priority().SetRoute(Mark1Route::Proc);
        Call c;
        c.plan = a.Plan(true, 0, at(1000), readColumn, readToggle);
        bool ran = false;
        a.PressAround<double>(c.plan, readEntry, writeEntry, [&]() -> double { for (int i = 0; i < 16; ++i) c.seen[i] = slot.entries[i]; ran = true; return 7.0; });
        a.Landing(c.plan, false);
        std::vector<Mark1ProcValue> args;
        std::string why;
        const bool built = a.ProcArgs(c.plan, 120.5, -40, args, why);
        a.ProcDone(c.plan, Mark1ProcOutcome::Returned);
        bool writesOnlyNone = true;
        for (const auto& [entry, value] : slot.writeLog) { (void)value; if (entry != 3) writesOnlyNone = false; }
        check("proc/fired_never_pressed", ran && c.plan.procColumn == 3 && c.plan.PressCount() == 0
            && c.plan.decision[0] == Mark1ColumnDecision::Proc && c.seen[3] == kMark1InputResting && a.Counters(3).presses == 0,
            "proc=" + std::to_string(c.plan.procColumn));
        check("proc/never_masked", c.seen[1] == kMark1InputHeld && writesOnlyNone && a.Priority().Counters().masked == 0);
        check("proc/args_from_the_shape", built && args.size() == 4 && args[0].number == 501 && args[1].number == 120.5
            && args[2].number == -40 && args[3].kind == Mark1ProcValue::Kind::Bool && !args[3].flag, why);
        check("proc/counted", a.Priority().Counters().procs == 1 && a.Counters(3).landed == 1 && a.Priority().ProcColumnCount() == 1);
        // A throw returns the route to the press for the session.
        const Call d = gate(a, true, 1, at(1000 + 15000), noLanding);   // due again: fired, reported returned by gate()
        Mark1FramePlan e = a.Plan(true, 2, at(1000 + 30000), readColumn, readToggle);
        a.ProcDone(e, Mark1ProcOutcome::Threw);
        const Mark1FramePlan after = a.Plan(true, 3, at(1000 + 45000), readColumn, readToggle);
        check("proc/a_throw_falls_back_to_the_press", d.plan.procColumn == 3 && e.procColumn == 3
            && a.Priority().Counters().procFail == 1 && a.Priority().Counters().procs == 3
            && a.Priority().RouteInForce() == Mark1Route::Press && after.procColumn == 0 && after.Press(3));
        // A refused call is no call made.
        Mark1Automation r;
        r.Priority().SetShape(Mark1ParseProcShape("id"));
        r.Priority().SetRoute(Mark1Route::Proc);
        const Mark1FramePlan rp = r.Plan(true, 0, at(0), readColumn, readToggle);
        r.ProcDone(rp, Mark1ProcOutcome::Refused);
        r.ProcDone(rp, Mark1ProcOutcome::Refused);                      // once per plan
        check("proc/refused_counts_a_failure_not_a_call", r.Priority().Counters().procs == 0 && r.Priority().Counters().procFail == 1
            && r.Priority().RouteInForce() == Mark1Route::Press);
        std::vector<Mark1ProcValue> none2;
        check("proc/refused_argument", !Mark1BuildProcArgs(Mark1ParseProcShape("id,x").args, 501, std::nan(""), 0, none2, why)
            && why == "x unreadable" && none2.empty()
            && !Mark1BuildProcArgs({}, 501, 0, 0, none2, why) && why == "shape unset");
        // No clock: nothing fired.
        Mark1Automation n;
        n.Priority().SetShape(Mark1ParseProcShape("id"));
        n.Priority().SetRoute(Mark1Route::Proc);
        const Mark1FramePlan np = n.Plan(true, 0, readColumn, readToggle);
        check("proc/no_clock_no_proc", np.procColumn == 0 && np.PressCount() == 0 && np.procMask == 1u);
        // An unreadable cooldown has no interval: it keeps the press.
        resetBar(); resetSlot();
        bar.columns[4].talentId = 502; bar.columns[4].cooldownRead = false;
        Mark1Automation u;
        u.Priority().SetShape(Mark1ParseProcShape("id"));
        u.Priority().SetRoute(Mark1Route::Proc);
        const Mark1FramePlan up = u.Plan(true, 0, at(0), readColumn, readToggle);
        check("proc/unreadable_cooldown_keeps_the_press", up.procColumn == 0 && up.Press(4) && up.procMask == 0u);
    }

    // ---- a proc cooldown column waits its interval ---------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[4].talentId = 601; bar.columns[4].cooldown = 14;     // E: Healing Zone's 14 s
        Mark1Automation a;
        a.Priority().SetShape(Mark1ParseProcShape("id"));
        a.Priority().SetRoute(Mark1Route::Proc);
        // Haste 100 and Faster Cast Rate 100: 14 / 1.5 + 1 / 2 = 9.8333 s.
        const Call first = gate(a, true, 0, at(0, 100, 100), noLanding);
        const Call before = gate(a, true, 1, at(9832, 100, 100), noLanding);
        const Call due = gate(a, true, 2, at(9833, 100, 100), noLanding);
        check("interval/cooldown_at_haste_plus_cast_at_fcr", first.plan.procColumn == 4 && before.plan.procColumn == 0
            && due.plan.procColumn == 4 && before.plan.decision[1] == Mark1ColumnDecision::SkipTurn,
            "before=" + std::to_string(before.plan.procColumn) + " due=" + std::to_string(due.plan.procColumn));
        // The stats the plan was made with set the next due time: haste 0,
        // Faster Cast Rate 0 (before any read) is the longest, 15 s.
        Mark1Automation b;
        b.Priority().SetShape(Mark1ParseProcShape("id"));
        b.Priority().SetRoute(Mark1Route::Proc);
        gate(b, true, 0, at(0), noLanding);
        const Call b1 = gate(b, true, 1, at(14999, 300, 0), noLanding);
        const Call b2 = gate(b, true, 2, at(15000, 300, 0), noLanding);
        const Call b3 = gate(b, true, 3, at(15000 + 7999, 300, 0), noLanding);   // haste 300 counts as 200: 7 + 1 s
        const Call b4 = gate(b, true, 4, at(15000 + 8000, 300, 0), noLanding);
        check("interval/next_due_time_set_when_fired", b1.plan.procColumn == 0 && b2.plan.procColumn == 4
            && b3.plan.procColumn == 0 && b4.plan.procColumn == 4);
        // The base cast seconds in force.
        Mark1PriorityCore k;
        Mark1PriorityColumn hz = cdCol(14);
        const double i1 = k.IntervalMs(hz, at(0));
        k.SetBaseCastSeconds(2.0);
        check("interval/base_cast_seconds_in_force", std::fabs(i1 - 15000) < 1e-6 && std::fabs(k.IntervalMs(hz, at(0)) - 16000) < 1e-6
            && !k.SetBaseCastSeconds(0.05) && !k.SetBaseCastSeconds(5.5) && k.Settings().baseCastSeconds == 2.0);
        // A clock that went backwards makes the column due again.
        Mark1PriorityCore back;
        back.SetShape(Mark1ParseProcShape("id"));
        back.SetRoute(Mark1Route::Proc);
        const Cols c = cols(cdCol(14), none(), none(), none());
        back.ProcFired(3, 50000, 15000);
        check("interval/clock_reset_is_due", back.Decide(0, c.c, at(60000)).procColumn == 0 && back.Decide(1, c.c, at(10)).procColumn == 3);
    }

    // ---- free columns on the proc route fire each cast time --------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 701; bar.columns[3].cooldown = 0.25;   // Q, no cooldown
        bar.columns[5].talentId = 702; bar.columns[5].cooldown = 0;      // R, no cooldown
        Mark1Automation a;
        a.Priority().SetShape(Mark1ParseProcShape("id"));
        a.Priority().SetRoute(Mark1Route::Proc);
        std::vector<int> fired;
        for (long long ms = 0; ms < 10000; ms += 50) {
            for (int call = 0; call < 2; ++call) {                     // the gate runs more than once a frame
                const Call c = gate(a, true, ms / 7, at(ms), noLanding);
                if (c.plan.procColumn) fired.push_back(c.plan.procColumn);
            }
        }
        long q = 0, r = 0;
        for (int col : fired) { if (col == 3) ++q; if (col == 5) ++r; }
        check("procfree/each_on_its_own_clock", q == 10 && r == 10, "Q=" + std::to_string(q) + " R=" + std::to_string(r));
        check("procfree/q_first_when_both_are_due", fired.size() >= 4 && fired[0] == 3 && fired[1] == 5 && fired[2] == 3 && fired[3] == 5,
            "first=" + (fired.empty() ? std::string("-") : std::to_string(fired[0])));
        check("procfree/never_pressed", a.Counters(3).presses == 0 && a.Counters(5).presses == 0 && a.Priority().ProcColumnCount() == 2);
        // At Faster Cast Rate 100 the cast takes half the time.
        Mark1Automation f;
        f.Priority().SetShape(Mark1ParseProcShape("id"));
        f.Priority().SetRoute(Mark1Route::Proc);
        long n = 0;
        for (long long ms = 0; ms < 10000; ms += 50) if (gate(f, true, ms, at(ms, 0, 100), noLanding).plan.procColumn) ++n;
        check("procfree/faster_cast_rate_halves_the_interval", n == 40, "procs=" + std::to_string(n));
    }

    // ---- the proc route never fires a toggle ------------------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 801; bar.columns[3].toggle = true; bar.columns[3].cooldown = 0;   // a toggle, Off
        bar.columns[4].talentId = 802; bar.columns[4].cooldown = 14;
        Mark1Automation a;
        a.Priority().SetShape(Mark1ParseProcShape("id"));
        a.Priority().SetRoute(Mark1Route::Proc);
        bool toggleProc = false;
        long toggleSeenPressed = 0;
        for (long long f = 0; f < 200; ++f) {
            const Call c = gate(a, true, f, at(f * 7), noLanding);
            if (c.plan.procColumn == 3 || (c.plan.procMask & 1u)) toggleProc = true;
            if (c.seen[3] == kMark1PressValue) ++toggleSeenPressed;
        }
        check("toggle/never_fired", !toggleProc && a.Counters(4).landed >= 1);
        check("toggle/takes_the_press", toggleSeenPressed >= 1 && a.Counters(3).presses == toggleSeenPressed,
            "pressed=" + std::to_string(toggleSeenPressed));
        Mark1PriorityCore k;
        k.SetShape(Mark1ParseProcShape("id"));
        k.SetRoute(Mark1Route::Proc);
        check("toggle/not_a_proc_column", !k.IsProcColumn(toggleCol()) && k.IsProcColumn(cdCol()) && k.IsProcColumn(freeCol())
            && !k.IsProcColumn(none()) && !k.IsProcColumn(unreadCdCol()));
    }

    // ---- the interval against the shared cases -----------------------------------------------
    {
        const int count = (int)(sizeof(kIntervalCases) / sizeof(kIntervalCases[0]));
        std::cout << "INTERVAL_CASES " << count << std::endl;
        for (int k = 0; k < count; ++k) {
            const IntervalCase& c = kIntervalCases[k];
            const double got = Mark1ProcIntervalMs(c.cooldown, c.haste, c.fcr, c.base);
            check("cases/interval_" + std::to_string(k), std::fabs(got - c.expectedMs) <= 1.0,
                "got=" + num(got) + " want=" + num(c.expectedMs));
        }
    }

    // ---- baseline: the lockout inactive touches nothing ---------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 901; bar.columns[4].talentId = 902;
        slot.entries[3] = kMark1InputHeld; slot.entries[4] = kMark1InputPressed;
        Mark1Automation a;
        const Call c = gate(a, false, 0, at(0), noLanding);
        check("lockbase/inactive_touches_nothing", c.seen[3] == kMark1InputHeld && c.seen[4] == kMark1InputPressed
            && slot.reads == 0 && slot.writes == 0 && a.Priority().Counters().locked == 0);
        // The control: the lockout off, a key held on an automated column is
        // not rested (its press still is the automation's).
        Mark1Automation off;
        off.Priority().SetLockout(false);
        gate(off, true, 0, Mark1GateClock{}, [](const Mark1FramePlan&) { return true; });   // Q lands: the yield
        const Call y = gate(off, true, 1, Mark1GateClock{}, noLanding);
        check("lockbase/lockout_off_leaves_the_key", y.plan.lockMask == 0u && y.seen[3] == kMark1InputHeld
            && y.seen[4] == kMark1InputPressed && off.Priority().Counters().locked == 0 && !y.plan.NeedsInput());
    }

    // ---- the lockout rests only automated columns' held entries ---------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 911;                                   // Q automated, a cooldown column
        bar.columns[4].talentId = 912;                                   // E automated
        bar.columns[5].talentId = 0;                                     // R empty: not automated
        bar.columns[6].talentId = 914; bar.columns[6].aura = true;       // Y an aura: not automated
        slot.entries[3] = kMark1InputHeld;                               // the player holds Q
        slot.entries[4] = kMark1InputPressed;                            // and presses E
        slot.entries[5] = kMark1InputHeld;                               // R's key held
        slot.entries[6] = kMark1InputPressed;                            // Y's key pressed
        Mark1Automation a;
        const Call c = gate(a, true, 0, Mark1GateClock{}, noLanding);
        check("lock/automated_held_and_pressed_rested", c.seen[4] == kMark1InputResting && c.plan.lockMask == 0x3u,
            "seen4=" + num(c.seen[4]));
        check("lock/before_the_press", c.seen[3] == kMark1PressValue && slot.entries[3] == kMark1InputHeld,
            "seen3=" + num(c.seen[3]) + " after3=" + num(slot.entries[3]));
        check("lock/not_automated_left_alone", c.seen[5] == kMark1InputHeld && c.seen[6] == kMark1InputPressed);
        check("lock/put_back_after_the_call", slot.entries[3] == kMark1InputHeld && slot.entries[4] == kMark1InputPressed
            && slot.entries[5] == kMark1InputHeld && slot.entries[6] == kMark1InputPressed);
        check("lock/counted_once_per_call", a.Priority().Counters().locked == 1);
        // 1 (resting) and 2 (release) are left alone; nothing locked, nothing counted.
        resetSlot();
        slot.entries[3] = 2.0; slot.entries[4] = kMark1InputResting;
        Mark1Automation b;
        gate(b, true, 0, Mark1GateClock{}, [](const Mark1FramePlan&) { return true; });   // Q lands: the yield
        const long writes = slot.writes;
        const Call y = gate(b, true, 1, Mark1GateClock{}, noLanding);
        check("lock/resting_and_release_left_alone", y.seen[3] == 2.0 && y.seen[4] == kMark1InputResting
            && slot.writes == writes && b.Priority().Counters().locked == 0 && y.plan.NeedsInput());
        // During the yield (no press) a held automated key is still rested.
        slot.entries[4] = kMark1InputHeld;
        const Call y2 = gate(b, true, 2, Mark1GateClock{}, noLanding);
        check("lock/during_the_yield", y2.plan.PressCount() == 0 && y2.seen[4] == kMark1InputResting
            && slot.entries[4] == kMark1InputHeld && b.Priority().Counters().locked == 1);
        // The proc route: the lockout too, with no press.
        resetSlot();
        slot.entries[3] = kMark1InputHeld;
        Mark1Automation p;
        p.Priority().SetShape(Mark1ParseProcShape("id"));
        p.Priority().SetRoute(Mark1Route::Proc);
        const Call pc = gate(p, true, 0, at(0), noLanding);
        check("lock/on_the_proc_route", pc.plan.procColumn == 3 && pc.plan.PressCount() == 0 && pc.seen[3] == kMark1InputResting
            && slot.entries[3] == kMark1InputHeld && p.Priority().Counters().locked == 1);
        // A throw puts the locked entry back.
        resetSlot();
        slot.entries[4] = kMark1InputHeld;
        Mark1Automation t;
        const Mark1FramePlan tp = t.Plan(true, 0, readColumn, readToggle);
        bool threw = false;
        try { t.PressAround<double>(tp, readEntry, writeEntry, []() -> double { throw std::runtime_error("gate"); }); }
        catch (const std::runtime_error&) { threw = true; }
        check("lock/put_back_after_a_throw", threw && slot.entries[4] == kMark1InputHeld && slot.entries[3] == kMark1InputResting);
    }

    // ---- the grey mask is the border mask -------------------------------------------------------
    {
        bool same = true;
        Mark1Automation a;
        for (int pattern = 0; pattern < 81; ++pattern) {
            resetBar();
            int p = pattern;
            for (int c = kMark1FirstColumn; c <= kMark1LastColumn; ++c) {
                const int kind = p % 3; p /= 3;                          // 0 empty, 1 a skill, 2 an aura
                bar.columns[c].talentId = kind == 0 ? 0 : 100 + c;
                bar.columns[c].aura = kind == 2;
                bar.columns[c].toggle = c == 4;
            }
            if (a.GreyMask(true, readColumn) != a.BorderMask(true, readColumn)) same = false;
        }
        resetBar();
        bar.columns[3].talentId = 1; bar.columns[5].talentId = 2; bar.columns[6].talentId = 3; bar.columns[6].aura = true;
        check("grey/the_border_mask", same && a.GreyMask(true, readColumn) == 0x5u && Mark1GreyMask(0xAu) == 0xAu);
        const long reads = bar.reads;
        check("grey/inactive_none", a.GreyMask(false, readColumn) == 0u && bar.reads == reads);
    }

    // ---- the research overrides refuse what is out of range --------------------------------------
    {
        Mark1PriorityCore k;
        check("overrides/ranges", !k.SetHoldValue(2.0) && k.SetHoldValue(3.0) && !k.SetYieldFrames(601) && !k.SetYieldFrames(-1)
            && k.SetYieldFrames(600) && !k.SetRestFrames(601) && k.SetRestFrames(0)
            && k.Settings().holdValue == 3.0 && k.Settings().yieldFrames == 600 && k.Settings().restFrames == 0);
        // The stat reads: at most once a refresh, a failure keeps the last value.
        Mark1StatCache s;
        const bool firstDue = s.Due(0);
        s.Begin(0);
        s.Store(Mark1Stat::Haste, true, 40);
        s.Store(Mark1Stat::FasterCastRate, false, 0);
        const bool early = s.Due(999);
        s.Begin(1000);
        s.Store(Mark1Stat::Haste, false, 0);
        check("overrides/stat_cache", firstDue && !early && s.Due(2000) && s.Value(Mark1Stat::Haste) == 40
            && !s.Have(Mark1Stat::FasterCastRate) && s.Value(Mark1Stat::FasterCastRate) == 0 && s.Fails() == 2
            && kMark1StatIds[0] == 37 && kMark1StatIds[1] == 103 && kMark1StatIds[2] == 106);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
