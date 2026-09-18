#pragma once

#include "Common.hpp"

namespace ForgePact {

// Toggle-skill active indicator (issue #11, Track B): outlines Soul Spurn's
// skill-bar slot while the Purgatory-toggled AOE is live. This header holds
// only the game-independent decision - no RValue, no CallBuiltin - so every
// branch is unit-testable without a game process. The actual read (resolving
// the AOE object, counting instances, resolving the local player) lives in
// ModuleMain.cpp, outside any research block, because it leans on
// HhResolveLocalPlayer and the SDK object table which this header does not
// pull in (docs/toggle-skills-research.md, "The read, and exactly what has
// been proven").
enum class ToggleIndicatorState { On, Off, Unreadable };

// Everything one enumeration pass over the AOE object measured. `objectResolved
// = false` means the object name itself did not resolve (asset_get_index < 0)
// - a different failure than "resolved but zero instances", which is Off, not
// Unreadable. `mine`/`others`/`unattributed` classify every AOE instance the
// scan actually visited (bounded by the scan cap, see `capped`) by comparing
// its own `playerNumber` against the local player's - an instance whose
// `playerNumber` could not be read is unattributed and never lights the
// indicator ("Co-op / ownership: the answer").
struct ToggleIndicatorReadDetail {
    bool   objectResolved      = false;
    long   n                   = 0;      // instance_number of the AOE object
    long   mine                = 0;
    long   others               = 0;
    long   unattributed        = 0;
    bool   capped              = false;  // the scan hit its instance budget
    bool   localResolved       = false;  // HhResolveLocalPlayer succeeded (or an override was supplied)
    bool   localNumberReadable = false;
    double localNumber         = 0.0;
};

// The decision itself, isolated from every game call so it can be pinned by
// tests/test_toggle_skill_behavior.py without a compiled game API stand-in
// doing anything but counting. Order matters and is deliberate:
//   1. the object itself must resolve, or nothing else is known - Unreadable;
//   2. zero instances is a real, cheap Off - it must not require resolving
//      the local player at all (read/no_aoe_is_off_without_resolving_player);
//   3. with instances present, the local player's own number must be
//      readable, or the comparison in step 4 means nothing - Unreadable;
//   4. an AOE that is ours lights the indicator; a foreign AOE alone does
//      not; instances present but all unattributed answer Unreadable rather
//      than guessing either way.
class ToggleIndicatorModel {
public:
    static ToggleIndicatorState Decide(const ToggleIndicatorReadDetail& d)
    {
        if (!d.objectResolved) return ToggleIndicatorState::Unreadable;
        if (d.n <= 0) return ToggleIndicatorState::Off;
        if (!d.localResolved || !d.localNumberReadable) return ToggleIndicatorState::Unreadable;
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
