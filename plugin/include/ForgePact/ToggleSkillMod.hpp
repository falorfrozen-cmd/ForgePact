#pragma once

#include "Common.hpp"

namespace ForgePact {

// Toggle-skill active indicator (issue #11, Track B): outlines Soul Spurn's
// skill-bar slot while the Purgatory-toggled AOE is live. This header holds
// only the game-independent decision - no RValue, no CallBuiltin - so every
// branch is unit-testable without a game process. The actual read (resolving
// the AOE object, counting instances, reading each one's own `isMyClient`
// and `purgatory`) lives in ModuleMain.cpp, outside any research block,
// because it leans on the SDK object table which this header does not pull
// in (docs/toggle-skills-research.md, "## Decision" -> "### P1: the
// indicator's read, control and slot design" -> "The read, and exactly what
// has been proven" and "Co-op / ownership after session 3: isMyClient").
enum class ToggleIndicatorState { On, Off, Unreadable };

// Everything one enumeration pass over the AOE object measured. `objectResolved
// = false` means the object name itself did not resolve (asset_get_index < 0)
// - a different failure than "resolved but zero instances", which is Off, not
// Unreadable. Ownership is decided per scanned instance from its own
// `isMyClient` field, not by comparing against anything read off the local
// player: `mine`/`others`/`unattributed` classify every AOE instance the scan
// actually visited (bounded by the scan cap, see `capped`) by that field alone
// - a `VALUE_BOOL` gives its truth, a numeric kind counts nonzero as true, and
// anything else (undefined, a string, or a throw) is unattributed and never
// lights the indicator. `markedMine`/`unmarkedMine`/`markUnreadableMine`
// further split the `mine` instances by their own `purgatory` field, read only
// for instances already classified as `mine` (docs/toggle-skills-research.md,
// "## Decision" -> "### P1: the indicator's read, control and slot design" ->
// "Co-op / ownership after session 3: isMyClient" and "Plain-cast flash (R10)
// and the Purgatory marker").
struct ToggleIndicatorReadDetail {
    bool   objectResolved      = false;
    long   n                   = 0;      // instance_number of the AOE object
    long   mine                = 0;      // own AOE instances (isMyClient true)
    long   others               = 0;      // foreign AOE instances (isMyClient false)
    long   unattributed        = 0;      // isMyClient unreadable (undefined/string/throw)
    long   markedMine          = 0;      // own instances whose own purgatory reads numeric > 0
    long   unmarkedMine        = 0;      // own instances whose own purgatory reads numeric <= 0
    long   markUnreadableMine  = 0;      // own instances whose own purgatory could not be read
    bool   capped              = false;  // the scan hit its instance budget
    // Set when the instance_number call itself threw, instead of answering
    // with a real count. Without this flag the catch's `d.n = 0` fallback is
    // indistinguishable from a real, measured zero, so a failed read would
    // silently decide Off - a quietly absent indicator that raises no
    // counter, rather than a failure a `tgprobe spurn` reply can show.
    bool   countReadFailed     = false;
};

// The decision itself, isolated from every game call so it can be pinned by
// tests/test_toggle_skill_behavior.py without a compiled game API stand-in
// doing anything but counting. Order matters and is deliberate:
//   1. the object itself must resolve, or nothing else is known - Unreadable;
//   2. a threw instance_number call is a failed read, not a measured zero -
//      Unreadable, checked before the cheap-Off branch below can shadow it;
//   3. zero instances is a real, cheap Off - it must not require reading any
//      instance's own fields at all (read/no_aoe_is_off);
//   4. with `requireMarker` false (the plain ownership read): any own
//      instance lights it; a foreign AOE alone does not; instances present
//      but all unattributed answer Unreadable rather than guessing either
//      way;
//   5. with `requireMarker` true (the shipped indicator, when session 4
//      proves the Purgatory marker): any own instance whose own purgatory
//      reads numeric > 0 lights it. Own instances that are all unmarked (a
//      readable purgatory that is not positive) answer Off. An own instance
//      whose own marker could not be read, with none marked, answers
//      Unreadable rather than guessing - only after that does the decision
//      fall through to the same foreign/unattributed handling as step 4.
class ToggleIndicatorModel {
public:
    static ToggleIndicatorState Decide(const ToggleIndicatorReadDetail& d, bool requireMarker = false)
    {
        if (!d.objectResolved) return ToggleIndicatorState::Unreadable;
        if (d.countReadFailed) return ToggleIndicatorState::Unreadable;
        if (d.n <= 0) return ToggleIndicatorState::Off;
        if (requireMarker) {
            if (d.markedMine > 0) return ToggleIndicatorState::On;
            if (d.markUnreadableMine > 0) return ToggleIndicatorState::Unreadable;
            if (d.unmarkedMine > 0) return ToggleIndicatorState::Off;
            if (d.others > 0) return ToggleIndicatorState::Off;
            return ToggleIndicatorState::Unreadable;   // no own instance at all, and every AOE present is unattributed
        }
        if (d.mine > 0) return ToggleIndicatorState::On;
        if (d.others > 0) return ToggleIndicatorState::Off;
        return ToggleIndicatorState::Unreadable;   // every AOE present is unattributed
    }
};

inline const char* ToggleIndicatorStateName(ToggleIndicatorState s)
{
    switch (s) {
        case ToggleIndicatorState::On:  return "on";
        case ToggleIndicatorState::Off: return "off";
        default: return "unreadable";
    }
}

} // namespace ForgePact
