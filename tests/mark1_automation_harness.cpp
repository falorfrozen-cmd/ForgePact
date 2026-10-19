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
           + k.toggleUnreadable + k.originalUnreadable + k.pressWriteFailed + k.restoreFailed;
    }
    return n + a.CallsThrew();
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
        // frames) never leaves a toggle stuck inside a window that cannot end.
        double sr[10] = {};
        gateCall(c, true, 5, sr);
        check("toggle/settle_survives_frame_reset", sr[3] == kMark1PressValue && sr[6] == kMark1PressValue);

        check("toggle/settle_constant_is_30", kMark1ToggleSettleFrames == 30);
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
        const double result = gateCall(a, true, 0, seen);
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
        const Mark1FramePlan plan = a.Plan(true, 0, readColumn, readToggle);
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
        gateCall(a, true, 0, seen);
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

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
