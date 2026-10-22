#pragma once

#include "Common.hpp"
#include "Mark1Priority.hpp"
#include <atomic>
#include <exception>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
// unit-testable without a game process (tests/test_mark1_automation_behavior.py
// and tests/test_mark1_priority_behavior.py):
//   - which columns are eligible: an empty column (talent id 0) and an aura
//     are skipped; a toggle - a row of ToggleSkillMod.hpp's table - only while
//     the toggle read answers Off and outside the settle window after this
//     column's own last press, never while the read is Unreadable; anything
//     else is eligible on every gate call;
//   - which eligible column is pressed, and with which value, is the cast
//     priority's (Mark1Priority.hpp): cooldown columns first in Q, E, R, Y
//     order, a column with no cooldown pressed until it lands, the yield after
//     a landing and the rests. The adapter reports after each call whether a
//     cast landed (Landing); on the proc route the core names a column to fire
//     straight into `TalentUseClass` after the original, and the adapter
//     reports how that call went (ProcDone);
//   - which columns wear the toggle-skill border, and are drawn grey: every
//     automated column that is neither empty nor an aura;
//   - the writes around the original, each put back in every path: the
//     lockout (an automated column's own entry the player holds or presses is
//     rested), the press itself (the value and the column-to-entry mapping),
//     and the mask (another key held during a press is rested).
// The reads behind it (the bar element's talent id, aura flag and cooldown,
// the toggle state through ToggleIndicatorRead, the `inputState` entries of
// the profile slot the gate's third argument names) live in ModuleMain.cpp,
// resolved by name, and are handed in as callables. With the bonus inactive
// nothing is read, written or counted.
//
// Known limits, by design: a skill that wants a second input (a target
// click) or a held key is cast as one press each gate call; an automated
// toggle cannot be switched off while the bonus is active, because the guard
// re-presses it whenever it reads Off; a key the player presses during a gate
// call that pressed a column counts as that column's landing.

// What a key's press frame leaves in a profile slot's `inputState` entry
// (docs/mark1-set-research.md, Live 1 `slot-keys`: an entry rests at 1, reads
// 4 on the press frame, 3 while held and 2 on release). `cast-input` measured
// that writing it during the gate's own call, and restoring it after, casts
// that slot with no key pressed. A cooldown column is pressed with it; a free
// column with the hold value in force (Mark1Priority.hpp's
// kMark1FreePressValue).
inline constexpr double kMark1PressValue = 4.0;

// How many frames after its own press a toggle column is left alone, so the
// toggled-on instance has time to appear before the read is trusted again.
// Mod live 1's `toggle-settle:` measured a forced press switching Soul Spurn
// on 19 frames later (docs/mark1-set-research.md, which the behavior test pins
// this against); the window is that plus 10 frames of margin.
inline constexpr long long kMark1ToggleSettleFrames = 29;

// What ToggleIndicatorRead answered for a toggle column (the adapter maps
// ToggleIndicatorState onto this, so this header needs no game object table).
enum class Mark1ToggleState { On, Off, Unreadable };

// One bar column as the adapter read it at the point of use.
struct Mark1ColumnRead {
    bool   readable = false;  // the bar element and its talent id read as a number
    double talentId = 0;      // 0 (or below): an empty column
    bool   aura     = false;  // the column's `auraSkill` is set
    bool   toggle   = false;  // the talent is a row of ToggleSkillMod.hpp's table
    bool   cooldownRead    = false;  // the talent's `abilityCooldown` read as a number
    double abilityCooldown = 0;      // seconds, from `global.talentStructMap` (static data)
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
    SkipTurn,              // eligible, not pressed: the yield, a rest, another column pressing, or a proc not due
    Proc,                  // eligible, fired on the proc route after the original
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
        case Mark1ColumnDecision::Proc:                 return "proc";
        default:                                        return "toggle-unreadable";
    }
}

// One gate call's decision for every automated column. Plan presses at most
// one column, the priority's pick; a plan built by hand may press several.
struct Mark1FramePlan {
    bool active = false;
    long long frame = 0;
    int holder = 0;                 // the column Plan pressed this call; 0: none
    bool pressFree = false;         // the holder was pressed as a free column
    unsigned long long call = 0;    // which Plan call made it (0: not made by Plan); Landing matches on it
    Mark1ColumnDecision decision[kMark1ColumnCount] = {
        Mark1ColumnDecision::SkipInactive, Mark1ColumnDecision::SkipInactive,
        Mark1ColumnDecision::SkipInactive, Mark1ColumnDecision::SkipInactive,
    };
    double value[kMark1ColumnCount] = { kMark1PressValue, kMark1PressValue, kMark1PressValue, kMark1PressValue };
    unsigned lockMask = 0;          // bit i: column kMark1FirstColumn + i is locked out of manual casts this call
    int procColumn = 0;             // the column to fire after the original on the proc route; 0: none
    unsigned procMask = 0;          // bit i: column kMark1FirstColumn + i is on the proc route this call
    Mark1GateClock clock;           // the clock and stats the proc's interval is computed from
    Mark1PriorityColumn columns[kMark1ColumnCount];
    double talentId[kMark1ColumnCount] = {};

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
    // Whether the call reads the profile slot's entries at all: to press, or
    // to lock an automated column out.
    bool NeedsInput() const { return active && (PressCount() > 0 || lockMask != 0u); }
};

// How the adapter's proc call went: it returned, it threw, or it was never
// made (a name that did not resolve, a refused argument).
enum class Mark1ProcOutcome { Returned, Threw, Refused };

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
    long landed             = 0;  // a cast landed during a call that pressed this column (the press route; a proc counts procs= only)
    long skipTurn           = 0;  // eligible, not pressed: the yield, a rest, another column pressing, a proc not due
    long yielded            = 0;  // rests begun: a cooldown press that did not land, or a free column's patience out
};

class Mark1Automation {
public:
    static Mark1Automation& Instance() {
        static Mark1Automation s_Instance;
        return s_Instance;
    }

    // The cast priority, its settings in force (the research overrides) and
    // the second stat line's counters.
    Mark1PriorityCore& Priority() { return m_Core; }
    const Mark1PriorityCore& Priority() const { return m_Core; }

    // Decide every automated column for one gate call. `readColumn(int column)`
    // returns a Mark1ColumnRead; `readToggle(int column)` returns a
    // Mark1ToggleState and is asked only for a toggle column outside its
    // settle window. A read that throws counts as unreadable. Of the columns
    // that pass, the priority presses at most one and, on the proc route,
    // names at most one to fire; the others count skip-turn. Inactive: no
    // callable is invoked and nothing is counted, and the next active call
    // starts the priority afresh (the proc clocks kept, as on a room change).
    // With no clock nothing is fired.
    template <class ReadColumn, class ReadToggle>
    Mark1FramePlan Plan(bool bonusActive, long long frame, ReadColumn&& readColumn, ReadToggle&& readToggle)
    {
        return Plan(bonusActive, frame, Mark1GateClock{}, readColumn, readToggle);
    }

    template <class ReadColumn, class ReadToggle>
    Mark1FramePlan Plan(bool bonusActive, long long frame, const Mark1GateClock& clock, ReadColumn&& readColumn, ReadToggle&& readToggle)
    {
        Mark1FramePlan plan;
        plan.frame = frame;
        if (!bonusActive) {
            if (m_WasActive) ClearPriority();
            m_WasActive = false;
            return plan;
        }
        if (!m_WasActive) { ClearPriority(); m_WasActive = true; }
        plan.active = true;
        plan.call = ++m_PlanCalls;
        plan.clock = clock;
        bool eligible[kMark1ColumnCount] = {};
        unsigned bordered = 0u;
        for (int column = kMark1FirstColumn; column <= kMark1LastColumn; ++column) {
            const int i = column - kMark1FirstColumn;
            Mark1ColumnRead r;
            bool threw = false;
            try { r = readColumn(column); } catch (...) { threw = true; r = Mark1ColumnRead{}; }
            plan.decision[i] = threw ? Mark1ColumnDecision::SkipUnreadable : DecideColumn(column, frame, r, readToggle);
            eligible[i] = plan.decision[i] == Mark1ColumnDecision::Press;
            if (!threw && Bordered(r)) bordered |= 1u << i;
            plan.columns[i].kind = Mark1ClassifyColumn(eligible[i], r.toggle, r.cooldownRead, r.abilityCooldown);
            plan.columns[i].toggle = r.toggle;
            plan.columns[i].cooldownRead = r.cooldownRead;
            plan.columns[i].abilityCooldown = r.abilityCooldown;
            plan.talentId[i] = r.readable ? r.talentId : 0;
            m_LastTalent[i].store(r.readable && r.talentId > 0 ? r.talentId : 0.0);
            m_LastKind[i].store((int)plan.columns[i].kind);
        }
        if (m_Core.Settings().lockout) plan.lockMask = bordered;
        const Mark1PriorityDecision d = m_Core.Decide(frame, plan.columns, clock);
        for (int i = 0; i < kMark1ColumnCount; ++i) {
            if (eligible[i]) {
                const int column = kMark1FirstColumn + i;
                if (column == d.pressColumn) plan.decision[i] = Mark1ColumnDecision::Press;
                else if (column == d.procColumn) plan.decision[i] = Mark1ColumnDecision::Proc;
                else plan.decision[i] = Mark1ColumnDecision::SkipTurn;
            }
            Count(i, plan.decision[i]);
        }
        plan.holder = d.pressColumn;
        plan.pressFree = d.pressFree;
        if (Mark1IsAutomatedColumn(d.pressColumn))
            plan.value[d.pressColumn - kMark1FirstColumn] = d.pressFree ? m_Core.Settings().holdValue : kMark1PressValue;
        plan.procColumn = d.procColumn;
        plan.procMask = d.procMask;
        m_ProcCall = plan.procColumn ? plan.call : 0;
        return plan;
    }

    // What the adapter saw once the original gate returned or threw: whether
    // a cast landed during that call. It counts for the plan's holder only if
    // PressAround wrote its press in that same call, and only once. A landing
    // opens the yield window; a cooldown press that did not land rests that
    // column (Mark1Priority.hpp). Anything else is ignored.
    void Landing(const Mark1FramePlan& plan, bool landed)
    {
        if (!plan.active || plan.call == 0 || plan.call != m_PressedCall) return;
        if (!Mark1IsAutomatedColumn(plan.holder)) return;
        m_PressedCall = 0;
        const int i = plan.holder - kMark1FirstColumn;
        m_Core.Landed(plan.holder, plan.frame, landed, plan.pressFree);
        if (landed) ++m_Counters[i].landed;
    }

    // The proc call's arguments for the plan's proc column, from the shape in
    // force, the column's talent id and the local player's position. False
    // (and `why` set) when there is no proc column or an argument is refused.
    bool ProcArgs(const Mark1FramePlan& plan, double x, double y, std::vector<Mark1ProcValue>& out, std::string& why) const
    {
        out.clear();
        if (!plan.active || !Mark1IsAutomatedColumn(plan.procColumn)) { why = "no proc column"; return false; }
        return Mark1BuildProcArgs(m_Core.Settings().shape, plan.talentId[plan.procColumn - kMark1FirstColumn], x, y, out, why);
    }

    // How the proc call for the plan's proc column went, once per plan. A
    // call made counts procs=; one that returned starts the column's clock on
    // its interval (cooldown at the haste plus cast time at the Faster Cast
    // Rate the plan was made with) and answers true. It never counts landed=:
    // a call that returned is not a cast seen (Live 1), and landed= stays the
    // press route's. One that threw or was refused counts proc-fail= and
    // returns the route to the press for the session.
    bool ProcDone(const Mark1FramePlan& plan, Mark1ProcOutcome outcome)
    {
        if (!plan.active || plan.call == 0 || plan.call != m_ProcCall) return false;
        if (!Mark1IsAutomatedColumn(plan.procColumn)) return false;
        m_ProcCall = 0;
        const int i = plan.procColumn - kMark1FirstColumn;
        if (outcome != Mark1ProcOutcome::Refused) m_Core.CountProc();
        if (outcome == Mark1ProcOutcome::Returned) {
            m_Core.ProcFired(plan.procColumn, plan.clock.nowMs, m_Core.IntervalMs(plan.columns[i], plan.clock));
            return true;
        }
        m_Core.CountProcFail();
        m_Core.ProcFailed();
        return false;
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

    // The columns whose bar icon is drawn grey: the border's.
    template <class ReadColumn>
    unsigned GreyMask(bool bonusActive, ReadColumn&& readColumn) const
    {
        return Mark1GreyMask(BorderMask(bonusActive, readColumn));
    }

    static bool Bordered(const Mark1ColumnRead& r)
    {
        return r.readable && !IsEmpty(r) && !r.aura;
    }

    // Run `call` (the original gate) with the plan's writes in place, and put
    // each written entry's own original value back afterwards - after a
    // return and after a throw alike, the throw then rethrown. In order:
    //   - the lockout: each locked column's own entry that reads 3 or 4 is
    //     written 1 (locked= once per call);
    //   - the press: each planned column's entry gets its press value; an
    //     entry the lockout rested keeps the player's value as its original;
    //   - the mask, only once a press was written: each mask entry that reads
    //     3 or 4 is written 1 (masked=; held-seen= even with the mask off).
    // `read(int entry, Value& out) -> bool` and `write(int entry, const
    // Value&) -> bool` reach the profile slot's `inputState` entries; `Value`
    // is built from a double. An entry whose original cannot be read is never
    // written, because it could not be put back. Returns what `call` returns.
    template <class Value, class Read, class Write, class Call>
    decltype(auto) PressAround(const Mark1FramePlan& plan, Read&& read, Write&& write, Call&& call)
    {
        constexpr int kMaxWrites = 2 * kMark1ColumnCount + kMark1MaxMaskEntries;
        int writtenEntry[kMaxWrites] = {};
        int writtenColumn[kMaxWrites] = {};   // the column a restore failure counts for; -1: a mask entry
        Value writtenOriginal[kMaxWrites] = {};
        int written = 0;
        bool pressed[kMark1ColumnCount] = {};
        if (plan.active) {
            bool planned[kMark1ColumnCount] = {};
            for (int i = 0; i < kMark1ColumnCount; ++i) planned[i] = plan.decision[i] == Mark1ColumnDecision::Press;
            const Mark1InputPlan inputs = Mark1PlanInputs(true, true, plan.lockMask, planned, plan.value, m_Core.Settings().mask);

            // The lockout, before the press.
            bool lockedAny = false;
            for (int k = 0; k < inputs.lockCount; ++k) {
                const int entry = inputs.lockEntry[k];
                Value original{};
                bool readOk = false;
                try { readOk = read(entry, original); } catch (...) { readOk = false; }
                if (!readOk || !Asks(original)) continue;
                bool wrote = false;
                try { wrote = write(entry, Value(kMark1InputResting)); } catch (...) { wrote = false; }
                if (!wrote) { try { (void)write(entry, original); } catch (...) {} continue; }
                writtenEntry[written] = entry;
                writtenColumn[written] = entry;
                writtenOriginal[written] = original;
                ++written;
                lockedAny = true;
            }
            if (lockedAny) m_Core.CountLocked();

            // The press.
            Value originals[kMark1ColumnCount] = {};
            for (int column = kMark1FirstColumn; column <= kMark1LastColumn; ++column) {
                const int i = column - kMark1FirstColumn;
                if (!plan.Press(column)) continue;
                const int entry = Mark1InputEntryForColumn(column);
                int lockedAt = -1;
                for (int w = 0; w < written; ++w) if (writtenEntry[w] == entry) lockedAt = w;
                bool readOk = lockedAt >= 0;
                if (!readOk) {
                    try { readOk = read(entry, originals[i]); } catch (...) { readOk = false; }
                }
                if (!readOk) { ++m_Counters[i].originalUnreadable; continue; }
                bool wrote = false;
                try { wrote = write(entry, Value(plan.value[i])); } catch (...) { wrote = false; }
                if (!wrote) {
                    // The write may have landed partly; put the original back
                    // all the same (a locked entry's goes back with the
                    // lockout's), and never count it as a press.
                    ++m_Counters[i].pressWriteFailed;
                    if (lockedAt < 0) { try { (void)write(entry, originals[i]); } catch (...) {} }
                    continue;
                }
                if (lockedAt < 0) {
                    writtenEntry[written] = entry;
                    writtenColumn[written] = column;
                    writtenOriginal[written] = originals[i];
                    ++written;
                }
                pressed[i] = true;
                ++m_Counters[i].presses;
                m_LastPress[i] = plan.frame;
                m_HasPressed[i] = true;
                if (column == plan.holder) m_PressedCall = plan.call;
            }

            // The mask, only on a call that pressed.
            bool anyPressed = false;
            for (int i = 0; i < kMark1ColumnCount; ++i) anyPressed = anyPressed || pressed[i];
            if (anyPressed) {
                bool heldSeen = false, maskedAny = false;
                for (int k = 0; k < inputs.maskCount && written < kMaxWrites; ++k) {
                    const int entry = inputs.maskEntry[k];
                    Value original{};
                    bool readOk = false;
                    try { readOk = read(entry, original); } catch (...) { readOk = false; }
                    if (!readOk || !Asks(original)) continue;
                    heldSeen = true;
                    if (!inputs.maskWrites) continue;
                    bool wrote = false;
                    try { wrote = write(entry, Value(kMark1InputResting)); } catch (...) { wrote = false; }
                    if (!wrote) { try { (void)write(entry, original); } catch (...) {} continue; }
                    writtenEntry[written] = entry;
                    writtenColumn[written] = -1;
                    writtenOriginal[written] = original;
                    ++written;
                    maskedAny = true;
                }
                if (heldSeen) m_Core.CountHeldSeen();
                if (maskedAny) m_Core.CountMasked();
            }
        }
        auto restore = [&]() {
            for (int w = written - 1; w >= 0; --w) {
                bool ok = false;
                try { ok = write(writtenEntry[w], writtenOriginal[w]); } catch (...) { ok = false; }
                if (ok) continue;
                if (Mark1IsAutomatedColumn(writtenColumn[w])) ++m_Counters[writtenColumn[w] - kMark1FirstColumn].restoreFailed;
                else ++m_MaskRestoreFailed;
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
        const int i = column - kMark1FirstColumn;
        const Counter& k = m_Counters[i];
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
        c.yielded            = m_Core.Yielded(i);
        return c;
    }
    long CallsThrew() const { return m_CallsThrew.load(); }
    long MaskRestoreFailed() const { return m_MaskRestoreFailed.load(); }

    // The second stat line's `talents=` and `free=`: each column's talent id
    // at the last active gate call (0: empty or unread), and its kind.
    double LastTalent(int column) const
    {
        return Mark1IsAutomatedColumn(column) ? m_LastTalent[column - kMark1FirstColumn].load() : 0.0;
    }
    Mark1ColumnKind LastKind(int column) const
    {
        return Mark1IsAutomatedColumn(column) ? (Mark1ColumnKind)m_LastKind[column - kMark1FirstColumn].load()
                                              : Mark1ColumnKind::NotEligible;
    }

    // A room (the local player's instance changed: Player_obj gets a new
    // instance id in every room): every column's settle window is forgotten,
    // and so is the priority's frame state (the rests, the yield, the free
    // columns' turn). The proc clocks are kept, so entering a room does not
    // fire a cooldown skill before its interval has run.
    void ForgetRoom()
    {
        for (int i = 0; i < kMark1ColumnCount; ++i) { m_HasPressed[i] = false; m_LastPress[i] = 0; }
        ClearPriority();
    }

    // A new character: everything a room forgets, and every proc clock.
    void ForgetCharacter()
    {
        ForgetRoom();
        m_Core.Reset();
    }

private:
    struct Counter {
        std::atomic<long> presses{ 0 }, unreadable{ 0 }, empty{ 0 }, aura{ 0 },
            toggleSettling{ 0 }, toggleOn{ 0 }, toggleUnreadable{ 0 },
            originalUnreadable{ 0 }, pressWriteFailed{ 0 }, restoreFailed{ 0 },
            landed{ 0 }, skipTurn{ 0 };
    };

    static bool IsEmpty(const Mark1ColumnRead& r) { return !(r.talentId > 0); }

    // An entry's original reads held (3) or pressed (4); a value that does
    // not read as a number does not ask.
    template <class Value>
    static bool Asks(const Value& v)
    {
        try {
            if constexpr (std::is_arithmetic_v<Value>) return Mark1EntryAsks(true, (double)v);
            else return Mark1EntryAsks(true, v.ToDouble());
        } catch (...) { return false; }
    }

    // The priority's frame state, for a room or the bonus active again; the
    // proc clocks are kept (only ForgetCharacter clears them).
    void ClearPriority()
    {
        m_Core.ForgetRoom();
        m_PressedCall = 0;
        m_ProcCall = 0;
    }

    // Within the window after this column's own last press. A frame counter
    // that went backwards ends the window rather than leaving it open-ended.
    bool Settling(int i, long long frame) const
    {
        if (!m_HasPressed[i]) return false;
        if (frame < m_LastPress[i]) return false;
        return frame - m_LastPress[i] < kMark1ToggleSettleFrames;
    }

    template <class ReadToggle>
    Mark1ColumnDecision DecideColumn(int column, long long frame, const Mark1ColumnRead& r, ReadToggle& readToggle) const
    {
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
            default: break;   // a press is counted when it is written (PressAround), a proc when it is done (ProcDone)
        }
    }

    Mark1PriorityCore m_Core;
    Counter m_Counters[kMark1ColumnCount];
    std::atomic<long> m_CallsThrew{ 0 };
    std::atomic<long> m_MaskRestoreFailed{ 0 };
    long long m_LastPress[kMark1ColumnCount] = {};
    bool m_HasPressed[kMark1ColumnCount] = {};
    std::atomic<double> m_LastTalent[kMark1ColumnCount] = {};
    std::atomic<int> m_LastKind[kMark1ColumnCount] = {};
    unsigned long long m_PlanCalls = 0;
    unsigned long long m_PressedCall = 0;   // the Plan call whose holder PressAround last pressed; 0: none pending
    unsigned long long m_ProcCall = 0;      // the Plan call whose proc is not yet reported; 0: none pending
    bool m_WasActive = false;
};

} // namespace ForgePact
