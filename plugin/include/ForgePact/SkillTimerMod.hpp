#pragma once

#include "Common.hpp"

namespace ForgePact {

// Timed-skill countdown (issue #55): draws how much of a timed cast is left
// over the skill's own hotbar slot. This header holds only the
// game-independent decision - no RValue, no CallBuiltin - so it is
// unit-testable without a game process, the same split ToggleSkillMod.hpp
// uses for the border's decision. The scan that finds each row's own
// instances and reads their `destroyTimer` lives in ModuleMain.cpp, outside
// any research block, next to ToggleIndicatorReadRow which this reuses the
// ownership rule of.
enum class SkillTimerStyle { Off, Arc, Bar, Number, Fade };

inline bool SkillTimerStyleFromName(const std::string& lower, SkillTimerStyle& out)
{
    if (lower == "off" || lower == "0") { out = SkillTimerStyle::Off; return true; }
    if (lower == "arc")    { out = SkillTimerStyle::Arc;    return true; }
    if (lower == "bar")    { out = SkillTimerStyle::Bar;    return true; }
    if (lower == "number") { out = SkillTimerStyle::Number; return true; }
    if (lower == "fade")   { out = SkillTimerStyle::Fade;   return true; }
    return false;
}

inline const char* SkillTimerStyleName(SkillTimerStyle s)
{
    switch (s) {
        case SkillTimerStyle::Off:    return "off";
        case SkillTimerStyle::Arc:    return "arc";
        case SkillTimerStyle::Bar:    return "bar";
        case SkillTimerStyle::Number: return "number";
        case SkillTimerStyle::Fade:   return "fade";
    }
    return "off";
}

// Route B (user decision, 2026-09-21): the fraction is the row's own shared
// timer field, current value divided by the FIRST value latched when the
// instance first appeared, clamped to 0..1. Named once here - the same
// timer field kToggleSkillRows' Maelstrom row also names as its own
// discriminator - so ModuleMain.cpp's read never restates the literal
// (ToggleSkillMod.hpp's own rule: "Every runtime name ... lives HERE and
// nowhere else in the plugin").
inline constexpr const char* kSkillTimerField = "destroyTimer";

// One of these per shipped row.
struct SkillTimerRowState {
    bool   latched = false;
    double latch = 0.0;
};

// What one draw's read decided, before any game call is made: whether an
// own instance existed at all, whether any own instance's own `destroyTimer`
// was readable, and (only when readable) the largest such value among them
// (D-T6) - the caller's job, since it needs the game API. Everything below is
// decided from those three inputs alone.
enum class SkillTimerOutcome {
    NoInstance,   // object resolves, zero own instances (or the object itself is unresolved)
    Unreadable,   // own instances exist, none has a numeric destroyTimer
    Expired,      // remaining <= 0 - never latches
    Drawn,        // remaining > 0 - latch held or (re-)taken, fraction is valid
};

struct SkillTimerDecision {
    SkillTimerOutcome outcome = SkillTimerOutcome::NoInstance;
    // Whether THIS call took or updated the latch ("latched=") or dropped an
    // existing one ("unlatched="), so the caller can count exactly the
    // transitions context calls for rather than every Drawn/NoInstance call.
    bool latchedThisCall = false;
    bool unlatchedThisCall = false;
    // Valid only when outcome == Drawn.
    double fraction = 0.0;
};

class SkillTimerModel {
public:
    static SkillTimerDecision Decide(SkillTimerRowState& state, bool anyOwnInstance,
                                      bool anyReadableTimer, double remaining)
    {
        SkillTimerDecision d;
        if (!anyOwnInstance) {
            // An instance first appearing is the only re-sync signal (a rise
            // in `remaining`, handled below); an instance disappearing drops
            // whatever latch was held so the NEXT appearance re-latches full
            // rather than dividing by a stale value.
            if (state.latched) {
                state.latched = false;
                d.unlatchedThisCall = true;
            }
            d.outcome = SkillTimerOutcome::NoInstance;
            return d;
        }
        if (!anyReadableTimer) {
            // Own instances exist but none has a numeric destroyTimer: the
            // latch is left exactly as it was - neither taken nor dropped -
            // since this call learned nothing about whether the cast is
            // still the same one.
            d.outcome = SkillTimerOutcome::Unreadable;
            return d;
        }
        if (remaining <= 0.0) {
            // Maelstrom's held -1, Blender's trailing -1, a plain cast's
            // -0.6 tail: never latch on a non-positive reading. The latch
            // itself is left exactly as it was (only the !anyOwnInstance
            // branch above ever drops it) - pinned by
            // skilltimer/non_positive_draws_nothing_and_never_latches. A
            // shorter later cast that starts below its own true 100% and
            // never corrects is the accepted, documented cost of that pin
            // (docs/toggle-skills-research.md "Mid-cast limitation").
            d.outcome = SkillTimerOutcome::Expired;
            return d;
        }
        if (!state.latched || remaining > state.latch) {
            // No latch yet, or a rise - the rise case IS "an instance first
            // appears" for a value that only ever falls within one cast, so
            // it is also the only signal that corrects a latch taken
            // mid-cast.
            state.latched = true;
            state.latch = remaining;
            d.latchedThisCall = true;
        }
        double fraction = remaining / state.latch;
        // The file bans bare std::max/std::min (test_no_bare_std_max_or_std_min).
        if (fraction < 0.0) fraction = 0.0;
        if (fraction > 1.0) fraction = 1.0;
        d.fraction = fraction;
        d.outcome = SkillTimerOutcome::Drawn;
        return d;
    }
};

} // namespace ForgePact
