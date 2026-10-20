// Behavioral regression harness for the Mark 1 set's Automation core
// (Mark1Automation.hpp).
//
// The Python runner injects the REAL ForgePact::Mark1Automation code below.
// Nothing of the game is touched: a controlled bar stands in for bar columns
// 3 to 6 of `UI_Hud_Talent_obj.row0` (each column's talent id, whether it is
// an aura, whether it is a row of ToggleSkillMod.hpp's toggle table, and what
// the toggle read answers), and a controlled `inputState` array stands in for
// the profile slot's entries, so every scenario can see exactly which columns
// were read, which entries were written, with what, and in which order.
//
// The turn rule's scenarios also run against a gate model (GateModel below):
// a test-side stand-in for which pressed column the game's gate casts, built
// from Live 2b's bar and timings. It is a model, not the game.
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// PRODUCTION_MARK1AUTOMATION

using ForgePact::Mark1Automation;
using ForgePact::Mark1ColumnDecision;
using ForgePact::Mark1ColumnRead;
using ForgePact::Mark1FramePlan;
using ForgePact::Mark1ToggleState;
using ForgePact::kMark1ColumnCount;
using ForgePact::kMark1FirstColumn;
using ForgePact::kMark1LastColumn;
using ForgePact::kMark1PressValue;
using ForgePact::kMark1ToggleSettleFrames;
using ForgePact::kMark1TurnPatienceFrames;
using ForgePact::kMark1TurnRestFrames;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

// ---- the controlled bar and profile slot ---------------------------------
struct Column {
    bool readable = true;
    double talentId = 0;        // 0: an empty column
    bool aura = false;
    bool toggle = false;
    Mark1ToggleState state = Mark1ToggleState::Off;
};
struct Bar {
    std::map<int, Column> columns;          // keyed by bar column 0..9
    long columnReads = 0;
    long toggleReads = 0;
    std::vector<int> toggleReadColumns;
};
struct Slot {
    double entries[10] = {};                // the profile slot's inputState entries
    bool unreadable[10] = {};               // reading this entry fails
    bool writeFails[10] = {};               // writing this entry fails
    long reads = 0;
    long writes = 0;
    std::vector<std::pair<int, double>> writeLog;
};
static Bar bar;
static Slot slot;
// What an entry reads with no key down (phase 1's `slot-keys`: it rests at
// 1, reads 4 on the press frame, 3 while held, 2 on release), so a restore
// is checked against a real resting value rather than a zero-initialised one.
static constexpr double kRest = 1.0;

static void resetBar() {
    bar = Bar();
    for (int c = 0; c < 10; ++c) bar.columns[c] = Column();
}
static void resetSlot() {
    slot = Slot();
    for (int i = 0; i < 10; ++i) slot.entries[i] = kRest;
}

static Mark1ColumnRead readColumn(int column) {
    ++bar.columnReads;
    const Column& c = bar.columns[column];
    Mark1ColumnRead r;
    r.readable = c.readable;
    r.talentId = c.talentId;
    r.aura = c.aura;
    r.toggle = c.toggle;
    return r;
}
static Mark1ToggleState readToggle(int column) {
    ++bar.toggleReads;
    bar.toggleReadColumns.push_back(column);
    return bar.columns[column].state;
}
static bool readEntry(int entry, double& out) {
    ++slot.reads;
    if (entry < 0 || entry >= 10 || slot.unreadable[entry]) return false;
    out = slot.entries[entry];
    return true;
}
static bool writeEntry(int entry, const double& value) {
    ++slot.writes;
    slot.writeLog.push_back({ entry, value });
    if (entry < 0 || entry >= 10 || slot.writeFails[entry]) return false;
    slot.entries[entry] = value;
    return true;
}

// One `CheckTalentUse` call: plan the columns, then press them around a stand-in
// for the original gate. `seen` captures the entries the gate saw.
static double gateCall(Mark1Automation& a, bool active, long long frame, double* seen = nullptr) {
    const Mark1FramePlan plan = a.Plan(active, frame, readColumn, readToggle);
    return a.PressAround<double>(plan, readEntry, writeEntry, [&]() -> double {
        if (seen) for (int i = 0; i < 10; ++i) seen[i] = slot.entries[i];
        return 7.0;
    });
}

static long totalPresses(const Mark1Automation& a) {
    long n = 0;
    for (int c = kMark1FirstColumn; c <= kMark1LastColumn; ++c) n += a.Counters(c).presses;
    return n;
}
static long totalCounted(const Mark1Automation& a) {
    long n = 0;
    for (int c = kMark1FirstColumn; c <= kMark1LastColumn; ++c) {
        const auto k = a.Counters(c);
        n += k.presses + k.unreadable + k.empty + k.aura + k.toggleSettling + k.toggleOn
           + k.toggleUnreadable + k.originalUnreadable + k.pressWriteFailed + k.restoreFailed
           + k.landed + k.skipTurn + k.yielded;
    }
    return n + a.CallsThrew();
}

// A plan built by hand with several columns pressed: the old press-every-column
// shape. Plan never presses two columns any more, so the scenarios that pin
// PressAround's mechanics on several entries at once build it this way.
static Mark1FramePlan handPlan(long long frame, std::initializer_list<int> columns) {
    Mark1FramePlan p;
    p.active = true;
    p.frame = frame;
    for (int c : columns) p.decision[c - kMark1FirstColumn] = Mark1ColumnDecision::Press;
    return p;
}

// The columns whose entry holds the press value right now (inside the call).
static std::vector<int> pressedNow() {
    std::vector<int> v;
    for (int c = kMark1FirstColumn; c <= kMark1LastColumn; ++c)
        if (slot.entries[c] == kMark1PressValue) v.push_back(c);
    return v;
}

// One gate call under the turn rule: Plan, press around a stand-in original,
// then report to the core whether a cast landed. `lands(column)` is asked with
// the lowest pressed column (0 when none) and answers whether it cast.
// Returns the plan's holder; `pressed` gets how many entries the call saw
// pressed.
template <class Lands>
static int turnCall(Mark1Automation& a, long long frame, Lands&& lands, int* pressed = nullptr) {
    const Mark1FramePlan plan = a.Plan(true, frame, readColumn, readToggle);
    bool landed = false;
    a.PressAround<double>(plan, readEntry, writeEntry, [&]() -> double {
        const std::vector<int> now = pressedNow();
        if (pressed) *pressed = (int)now.size();
        landed = lands(now.empty() ? 0 : now.front());
        return 7.0;
    });
    a.Landing(plan, landed);
    return plan.holder;
}
static bool neverLands(int) { return false; }
static bool alwaysLands(int column) { return column != 0; }

// ---- the gate model (a test-side stand-in, labelled as a model, not the game)
// Once a frame, the gate casts at most one pressed column: the first, in the
// model's order (lowest column first, or highest first), that is past its own
// cooldown, and only when the frame is past the cast lock (60 frames after any
// landing). Live 2b's bar: Shadow Bolt on Q and Heavenly Fire on R, 36 frames
// each; Healing Zone on E, 700; Y empty.
struct GateModel {
    bool highestFirst = false;
    long long lastAny = -1;
    long long last[10];
    long long cooldown[10] = {};
    long landings[10] = {};
    GateModel() {
        for (long long& l : last) l = -1;
        cooldown[3] = 36; cooldown[4] = 700; cooldown[5] = 36;
    }
    bool Cast(long long frame) {
        if (lastAny >= 0 && frame - lastAny < 60) return false;
        for (int k = 0; k < kMark1ColumnCount; ++k) {
            const int c = highestFirst ? kMark1LastColumn - k : kMark1FirstColumn + k;
            if (slot.entries[c] != kMark1PressValue) continue;
            if (last[c] >= 0 && frame - last[c] < cooldown[c]) continue;
            last[c] = frame;
            lastAny = frame;
            ++landings[c];
            return true;
        }
        return false;
    }
};
static void liveTwoBBar() {
    resetBar(); resetSlot();
    bar.columns[3].talentId = 101;   // Shadow Bolt
    bar.columns[4].talentId = 102;   // Healing Zone
    bar.columns[5].talentId = 103;   // Heavenly Fire
    bar.columns[6].talentId = 0;     // empty
}
// 3600 frames of the turn rule against the model; the landings per column.
static GateModel runTurnsUnderModel(bool highestFirst) {
    liveTwoBBar();
    GateModel m;
    m.highestFirst = highestFirst;
    Mark1Automation a;
    for (long long f = 0; f < 3600; ++f) turnCall(a, f, [&](int) { return m.Cast(f); });
    return m;
}
static std::string landingsText(const GateModel& m) {
    return "Q=" + std::to_string(m.landings[3]) + " E=" + std::to_string(m.landings[4])
        + " R=" + std::to_string(m.landings[5]) + " Y=" + std::to_string(m.landings[6]);
}

int main() {
    // ---- baseline: the bonus inactive --------------------------------------
    {
        resetBar(); resetSlot();
        for (int c = kMark1FirstColumn; c <= kMark1LastColumn; ++c) bar.columns[c].talentId = 100 + c;
        bar.columns[4].toggle = true;
        Mark1Automation a;
        double seen[10] = {};
        bool atRest = true;
        for (long long f = 0; f < 120; ++f) {
            gateCall(a, false, f, seen);
            for (int i = 0; i < 10; ++i) if (seen[i] != kRest) atRest = false;
        }
        check("baseline/no_column_read", bar.columnReads == 0 && bar.toggleReads == 0,
            "columnReads=" + std::to_string(bar.columnReads) + " toggleReads=" + std::to_string(bar.toggleReads));
        check("baseline/no_column_pressed", atRest && totalPresses(a) == 0);
        check("baseline/nothing_counted", totalCounted(a) == 0, "counted=" + std::to_string(totalCounted(a)));
        check("baseline/no_border", a.BorderMask(false, readColumn) == 0u && bar.columnReads == 0);
        check("baseline/press_scope_writes_nothing", slot.reads == 0 && slot.writes == 0,
            "reads=" + std::to_string(slot.reads) + " writes=" + std::to_string(slot.writes));
    }

    // ---- empty and aura columns --------------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 0;                                   // empty
        bar.columns[4].talentId = 41; bar.columns[4].aura = true;     // an aura
        bar.columns[5].talentId = 0;  bar.columns[5].toggle = true;   // empty, even if flagged
        bar.columns[6].talentId = 42; bar.columns[6].aura = true; bar.columns[6].toggle = true;
        Mark1Automation a;
        double seen[10] = {};
        bool pressedAny = false;
        for (long long f = 0; f < 60; ++f) {
            gateCall(a, true, f, seen);
            for (int i = 0; i < 10; ++i) if (seen[i] != kRest) pressedAny = true;
        }
        check("columns/empty_never_pressed", !pressedAny && a.Counters(3).presses == 0 && a.Counters(5).presses == 0);
        check("columns/aura_never_pressed", a.Counters(4).presses == 0 && a.Counters(6).presses == 0 && bar.toggleReads == 0,
            "toggleReads=" + std::to_string(bar.toggleReads));
        check("columns/empty_and_aura_counted",
            a.Counters(3).empty == 60 && a.Counters(5).empty == 60 && a.Counters(4).aura == 60 && a.Counters(6).aura == 60,
            "empty3=" + std::to_string(a.Counters(3).empty) + " aura4=" + std::to_string(a.Counters(4).aura));
        check("border/empty_and_aura_unbordered", a.BorderMask(true, readColumn) == 0u);
        check("columns/press_scope_untouched", slot.writes == 0);

        resetBar(); resetSlot();
        bar.columns[3].readable = false; bar.columns[3].talentId = 55;
        Mark1Automation b;
        double seen2[10] = {};
        gateCall(b, true, 0, seen2);
        check("columns/unreadable_never_pressed", seen2[3] == kRest && b.Counters(3).presses == 0 && b.Counters(3).unreadable == 1);
        check("border/unreadable_unbordered", (b.BorderMask(true, readColumn) & (1u << 0)) == 0u);
    }

    // ---- an ordinary column ------------------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[5].talentId = 77;
        Mark1Automation a;
        long seenPressed = 0;
        for (long long f = 0; f < 200; ++f) {
            double seen[10] = {};
            gateCall(a, true, f, seen);
            if (seen[5] == kMark1PressValue) ++seenPressed;
        }
        check("columns/ordinary_every_call", seenPressed == 200 && bar.toggleReads == 0,
            "pressed=" + std::to_string(seenPressed));
        double seenA[10] = {}, seenB[10] = {};
        gateCall(a, true, 200, seenA);
        gateCall(a, true, 200, seenB);
        check("columns/ordinary_same_frame_twice", seenA[5] == kMark1PressValue && seenB[5] == kMark1PressValue);
        check("columns/ordinary_counted", a.Counters(5).presses == 202 && a.Counters(3).empty == 202,
            "presses=" + std::to_string(a.Counters(5).presses));
    }

    // ---- the border -----------------------------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 10;                                  // ordinary
        bar.columns[4].talentId = 11; bar.columns[4].toggle = true;   // a toggle (On or Off alike)
        bar.columns[4].state = Mark1ToggleState::On;
        bar.columns[5].talentId = 12; bar.columns[5].aura = true;     // an aura
        bar.columns[6].talentId = 0;                                   // empty
        bar.columns[1].talentId = 13;                                  // not an automated column
        Mark1Automation a;
        const unsigned mask = a.BorderMask(true, readColumn);
        check("border/ordinary_and_toggle_bordered", mask == ((1u << 0) | (1u << 1)) && bar.toggleReads == 0,
            "mask=" + std::to_string(mask));
    }

    // ---- the toggle guard ------------------------------------------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 20; bar.columns[3].toggle = true; bar.columns[3].state = Mark1ToggleState::Off;
        bar.columns[4].talentId = 21; bar.columns[4].toggle = true; bar.columns[4].state = Mark1ToggleState::On;
        Mark1Automation a;
        double seen[10] = {};
        gateCall(a, true, 1000, seen);
        check("toggle/off_pressed", seen[3] == kMark1PressValue && a.Counters(3).presses == 1);
        long onPressed = 0;
        for (long long f = 1000; f < 1300; ++f) {
            double s[10] = {};
            gateCall(a, true, f, s);
            if (s[4] != kRest) ++onPressed;
        }
        check("toggle/on_never_pressed", onPressed == 0 && a.Counters(4).presses == 0 && a.Counters(4).toggleOn == 301,
            "toggleOn=" + std::to_string(a.Counters(4).toggleOn));

        // Column 3 stays Off (the cast did not take): it is not pressed again
        // inside its settle window, and is pressed again the first frame after.
        resetSlot();
        Mark1Automation b;
        double s0[10] = {};
        gateCall(b, true, 5000, s0);
        long inside = 0;
        for (long long f = 5001; f < 5000 + kMark1ToggleSettleFrames; ++f) {
            double s[10] = {};
            gateCall(b, true, f, s);
            if (s[3] != kRest) ++inside;
        }
        check("toggle/settle_window_blocks", s0[3] == kMark1PressValue && inside == 0
            && b.Counters(3).toggleSettling == kMark1ToggleSettleFrames - 1,
            "inside=" + std::to_string(inside) + " settling=" + std::to_string(b.Counters(3).toggleSettling));
        double after[10] = {};
        gateCall(b, true, 5000 + kMark1ToggleSettleFrames, after);
        check("toggle/settle_window_ends", after[3] == kMark1PressValue && b.Counters(3).presses == 2);

        // The window is the column's own: a second Off toggle pressed later
        // is not held back by the first column's press.
        resetBar(); resetSlot();
        bar.columns[3].talentId = 20; bar.columns[3].toggle = true;
        bar.columns[6].talentId = 23; bar.columns[6].toggle = true;
        bar.columns[6].readable = false;
        Mark1Automation c;
        gateCall(c, true, 100);
        bar.columns[6].readable = true;
        double sc[10] = {};
        gateCall(c, true, 101, sc);
        check("toggle/settle_is_per_column", sc[6] == kMark1PressValue && sc[3] == kRest
            && c.Counters(6).presses == 1 && c.Counters(3).toggleSettling == 1);

        // A frame counter that went backwards (a restart of whatever counts
        // frames) never leaves a toggle stuck inside a window that cannot end:
        // both toggles are eligible again (neither counts as settling), and
        // under the turn rule the lower one takes the call.
        double sr[10] = {};
        gateCall(c, true, 5, sr);
        check("toggle/settle_survives_frame_reset", sr[3] == kMark1PressValue && sr[6] == kRest
            && c.Counters(6).skipTurn == 1 && c.Counters(6).toggleSettling == 0 && c.Counters(3).toggleSettling == 1,
            "skipTurn6=" + std::to_string(c.Counters(6).skipTurn));

        check("toggle/settle_constant_is_29", kMark1ToggleSettleFrames == 29);
    }
    {
        resetBar(); resetSlot();
        bar.columns[5].talentId = 30; bar.columns[5].toggle = true; bar.columns[5].state = Mark1ToggleState::Unreadable;
        Mark1Automation a;
        long pressed = 0;
        for (long long f = 0; f < 300; ++f) {
            double s[10] = {};
            gateCall(a, true, f, s);
            if (s[5] != kRest) ++pressed;
        }
        check("toggle/unreadable_never_pressed", pressed == 0 && a.Counters(5).presses == 0 && slot.writes == 0);
        check("toggle/unreadable_counted", a.Counters(5).toggleUnreadable == 300,
            "toggleUnreadable=" + std::to_string(a.Counters(5).toggleUnreadable));
    }
    {
        // A toggle held on: pressed while Off, its instance shows up a few
        // frames later (inside the settle window), and from then on the toggle
        // reads On for 600 frames - and is never pressed again, so it is never
        // switched back off. The toggle re-cast guard (`toggleguard`) keeps a
        // double-cast proc from flipping it; this guard keeps the bonus from it.
        resetBar(); resetSlot();
        bar.columns[4].talentId = 40; bar.columns[4].toggle = true; bar.columns[4].state = Mark1ToggleState::Off;
        Mark1Automation a;
        long pressed = 0;
        bool on = false;
        long framesOff = 0;
        long long pressedAt = -1;
        const long long showsAfter = 6;
        for (long long f = 0; f < 600; ++f) {
            if (pressedAt >= 0 && f - pressedAt >= showsAfter) bar.columns[4].state = Mark1ToggleState::On;
            for (int call = 0; call < 2; ++call) {                     // the gate runs more than once a frame
                double s[10] = {};
                gateCall(a, true, f, s);
                if (s[4] == kMark1PressValue) {
                    ++pressed;
                    on = !on;                                          // a press flips the toggle
                    if (pressedAt < 0) pressedAt = f;
                }
            }
            if (f > pressedAt + showsAfter && !on) ++framesOff;
        }
        check("toggle/held_on_600_frames_pressed_once", pressed == 1 && a.Counters(4).presses == 1,
            "pressed=" + std::to_string(pressed));
        check("toggle/held_on_600_frames_never_off", on && framesOff == 0, "framesOff=" + std::to_string(framesOff));
    }

    // ---- the press itself ------------------------------------------------------
    {
        resetBar(); resetSlot();
        for (int c = 0; c < 10; ++c) bar.columns[c].talentId = 200 + c;   // every column holds a skill
        for (int i = 0; i < 10; ++i) slot.entries[i] = 1.0 + i;            // distinct originals
        Mark1Automation a;
        double seen[10] = {};
        // Every automated column pressed in one call: a plan built by hand,
        // since Plan presses one column per call under the turn rule.
        const double result = a.PressAround<double>(handPlan(0, { 3, 4, 5, 6 }), readEntry, writeEntry, [&]() -> double {
            for (int i = 0; i < 10; ++i) seen[i] = slot.entries[i];
            return 7.0;
        });
        bool value4 = true, mapping = true, others = true;
        for (int c = kMark1FirstColumn; c <= kMark1LastColumn; ++c) {
            if (seen[c] != 4.0) value4 = false;
            if (ForgePact::Mark1InputEntryForColumn(c) != c) mapping = false;
        }
        for (int i = 0; i < 10; ++i)
            if ((i < kMark1FirstColumn || i > kMark1LastColumn) && seen[i] != 1.0 + i) others = false;
        check("press/value_4_written", value4 && kMark1PressValue == 4.0);
        check("press/column_entry_mapping", mapping && kMark1FirstColumn == 3 && kMark1LastColumn == 6 && kMark1ColumnCount == 4);
        check("press/outside_columns_rejected",
            ForgePact::Mark1InputEntryForColumn(2) < 0 && ForgePact::Mark1InputEntryForColumn(7) < 0
            && ForgePact::Mark1InputEntryForColumn(0) < 0 && !ForgePact::Mark1IsAutomatedColumn(9));
        bool onlyPlanned = true;
        for (const auto& [entry, value] : slot.writeLog)
            if (entry < kMark1FirstColumn || entry > kMark1LastColumn) onlyPlanned = false;
        check("press/only_planned_entries_written", onlyPlanned && others && slot.writeLog.size() == 8,
            "writes=" + std::to_string(slot.writeLog.size()));
        bool restored = true;
        for (int i = 0; i < 10; ++i) if (slot.entries[i] != 1.0 + i) restored = false;
        check("press/restored_after_return", restored);
        check("press/original_return_passed_through", result == 7.0);
        check("press/value_seen_by_the_call", seen[3] == 4.0 && seen[6] == 4.0);
    }
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 300; bar.columns[6].talentId = 301;
        slot.entries[3] = 0.5; slot.entries[6] = 2.0;
        Mark1Automation a;
        const Mark1FramePlan plan = handPlan(0, { 3, 6 });                 // two entries to put back
        bool threw = false;
        double during3 = -1;
        try {
            a.PressAround<double>(plan, readEntry, writeEntry, [&]() -> double {
                during3 = slot.entries[3];
                throw std::runtime_error("the gate threw");
            });
        } catch (const std::runtime_error& e) {
            threw = std::string(e.what()) == "the gate threw";
        }
        check("press/restored_after_throw", during3 == 4.0 && slot.entries[3] == 0.5 && slot.entries[6] == 2.0,
            "entry3=" + std::to_string(slot.entries[3]) + " entry6=" + std::to_string(slot.entries[6]));
        check("press/throw_propagates", threw && a.CallsThrew() == 1);
    }
    {
        // An entry whose original cannot be read is not pressed: it could not
        // be put back.
        resetBar(); resetSlot();
        bar.columns[4].talentId = 400; bar.columns[5].talentId = 401;
        slot.unreadable[4] = true;
        Mark1Automation a;
        double seen[10] = {};
        a.PressAround<double>(handPlan(0, { 4, 5 }), readEntry, writeEntry, [&]() -> double {
            for (int i = 0; i < 10; ++i) seen[i] = slot.entries[i];
            return 7.0;
        });
        bool wrote4 = false;
        for (const auto& [entry, value] : slot.writeLog) if (entry == 4) wrote4 = true;
        check("press/unreadable_original_untouched", !wrote4 && seen[5] == 4.0 && slot.entries[5] == kRest);
        check("press/unreadable_original_counted", a.Counters(4).originalUnreadable == 1 && a.Counters(4).presses == 0
            && a.Counters(5).presses == 1);
    }
    {
        // A restore that fails is counted, so a `stat` line can show it.
        resetBar(); resetSlot();
        bar.columns[6].talentId = 500;
        Mark1Automation a;
        const Mark1FramePlan plan = a.Plan(true, 0, readColumn, readToggle);
        int writesSeen = 0;
        auto failRestore = [&](int entry, const double& value) {
            ++writesSeen;
            if (writesSeen >= 2) return false;                         // the press lands, the restore does not
            return writeEntry(entry, value);
        };
        a.PressAround<double>(plan, readEntry, failRestore, []() { return 0.0; });
        check("press/restore_failure_counted", a.Counters(6).restoreFailed == 1 && a.Counters(6).presses == 1);
    }
    {
        // A void original is supported too (the scope does not need a result).
        resetBar(); resetSlot();
        bar.columns[3].talentId = 600;
        Mark1Automation a;
        const Mark1FramePlan plan = a.Plan(true, 0, readColumn, readToggle);
        double during = -1;
        a.PressAround<double>(plan, readEntry, writeEntry, [&]() { during = slot.entries[3]; });
        check("press/void_original", during == 4.0 && slot.entries[3] == kRest);
    }
    {
        // A read callback that throws is a column the core cannot read, never
        // a press and never an escape out of the gate.
        resetBar(); resetSlot();
        bar.columns[3].talentId = 700; bar.columns[3].toggle = true;
        Mark1Automation a;
        bool escaped = false;
        Mark1FramePlan plan;
        try {
            plan = a.Plan(true, 0,
                [](int column) -> Mark1ColumnRead { if (column == 4) throw std::runtime_error("x"); return readColumn(column); },
                [](int) -> Mark1ToggleState { throw std::runtime_error("y"); });
        } catch (...) { escaped = true; }
        check("columns/throwing_reads_contained", !escaped && !plan.Press(3) && !plan.Press(4)
            && a.Counters(4).unreadable == 1 && a.Counters(3).toggleUnreadable == 1);
    }

    // ---- baseline: what the old press-every-column rule did ----------------------
    {
        // Live 2b's shape under the gate model: with Q, E and R pressed on
        // every call and the lowest pressed column taking the cast, R (column
        // 5) never lands while Q does. (The model starves E too, which Live 2b
        // did not, so nothing is asserted about column 4.)
        liveTwoBBar();
        GateModel m;
        Mark1Automation a;
        for (long long f = 0; f < 3600; ++f)
            a.PressAround<double>(handPlan(f, { 3, 4, 5 }), readEntry, writeEntry, [&]() -> double { m.Cast(f); return 7.0; });
        check("baseline/gate_model_press_every_column_starves_column_5", m.landings[5] == 0 && m.landings[3] > 0,
            landingsText(m));
    }
    {
        // One eligible column alone is pressed on every gate call, a cast or not.
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101;
        Mark1Automation a;
        long pressedCalls = 0;
        for (long long f = 0; f < 1000; ++f) if (turnCall(a, f, neverLands) == 3) ++pressedCalls;
        check("baseline/lone_eligible_column_pressed_every_call",
            pressedCalls == 1000 && a.Counters(3).presses == 1000 && a.Counters(3).yielded == 0,
            "pressed=" + std::to_string(pressedCalls) + " yielded=" + std::to_string(a.Counters(3).yielded));
    }

    // ---- turns: one column per gate call, the longest-waiting first ---------------
    {
        liveTwoBBar();
        GateModel m;
        Mark1Automation a;
        bool onePerCall = true;
        for (long long f = 0; f < 3600; ++f) {
            int pressed = -1;
            const int holder = turnCall(a, f, [&](int) { return m.Cast(f); }, &pressed);
            if (pressed != 1 || holder < 3 || holder > 5) onePerCall = false;
        }
        check("turns/one_column_per_call", onePerCall);
        long skips = 0, presses = 0;
        for (int c = 3; c <= 5; ++c) { skips += a.Counters(c).skipTurn; presses += a.Counters(c).presses; }
        check("turns/others_counted_skip_turn", presses == 3600 && skips == 7200
            && a.Counters(6).skipTurn == 0 && a.Counters(6).empty == 3600,
            "presses=" + std::to_string(presses) + " skipTurn=" + std::to_string(skips));
    }
    {
        // Landings decide the order: a column that has never landed has waited
        // longest (ties to the lower column), a landed one goes to the back.
        liveTwoBBar();
        Mark1Automation a;
        std::vector<int> order;
        for (long long f = 0; f < 10; ++f)
            order.push_back(turnCall(a, f, [&](int column) { return column != 0 && f != 6; }));
        const std::vector<int> want = { 3, 4, 5, 3, 4, 5, 3, 3, 4, 5 };
        std::string got;
        for (int c : order) got += std::to_string(c);
        check("turns/ties_go_to_the_lower_column", order.size() == 10 && order[0] == 3 && order[1] == 4);
        check("turns/longest_waiting_first", order == want, "order=" + got);
        check("turns/landed_column_goes_to_the_back", order[3] == 3 && order[4] == 4 && order[8] == 4 && order[9] == 5,
            "order=" + got);
        check("turns/landing_counted", a.Counters(3).landed == 3 && a.Counters(4).landed == 3 && a.Counters(5).landed == 3
            && a.Counters(3).presses == 4 && a.Counters(4).presses == 3 && a.Counters(5).presses == 3
            && a.Counters(6).landed == 0,
            "landed=" + std::to_string(a.Counters(3).landed) + "/" + std::to_string(a.Counters(4).landed)
            + "/" + std::to_string(a.Counters(5).landed));
    }
    {
        // A landing counts only for a column this call actually pressed.
        liveTwoBBar();
        Mark1Automation a;
        const Mark1FramePlan inactive = a.Plan(false, 0, readColumn, readToggle);
        a.Landing(inactive, true);
        const bool inactiveNothing = totalCounted(a) == 0 && bar.columnReads == 0;
        const Mark1FramePlan hp = handPlan(0, { 3, 4 });
        a.PressAround<double>(hp, readEntry, writeEntry, []() { return 7.0; });
        a.Landing(hp, true);
        const bool handNothing = a.Counters(3).landed == 0 && a.Counters(4).landed == 0;

        Mark1Automation b;
        slot.writeFails[3] = true;                                     // the press cannot be written
        turnCall(b, 0, [](int) { return true; });                      // a cast landed all the same: the player's key
        slot.writeFails[3] = false;
        const bool failedNothing = b.Counters(3).landed == 0 && b.Counters(3).pressWriteFailed == 1;
        const int next = turnCall(b, 1, neverLands);                    // a landing=false call counts nothing
        check("turns/landing_needs_a_written_press", inactiveNothing && handNothing && failedNothing
            && next == 3 && b.Counters(3).landed == 0,
            "next=" + std::to_string(next));

        Mark1Automation c;
        const Mark1FramePlan p = c.Plan(true, 0, readColumn, readToggle);
        c.PressAround<double>(p, readEntry, writeEntry, []() { return 7.0; });
        c.Landing(p, true);
        c.Landing(p, true);                                            // the same call reported twice
        check("turns/one_landing_per_call", c.Counters(3).landed == 1 && turnCall(c, 1, neverLands) == 4);
    }
    {
        // Patience and rest: Q and E, neither ever casting. Q is pressed for 90
        // frames, then rests 288 while E takes the turn; E's own patience runs
        // out with Q resting, so E is not rested (no other column is free);
        // Q's wait was never restarted, so it is first again when its rest ends.
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101; bar.columns[4].talentId = 102;
        Mark1Automation a;
        std::vector<int> holder;
        long yieldedQAt377 = -1, skipQAt377 = -1;
        for (long long f = 0; f <= 600; ++f) {
            holder.push_back(turnCall(a, f, neverLands));
            if (f == 377) { yieldedQAt377 = a.Counters(3).yielded; skipQAt377 = a.Counters(3).skipTurn; }
        }
        auto all = [&](long long from, long long to, int column) {
            for (long long f = from; f <= to; ++f) if (holder[(size_t)f] != column) return false;
            return true;
        };
        const long long p = kMark1TurnPatienceFrames, r = kMark1TurnRestFrames;
        check("turns/patience_then_rest", all(0, p - 1, 3) && holder[(size_t)p] == 4 && yieldedQAt377 == 1,
            "holder@90=" + std::to_string(holder[(size_t)p]) + " yieldedQ=" + std::to_string(yieldedQAt377));
        check("turns/rest_lasts_its_frames", all(p, p + r - 1, 4) && holder[(size_t)(p + r)] == 3
            && skipQAt377 == r, "skipTurnQ=" + std::to_string(skipQAt377));
        check("turns/rest_needs_another_free_column", a.Counters(4).yielded == 0 && all(p + p, p + r - 1, 4),
            "yieldedE=" + std::to_string(a.Counters(4).yielded));
        check("turns/rest_keeps_the_wait", all(p + r, p + r + p - 1, 3) && holder[(size_t)(p + r + p)] == 4
            && a.Counters(3).yielded == 2 && a.Counters(3).landed == 0,
            "yieldedQ=" + std::to_string(a.Counters(3).yielded));
        check("turns/constants_90_and_288", kMark1TurnPatienceFrames == 90 && kMark1TurnRestFrames == 288);
    }
    {
        // A holder that stops being eligible loses the turn at once, and gets
        // it back (with a fresh run) as soon as it is eligible again.
        liveTwoBBar();
        Mark1Automation a;
        std::vector<int> holder;
        for (long long f = 0; f <= 101; ++f) {
            bar.columns[3].aura = f == 10;
            holder.push_back(turnCall(a, f, neverLands));
        }
        check("turns/holder_losing_eligibility_loses_the_turn",
            holder[9] == 3 && holder[10] == 4 && holder[11] == 3 && holder[100] == 3 && holder[101] == 4
            && a.Counters(3).yielded == 1 && a.Counters(3).aura == 1,
            "holder@10=" + std::to_string(holder[10]) + " holder@101=" + std::to_string(holder[101]));
    }
    {
        // ForgetPresses clears the waits, the rests and the run.
        liveTwoBBar();
        Mark1Automation a;
        turnCall(a, 0, alwaysLands);                                   // Q lands
        turnCall(a, 1, alwaysLands);                                   // E lands; R would be next
        a.ForgetPresses();
        const int afterWaits = turnCall(a, 2, neverLands);

        resetBar(); resetSlot();
        bar.columns[3].talentId = 101; bar.columns[4].talentId = 102;
        Mark1Automation b;
        for (long long f = 0; f <= kMark1TurnPatienceFrames; ++f) turnCall(b, f, neverLands);   // Q resting from 90
        b.ForgetPresses();
        std::vector<int> holder;
        for (long long f = 91; f <= 181; ++f) holder.push_back(turnCall(b, f, neverLands));
        check("turns/forget_presses_clears_the_turns", afterWaits == 3 && a.Counters(3).landed == 1
            && holder.front() == 3 && holder[180 - 91] == 3 && holder[181 - 91] == 4,
            "afterWaits=" + std::to_string(afterWaits) + " holder@91=" + std::to_string(holder.front()));
    }
    {
        // The bonus going inactive and active again starts the turns afresh
        // (an inactive call still reads, writes and counts nothing).
        liveTwoBBar();
        Mark1Automation a;
        turnCall(a, 0, alwaysLands);
        turnCall(a, 1, alwaysLands);
        const long counted = totalCounted(a);
        const long reads = bar.columnReads;
        gateCall(a, false, 2);
        const bool inactiveNothing = totalCounted(a) == counted && bar.columnReads == reads;
        check("turns/reactivation_clears_the_turns", inactiveNothing && turnCall(a, 3, neverLands) == 3);
    }
    {
        // A frame counter that went backwards ends a rest rather than leaving
        // it open-ended.
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101; bar.columns[4].talentId = 102;
        Mark1Automation a;
        int at90 = 0;
        for (long long f = 0; f <= kMark1TurnPatienceFrames; ++f) at90 = turnCall(a, f, neverLands);
        check("turns/frame_reset_ends_a_rest", at90 == 4 && turnCall(a, 5, neverLands) == 3);
    }

    // ---- no_starvation: the turn rule under the gate model ------------------------
    {
        const GateModel lo = runTurnsUnderModel(false);
        const GateModel hi = runTurnsUnderModel(true);
        auto fair = [](const GateModel& m) {
            const long d = m.landings[3] - m.landings[5];
            return d <= 1 && d >= -1 && m.landings[3] > 0 && m.landings[4] >= 3 && m.landings[6] == 0;
        };
        check("no_starvation/lowest_pressed_column_casts", fair(lo), landingsText(lo));
        check("no_starvation/highest_pressed_column_casts", fair(hi), landingsText(hi));
        bool same = true;
        for (int c = kMark1FirstColumn; c <= kMark1LastColumn; ++c) if (lo.landings[c] != hi.landings[c]) same = false;
        check("no_starvation/gate_order_does_not_change_the_result", same, landingsText(lo) + " vs " + landingsText(hi));
    }

    // ---- lone_column: the one eligible column is never rested ----------------------
    {
        resetBar(); resetSlot();
        bar.columns[3].talentId = 101;                                  // ordinary
        bar.columns[4].talentId = 102; bar.columns[4].aura = true;      // an aura
        bar.columns[5].talentId = 103; bar.columns[5].toggle = true; bar.columns[5].state = Mark1ToggleState::On;
        bar.columns[6].talentId = 0;                                    // empty
        Mark1Automation a;
        long pressedCalls = 0;
        for (long long f = 0; f < 1000; ++f) if (turnCall(a, f, neverLands) == 3) ++pressedCalls;
        check("lone_column/beside_ineligible_columns_pressed_every_call",
            pressedCalls == 1000 && a.Counters(3).yielded == 0 && a.Counters(4).skipTurn == 0
            && a.Counters(5).skipTurn == 0 && a.Counters(5).toggleOn == 1000,
            "pressed=" + std::to_string(pressedCalls));

        resetBar(); resetSlot();
        bar.columns[5].talentId = 103;
        Mark1Automation b;
        long pressedB = 0;
        for (long long f = 0; f < 1000; ++f) if (turnCall(b, f, [&](int c) { return c != 0 && f % 60 == 0; }) == 5) ++pressedB;
        check("lone_column/landing_keeps_it_pressed", pressedB == 1000 && b.Counters(5).landed == 17 && b.Counters(5).yielded == 0,
            "pressed=" + std::to_string(pressedB) + " landed=" + std::to_string(b.Counters(5).landed));
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
