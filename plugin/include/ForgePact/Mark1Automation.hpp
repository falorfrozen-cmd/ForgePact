#pragma once

#include "Common.hpp"
#include <atomic>
#include <exception>
#include <type_traits>
#include <utility>

namespace ForgePact {

// Mark 1 set, Automation (issue #166, phase 2): while the full set is worn
// and its switch is on, the skills on the bar's Q, E, R and Y slots - bar
// columns 3, 4, 5 and 6 of `UI_Hud_Talent_obj.row0` (RDM § 8.3) - are cast
// for the player whenever the game's own gate allows. The cast is asked for
// exactly the way a held key asks for it: inside the `CheckTalentUse`
// detour, before the original runs, the profile slot's `inputState` entry of
// the column's own index is set to the press value, and the original value is
// put back once the original has returned or thrown. The gate then decides
// everything it always decides (cooldown, resource, requirement); this mod
// never writes a cooldown, mana or talent state and never presses a key.
//
// This header holds only the game-independent part, so every branch is
// unit-testable without a game process (tests/test_mark1_automation_behavior.py):
//   - which columns are eligible: an empty column (talent id 0) and an aura
//     are skipped; a toggle - a row of ToggleSkillMod.hpp's table - only while
//     the toggle read answers Off and outside the settle window after this
//     column's own last press, never while the read is Unreadable; anything
//     else is eligible on every gate call;
//   - the turn rule: one eligible column is pressed per gate call, the one
//     that has waited longest since its last landed cast (ties to the lower
//     column), so a skill with no cooldown cannot keep the others from ever
//     being cast (Live 2b: Shadow Bolt on Q took 20 casts, Heavenly Fire on R
//     none, with every column pressed on every call). Which of several
//     pressed slots the game's gate takes is not established, so the rule
//     never presses two. The adapter reports after each call whether a cast
//     landed (Landing); a landing sends that column to the back of the
//     queue. A holder pressed for kMark1TurnPatienceFrames without a landing
//     (on cooldown, short of mana) rests for kMark1TurnRestFrames, unless no
//     other eligible column is free to take the turn;
//   - which columns wear the toggle-skill border: every automated column
//     that is neither empty nor an aura;
//   - the press itself: the value, the column-to-entry mapping, and the
//     restore in every path.
// The reads behind it (the bar element's talent id and aura flag, the toggle
// state through ToggleIndicatorRead, the `inputState` entries of the profile
// slot the gate's third argument names) live in ModuleMain.cpp, resolved by
// name, and are handed in as callables. With the bonus inactive nothing is
// read, written or counted.
//
// Known limits, by design: a skill that wants a second input (a target
// click) or a held key is cast as one press each gate call; an automated
// toggle cannot be switched off while the bonus is active, because the guard
// re-presses it whenever it reads Off; a key the player presses during a gate
// call that pressed a column counts as that column's landing.

// The automated bar columns (Q, E, R, Y).
inline constexpr int kMark1FirstColumn = 3;
inline constexpr int kMark1LastColumn = 6;
inline constexpr int kMark1ColumnCount = kMark1LastColumn - kMark1FirstColumn + 1;

// What a key's press frame leaves in a profile slot's `inputState` entry
// (docs/mark1-set-research.md, Live 1 `slot-keys`: an entry rests at 1, reads
// 4 on the press frame, 3 while held and 2 on release). `cast-input` measured
// that writing it during the gate's own call, and restoring it after, casts
// that slot with no key pressed.
inline constexpr double kMark1PressValue = 4.0;

// How many frames after its own press a toggle column is left alone, so the
// toggled-on instance has time to appear before the read is trusted again.
// Mod live 1's `toggle-settle:` measured a forced press switching Soul Spurn
// on 19 frames later (docs/mark1-set-research.md, which the behavior test pins
// this against); the window is that plus 10 frames of margin.
inline constexpr long long kMark1ToggleSettleFrames = 29;

// The turn rule's two windows, both provisional (Live 1 of the turns
// workorder records the casts per column they give). Patience has to outlast
// the gap between two casts while every column is refused: Live 2b saw Shadow
// Bolt cast about every 60 frames at 144 fps, and a held key repeats every 57
// frames (RDM § 7.2). The rest, 2 s at 144 fps, caps what a column on cooldown
// costs the others at about a quarter of the time.
inline constexpr long long kMark1TurnPatienceFrames = 90;
inline constexpr long long kMark1TurnRestFrames = 288;

inline constexpr bool Mark1IsAutomatedColumn(int column)
{
    return column >= kMark1FirstColumn && column <= kMark1LastColumn;
}

// The `inputState` entry a column presses: the entry of the same index.
// Anything outside the automated columns has no entry (-1).
inline constexpr int Mark1InputEntryForColumn(int column)
{
    return Mark1IsAutomatedColumn(column) ? column : -1;
}

// What ToggleIndicatorRead answered for a toggle column (the adapter maps
// ToggleIndicatorState onto this, so this header needs no game object table).
enum class Mark1ToggleState { On, Off, Unreadable };

// One bar column as the adapter read it at the point of use.
struct Mark1ColumnRead {
    bool   readable = false;  // the bar element and its talent id read as a number
    double talentId = 0;      // 0 (or below): an empty column
    bool   aura     = false;  // the column's `auraSkill` is set
    bool   toggle   = false;  // the talent is a row of ToggleSkillMod.hpp's table
};

enum class Mark1ColumnDecision {
    Press,
    SkipInactive,          // the bonus is not active: nothing was read
    SkipUnreadable,        // the column itself could not be read
    SkipEmpty,
    SkipAura,
    SkipToggleSettling,    // inside this column's own settle window
    SkipToggleOn,
    SkipToggleUnreadable,
    SkipTurn,              // eligible, but another column holds this call's turn (or this one rests)
};

inline const char* Mark1ColumnDecisionName(Mark1ColumnDecision d)
{
    switch (d) {
        case Mark1ColumnDecision::Press:                return "press";
        case Mark1ColumnDecision::SkipInactive:         return "inactive";
        case Mark1ColumnDecision::SkipUnreadable:       return "unreadable";
        case Mark1ColumnDecision::SkipEmpty:            return "empty";
        case Mark1ColumnDecision::SkipAura:             return "aura";
        case Mark1ColumnDecision::SkipToggleSettling:   return "settling";
        case Mark1ColumnDecision::SkipToggleOn:         return "toggle-on";
        case Mark1ColumnDecision::SkipTurn:             return "skip-turn";
        default:                                        return "toggle-unreadable";
    }
}

// One gate call's decision for every automated column. Plan presses at most
// one column, the turn's holder; a plan built by hand may press several.
struct Mark1FramePlan {
    bool active = false;
    long long frame = 0;
    int holder = 0;                 // the column Plan gave this call's turn; 0: none
    unsigned long long call = 0;    // which Plan call made it (0: not made by Plan); Landing matches on it
    Mark1ColumnDecision decision[kMark1ColumnCount] = {
        Mark1ColumnDecision::SkipInactive, Mark1ColumnDecision::SkipInactive,
        Mark1ColumnDecision::SkipInactive, Mark1ColumnDecision::SkipInactive,
    };

    bool Press(int column) const
    {
        return Mark1IsAutomatedColumn(column)
            && decision[column - kMark1FirstColumn] == Mark1ColumnDecision::Press;
    }
    int PressCount() const
    {
        int n = 0;
        for (int i = 0; i < kMark1ColumnCount; ++i) if (decision[i] == Mark1ColumnDecision::Press) ++n;
        return n;
    }
};

// Per-column counters, for the `mark1set stat` line: every press and every
// reason a column was not pressed, so "reports itself on while doing
// nothing" shows up as a skip count instead of as silence.
struct Mark1ColumnCounters {
    long presses            = 0;  // the press value was written for one gate call
    long unreadable         = 0;
    long empty              = 0;
    long aura               = 0;
    long toggleSettling     = 0;
    long toggleOn           = 0;
    long toggleUnreadable   = 0;
    long originalUnreadable = 0;  // planned, but the entry's own value could not be read to put back
    long pressWriteFailed   = 0;
    long restoreFailed      = 0;
    long landed             = 0;  // a cast landed during a call that pressed this column
    long skipTurn           = 0;  // eligible, not pressed: another column held the turn, or this one rested
    long yielded            = 0;  // gave up the turn after kMark1TurnPatienceFrames without a landing
};

class Mark1Automation {
public:
    static Mark1Automation& Instance() {
        static Mark1Automation s_Instance;
        return s_Instance;
    }

    // Decide every automated column for one gate call. `readColumn(int column)`
    // returns a Mark1ColumnRead; `readToggle(int column)` returns a
    // Mark1ToggleState and is asked only for a toggle column outside its
    // settle window. A read that throws counts as unreadable. Of the columns
    // that pass, only the turn's holder is pressed; the others count
    // skip-turn. Inactive: no callable is invoked and nothing is counted, and
    // the next active call starts the turns afresh.
    template <class ReadColumn, class ReadToggle>
    Mark1FramePlan Plan(bool bonusActive, long long frame, ReadColumn&& readColumn, ReadToggle&& readToggle)
    {
        Mark1FramePlan plan;
        plan.frame = frame;
        if (!bonusActive) { m_WasActive = false; return plan; }
        if (!m_WasActive) { ClearTurns(); m_WasActive = true; }
        plan.active = true;
        plan.call = ++m_PlanCalls;
        bool eligible[kMark1ColumnCount] = {};
        for (int column = kMark1FirstColumn; column <= kMark1LastColumn; ++column) {
            const int i = column - kMark1FirstColumn;
            plan.decision[i] = DecideColumn(column, frame, readColumn, readToggle);
            eligible[i] = plan.decision[i] == Mark1ColumnDecision::Press;
        }
        const int holder = TakeTurn(eligible, frame);
        for (int i = 0; i < kMark1ColumnCount; ++i) {
            if (eligible[i] && i != holder) plan.decision[i] = Mark1ColumnDecision::SkipTurn;
            Count(i, plan.decision[i]);
        }
        if (holder >= 0) plan.holder = kMark1FirstColumn + holder;
        return plan;
    }

    // What the adapter saw once the original gate returned or threw: whether
    // a cast landed during that call. A landing counts for the plan's holder
    // only if PressAround wrote its press in that same call, and only once;
    // it restarts the holder's wait, so the next call goes to whichever column
    // has now waited longest. Anything else is ignored.
    void Landing(const Mark1FramePlan& plan, bool landed)
    {
        if (!landed || !plan.active || plan.call == 0 || plan.call != m_PressedCall) return;
        if (!Mark1IsAutomatedColumn(plan.holder)) return;
        m_PressedCall = 0;
        const int i = plan.holder - kMark1FirstColumn;
        m_LandedAt[i] = ++m_Landings;
        m_Resting[i] = false;
        if (m_RunColumn == i) m_RunColumn = -1;
        ++m_Counters[i].landed;
    }

    // The automated columns that wear the toggle-skill border: bit (column -
    // kMark1FirstColumn) is set for every column that reads as neither empty
    // nor an aura. Inactive: nothing is read and no bit is set.
    template <class ReadColumn>
    unsigned BorderMask(bool bonusActive, ReadColumn&& readColumn) const
    {
        if (!bonusActive) return 0u;
        unsigned mask = 0u;
        for (int column = kMark1FirstColumn; column <= kMark1LastColumn; ++column) {
            Mark1ColumnRead r;
            try { r = readColumn(column); } catch (...) { continue; }
            if (Bordered(r)) mask |= 1u << (column - kMark1FirstColumn);
        }
        return mask;
    }

    static bool Bordered(const Mark1ColumnRead& r)
    {
        return r.readable && !IsEmpty(r) && !r.aura;
    }

    // Run `call` (the original gate) with every planned column pressed, and
    // put each pressed entry's own original value back afterwards - after a
    // return and after a throw alike, the throw then rethrown. `read(int
    // entry, Value& out) -> bool` and `write(int entry, const Value&) -> bool`
    // reach the profile slot's `inputState` entries; `Value` is built from
    // kMark1PressValue for the press. An entry whose original cannot be read
    // is never pressed, because it could not be put back. Returns what `call`
    // returns.
    template <class Value, class Read, class Write, class Call>
    decltype(auto) PressAround(const Mark1FramePlan& plan, Read&& read, Write&& write, Call&& call)
    {
        Value originals[kMark1ColumnCount] = {};
        bool pressed[kMark1ColumnCount] = {};
        if (plan.active) {
            for (int column = kMark1FirstColumn; column <= kMark1LastColumn; ++column) {
                const int i = column - kMark1FirstColumn;
                if (!plan.Press(column)) continue;
                const int entry = Mark1InputEntryForColumn(column);
                bool readOk = false;
                try { readOk = read(entry, originals[i]); } catch (...) { readOk = false; }
                if (!readOk) { ++m_Counters[i].originalUnreadable; continue; }
                bool wrote = false;
                try { wrote = write(entry, Value(kMark1PressValue)); } catch (...) { wrote = false; }
                if (!wrote) {
                    // The write may have landed partly; put the original back
                    // all the same, and never count it as a press.
                    ++m_Counters[i].pressWriteFailed;
                    try { (void)write(entry, originals[i]); } catch (...) {}
                    continue;
                }
                pressed[i] = true;
                ++m_Counters[i].presses;
                m_LastPress[i] = plan.frame;
                m_HasPressed[i] = true;
                if (column == plan.holder) m_PressedCall = plan.call;
            }
        }
        auto restore = [&]() {
            for (int i = 0; i < kMark1ColumnCount; ++i) {
                if (!pressed[i]) continue;
                bool ok = false;
                try { ok = write(Mark1InputEntryForColumn(kMark1FirstColumn + i), originals[i]); } catch (...) { ok = false; }
                if (!ok) ++m_Counters[i].restoreFailed;
            }
        };
        // The put-back runs on the way out either way, from a guard rather
        // than a catch that rethrows: the exception reaches the adapter
        // untouched, and the adapter's own catch writes the rethrow line
        // before the game gets it back (test_crash_trap_contract.py).
        struct PutBack {
            Mark1Automation& self;
            decltype(restore)& run;
            const int uncaught = std::uncaught_exceptions();
            ~PutBack()
            {
                if (std::uncaught_exceptions() > uncaught) ++self.m_CallsThrew;
                run();
            }
        } putBack{ *this, restore };
        using Result = decltype(call());
        if constexpr (std::is_void_v<Result>) {
            call();
            return;
        } else {
            decltype(auto) result = call();
            return static_cast<Result>(result);
        }
    }

    Mark1ColumnCounters Counters(int column) const
    {
        Mark1ColumnCounters c;
        if (!Mark1IsAutomatedColumn(column)) return c;
        const Counter& k = m_Counters[column - kMark1FirstColumn];
        c.presses            = k.presses.load();
        c.unreadable         = k.unreadable.load();
        c.empty              = k.empty.load();
        c.aura               = k.aura.load();
        c.toggleSettling     = k.toggleSettling.load();
        c.toggleOn           = k.toggleOn.load();
        c.toggleUnreadable   = k.toggleUnreadable.load();
        c.originalUnreadable = k.originalUnreadable.load();
        c.pressWriteFailed   = k.pressWriteFailed.load();
        c.restoreFailed      = k.restoreFailed.load();
        c.landed             = k.landed.load();
        c.skipTurn           = k.skipTurn.load();
        c.yielded            = k.yielded.load();
        return c;
    }
    long CallsThrew() const { return m_CallsThrew.load(); }

    // A new character or room: every column's settle window is forgotten, and
    // so are the turns (the waits, the rests and the holder's run).
    void ForgetPresses()
    {
        for (int i = 0; i < kMark1ColumnCount; ++i) { m_HasPressed[i] = false; m_LastPress[i] = 0; }
        ClearTurns();
    }

private:
    struct Counter {
        std::atomic<long> presses{ 0 }, unreadable{ 0 }, empty{ 0 }, aura{ 0 },
            toggleSettling{ 0 }, toggleOn{ 0 }, toggleUnreadable{ 0 },
            originalUnreadable{ 0 }, pressWriteFailed{ 0 }, restoreFailed{ 0 },
            landed{ 0 }, skipTurn{ 0 }, yielded{ 0 };
    };

    static bool IsEmpty(const Mark1ColumnRead& r) { return !(r.talentId > 0); }

    void ClearTurns()
    {
        for (int i = 0; i < kMark1ColumnCount; ++i) { m_LandedAt[i] = 0; m_Resting[i] = false; m_RestStart[i] = 0; }
        m_Landings = 0;
        m_RunColumn = -1;
        m_RunStart = 0;
        m_PressedCall = 0;
    }

    // Column index a has waited longer than b: waits are ordered by landing,
    // not by frame, so a frame counter that goes backwards cannot reorder
    // them. Never landed (0) is the longest wait; ties go to the lower column.
    bool WaitedLonger(int a, int b) const
    {
        if (m_LandedAt[a] != m_LandedAt[b]) return m_LandedAt[a] < m_LandedAt[b];
        return a < b;
    }

    // Inside this column's rest. A frame counter that went backwards ends the
    // rest rather than leaving it open-ended.
    bool Resting(int i, long long frame) const
    {
        if (!m_Resting[i]) return false;
        if (frame < m_RestStart[i]) return false;
        return frame - m_RestStart[i] < kMark1TurnRestFrames;
    }

    // The eligible column, not resting, that has waited longest; -1 when
    // none. With `ignoreRest` a resting column counts as free.
    int Longest(const bool (&eligible)[kMark1ColumnCount], long long frame, bool ignoreRest, int except = -1) const
    {
        int best = -1;
        for (int i = 0; i < kMark1ColumnCount; ++i) {
            if (!eligible[i] || i == except) continue;
            if (!ignoreRest && Resting(i, frame)) continue;
            if (best < 0 || WaitedLonger(i, best)) best = i;
        }
        return best;
    }

    // This call's holder (a column index, -1 when nothing is eligible). A
    // holder whose run has lasted kMark1TurnPatienceFrames rests, and counts
    // yielded, when another eligible column is free to take the turn; its
    // wait is not restarted. When every eligible column rests, the longest
    // waiting is pressed all the same: a lone eligible column is pressed on
    // every call.
    int TakeTurn(const bool (&eligible)[kMark1ColumnCount], long long frame)
    {
        int holder = Longest(eligible, frame, false);
        if (holder >= 0 && holder == m_RunColumn) {
            if (frame < m_RunStart) m_RunStart = frame;
            if (frame - m_RunStart >= kMark1TurnPatienceFrames) {
                const int next = Longest(eligible, frame, false, holder);
                if (next >= 0) {
                    m_Resting[holder] = true;
                    m_RestStart[holder] = frame;
                    ++m_Counters[holder].yielded;
                    holder = next;
                }
            }
        }
        if (holder < 0) holder = Longest(eligible, frame, true);
        if (holder != m_RunColumn) { m_RunColumn = holder; m_RunStart = frame; }
        return holder;
    }

    // Within the window after this column's own last press. A frame counter
    // that went backwards ends the window rather than leaving it open-ended.
    bool Settling(int i, long long frame) const
    {
        if (!m_HasPressed[i]) return false;
        if (frame < m_LastPress[i]) return false;
        return frame - m_LastPress[i] < kMark1ToggleSettleFrames;
    }

    template <class ReadColumn, class ReadToggle>
    Mark1ColumnDecision DecideColumn(int column, long long frame, ReadColumn& readColumn, ReadToggle& readToggle) const
    {
        Mark1ColumnRead r;
        try { r = readColumn(column); } catch (...) { return Mark1ColumnDecision::SkipUnreadable; }
        if (!r.readable) return Mark1ColumnDecision::SkipUnreadable;
        if (IsEmpty(r)) return Mark1ColumnDecision::SkipEmpty;
        if (r.aura) return Mark1ColumnDecision::SkipAura;
        if (!r.toggle) return Mark1ColumnDecision::Press;
        const int i = column - kMark1FirstColumn;
        if (Settling(i, frame)) return Mark1ColumnDecision::SkipToggleSettling;
        Mark1ToggleState state = Mark1ToggleState::Unreadable;
        try { state = readToggle(column); } catch (...) { state = Mark1ToggleState::Unreadable; }
        if (state == Mark1ToggleState::Off) return Mark1ColumnDecision::Press;
        if (state == Mark1ToggleState::On) return Mark1ColumnDecision::SkipToggleOn;
        return Mark1ColumnDecision::SkipToggleUnreadable;
    }

    void Count(int i, Mark1ColumnDecision d)
    {
        Counter& k = m_Counters[i];
        switch (d) {
            case Mark1ColumnDecision::SkipUnreadable:       ++k.unreadable; break;
            case Mark1ColumnDecision::SkipEmpty:            ++k.empty; break;
            case Mark1ColumnDecision::SkipAura:             ++k.aura; break;
            case Mark1ColumnDecision::SkipToggleSettling:   ++k.toggleSettling; break;
            case Mark1ColumnDecision::SkipToggleOn:         ++k.toggleOn; break;
            case Mark1ColumnDecision::SkipToggleUnreadable: ++k.toggleUnreadable; break;
            case Mark1ColumnDecision::SkipTurn:             ++k.skipTurn; break;
            default: break;   // a press is counted when it is written (PressAround)
        }
    }

    Counter m_Counters[kMark1ColumnCount];
    std::atomic<long> m_CallsThrew{ 0 };
    long long m_LastPress[kMark1ColumnCount] = {};
    bool m_HasPressed[kMark1ColumnCount] = {};

    // The turns. m_LandedAt orders the waits (0: no landing since the turns
    // were last cleared); the run is how long the current holder has held the
    // turn without a landing.
    unsigned long long m_LandedAt[kMark1ColumnCount] = {};
    unsigned long long m_Landings = 0;
    bool m_Resting[kMark1ColumnCount] = {};
    long long m_RestStart[kMark1ColumnCount] = {};
    int m_RunColumn = -1;
    long long m_RunStart = 0;
    unsigned long long m_PlanCalls = 0;
    unsigned long long m_PressedCall = 0;   // the Plan call whose holder PressAround last pressed; 0: none pending
    bool m_WasActive = false;
};

} // namespace ForgePact
