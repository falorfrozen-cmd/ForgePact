#pragma once

#include "Common.hpp"

namespace ForgePact {

// Toggle-skill active indicator (issue #11, Track B): marks the skill-bar slot
// of a toggle skill while its toggled-on instance is live - Soul Spurn's
// Purgatory drain in T1, and every row of kToggleSkillRows below since phase
// S. The decision is per row and identical in shape; only the row's
// discriminator differs. This header holds
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
// - a `VALUE_BOOL` gives its truth, a numeric kind counts true only when its
// value is numeric > 0, and anything else (undefined, a string, or a throw) is unattributed and never
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

// ---- the shipped toggle-skill table (issue #11, phase S) -------------------
// Session 6 measured five toggle skills that keep a persistent instance alive
// while the toggle is on, and, per row, how that instance tells a toggle from
// a plain cast (docs/toggle-skills-research.md, "## Results" -> "### Toggle
// skill table" and "## Decision" -> "### After session 6"). Every runtime
// name the indicator and the guard need lives HERE and nowhere else in the
// plugin: the object enumerator, the ownership field, the discriminator field
// and the sub-talent slot.
//
// Three rows ship a CONTROLLER object rather than the damage object the static
// search predicted, because session 6 rejected the predicted one in each case
// (Lunar Orbit's passes through the held timer value on a plain cast, Crematus'
// projectile outlives the toggle, Submerged Knives' flickers). Those three
// controllers have no readable ownership field either, so they carry
// `ownershipField = nullptr` and every instance counts as own - ForgePact is
// offline-only, so that is the documented D-N3 behaviour, not a co-op risk.
//
// Shield Lancer's Counter and Butcher's Blender are deliberately absent:
// session 6 observed no persistent ON instance for Counter over the one
// ON/OFF cycle it measured (its toggle state read as a player buff, not a
// per-skill instance) and never ran Blender's ON/OFF steps at all. Both are
// recorded results, not omissions - and the Counter one is "not observed in
// that pass", not "does not exist".
//
// There is no talent id column. Ids move with every game build, so each row's
// id is resolved at runtime from `global.talentStructMap` by `abilityId`
// (D-P1; repo AGENTS.md, "Never Call an Address You Resolved by Hand" ->
// resolve by name), and a row whose id is not resolved yet is skipped by both
// mods and counted.
enum class ToggleOnMark {
    Marker,      // an own instance whose `markField` reads numeric > 0 is ON
    TimerHeld,   // an own instance whose `markField` reads EXACTLY `heldValue` is ON
    None,        // any own instance is ON - the plain form creates no instance at all
};

struct ToggleSkillRow {
    const char* abilityId;                    // the talent struct's own `abilityId` string
    int subTalentSlot;                        // the toggle sub-talent's `s<NN>` key
    HeroSiege::Objects::GameObject onObject;  // the instance that exists while the toggle is on
    const char* ownershipField;               // nullptr: no readable ownership field (D-N3)
    ToggleOnMark mark;
    const char* markField;                    // Marker/TimerHeld only; nullptr for None
    double heldValue;                         // TimerHeld only: the measured held value
};

inline constexpr ToggleSkillRow kToggleSkillRows[] = {
    { "soulSpurn", 12, HeroSiege::Objects::GameObject::White_Mage_Soul_Spurn_AOE_obj,
      "isMyClient", ToggleOnMark::Marker, "purgatory", 0.0 },
    { "lunarOrbit", 11, HeroSiege::Objects::GameObject::Exo_Lunar_Orbit_Crescent_Moon_obj,
      nullptr, ToggleOnMark::None, nullptr, 0.0 },
    { "crematus", 13, HeroSiege::Objects::GameObject::Plague_Doctor_Crematus_Controller_obj,
      nullptr, ToggleOnMark::Marker, "skillContamination", 0.0 },
    { "submergedKnives", 13, HeroSiege::Objects::GameObject::Butcher_Submerged_Knives_Knifehoarder_obj,
      nullptr, ToggleOnMark::None, nullptr, 0.0 },
    { "maelstromOfFrost", 11, HeroSiege::Objects::GameObject::Prophet_Maelstrom_obj,
      "isMyClient", ToggleOnMark::TimerHeld, "destroyTimer", -1.0 },
};
inline constexpr int kToggleSkillRowCount =
    (int)(sizeof(kToggleSkillRows) / sizeof(kToggleSkillRows[0]));

// A `None` row lights on any own instance, so the shared decision runs with
// requireMarker=false for it and true for every other row. Either way
// ToggleIndicatorModel::Decide above is untouched - each row's discriminator
// only decides which of markedMine/unmarkedMine/markUnreadableMine an own
// instance is counted into (D-P5).
inline constexpr bool ToggleRowRequiresMark(const ToggleSkillRow& row)
{
    return row.mark != ToggleOnMark::None;
}

// The toggle sub-talent's index inside `global.subTalentMap` (session 6:
// every one of the five slots was measured at index 1, and an unallocated
// slot reads 0.000000 with the key present, never absent).
//
// That is one character on one build, so it is where the read STARTS, not
// what it assumes: the index that answers is the one whose `t<talentId>`
// struct is actually there, which is the shape the research probe's own
// `tgprobe tgl sub` walks. Were index 1 a character or player slot on
// another save, a fixed index would leave the guard silently inert with
// nothing but a `subUnreadable=` counter to show for it (phase S review
// follow-up); the index that did answer is reported in `toggleguard stat`.
inline constexpr int kToggleSubTalentMapIndex = 1;
// How far past the measured index the fallback scan is willing to look. The
// map is one entry per character slot, so this is a bound on a mistake, not
// a real limit - `tgprobe tgl sub` uses the same cap on the same array.
inline constexpr int kToggleSubTalentScanCap = 16;

// Re-cast guard (issue #11, Track A; `toggleguard`). Session 1 measured the
// double-cast proc re-casting a toggle skill as a `TalentUseClass` call whose
// `self` is `Universal_Double_Cast_obj`, with no `TalentUse` call in front of
// it - so a proc can flip Soul Spurn straight back after the player's own
// press. The guard refuses exactly that call: guard on, caller is the
// double-cast object, talent is a guarded one. It never reads the toggle's
// state (docs/toggle-skills-research.md, "## Decision" -> "### Track A design
// (D-N1)": the state cannot tell "just turned off" from "never on").
// Everything else - the player's own cast, the chained follow-up casts, a proc
// of any other talent - passes.
//
// Phase S widens "a guarded one" from Soul Spurn alone to every row of
// kToggleSkillRows whose talent id has been resolved at runtime, and gates the
// refusal itself on that row's toggle sub-talent being allocated, read at the
// call in HookTalentUseClass (D-P3). This model still decides only the part
// that needs no game call; the sub-talent read, being a game read that can
// fail, lives with the hook and fails open there.
//
// Game-independent like ToggleIndicatorModel above: who the caller is and
// which talent it names are worked out in ModuleMain.cpp's HookTalentUseClass,
// by name; this only decides, and its own text is unchanged by phase S. The
// guarded talent is a constructor argument, so the header needs no talent id
// of its own - and since phase S neither does the plugin, because every row's
// id is resolved at runtime from its `abilityId`. Membership in the shipped
// table is settled before this model is built, so the id it is handed is
// always the matched row's own resolved id.
enum class ToggleGuardDecision { Pass, Refuse };

class ToggleGuardModel {
public:
    explicit constexpr ToggleGuardModel(int guardedTalentId) : m_GuardedTalentId(guardedTalentId) {}

    constexpr bool Guards(int talentId) const { return talentId == m_GuardedTalentId; }

    constexpr ToggleGuardDecision Decide(bool enabled, bool callerIsDoubleCast, int talentId) const
    {
        if (!enabled) return ToggleGuardDecision::Pass;
        if (!callerIsDoubleCast) return ToggleGuardDecision::Pass;
        if (!Guards(talentId)) return ToggleGuardDecision::Pass;
        return ToggleGuardDecision::Refuse;
    }

private:
    int m_GuardedTalentId;
};

// `toggleguard 1` only ARMS the guard, the RelicFilterMod shape: installing
// a script hook while character selection is still running stalls the runner
// (guide Known Limitations item 8), so FrameCallback installs the
// `TalentUseClass` hook later, once the setup gate has passed and a player
// exists, then calls ClearPending(). `toggleguard 0` clears the enabled flag
// only; an installed hook stays, and its first statement passes every call
// straight through while the flag is off.
class ToggleGuardMod {
public:
    static ToggleGuardMod& Instance() {
        static ToggleGuardMod s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(); }
    bool IsPending() const { return m_Pending.load(); }
    void ClearPending() { m_Pending.store(false); }

    // `alreadyHooked` is whether the TalentUseClass trampoline exists yet;
    // this class does not own it (ModuleMain.cpp's g_OrigTalentUseClass).
    void SetEnabled(bool enabled, bool alreadyHooked) {
        m_Enabled.store(enabled);
        if (enabled && !alreadyHooked) m_Pending.store(true);
        if (!enabled) m_Pending.store(false);
    }

private:
    ToggleGuardMod() = default;
    std::atomic<bool> m_Enabled{ false };
    std::atomic<bool> m_Pending{ false };
};

} // namespace ForgePact
