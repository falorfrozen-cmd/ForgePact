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

// ---- the countdown's own table (session 8, D-S1) ---------------------------
// The countdown reads THIS table and nothing else - not kToggleSkillRows,
// whose rows are chosen for a different question (does the toggle's instance
// exist), and no timer spanning a plain cast was observed for two of them. A
// row is here only because the duration sweep measured it
// (docs/toggle-skills-research.md, "Duration sweep (session 8)" -> "Results",
// status `ship`): its object appeared for exactly one skill, carried a
// positive `destroyTimer` that repeated across casts, and that timer spanned
// the object's whole life, so it is the cast's duration and not, say, a
// projectile's own lifetime. Companion skills (turrets, totems and the like)
// are left out even when measured: they can have several instances at once
// and would need one countdown each (owner, 2026-09-21).
//
// There is no talent id column, for the same reason kToggleSkillRows has
// none: ids move with every game build, so each row's id is resolved at
// runtime from `global.talentStructMap` by `abilityId`, in the same walk as
// the toggle table's, and an unresolved row is skipped and counted.
struct SkillTimerRow {
    const char* abilityId;                    // the talent struct's own `abilityId` string
    HeroSiege::Objects::GameObject object;    // the cast's own object, whose `destroyTimer` is read
    const char* ownershipField;               // nullptr: measured `own=unreadable`, every instance own (D-N3)
    double measuredFirst;                     // the sweep's recorded first reading - documentation, never a total
    const char* displayName;                  // what the player text calls the skill
};

// measuredFirst is what the Results table recorded, kept beside the row so a
// contract test can tie each row to its measurement. The draw never divides by
// it: route B latches each cast's own first reading (Blade Barrier's live
// 1296 is not its talent's predicted 864, and a stat change moves either).
inline constexpr SkillTimerRow kSkillTimerRows[] = {
    { "healingZone", HeroSiege::Objects::GameObject::White_Mage_Healing_Zone_obj,
      nullptr, 1152.0, "Healing Zone (White Mage)" },
    { "bladeBarrier", HeroSiege::Objects::GameObject::Samurai_Blade_Barrier_obj,
      "isMyClient", 1296.0, "Blade Barrier (Samurai)" },
    { "soulSpurn", HeroSiege::Objects::GameObject::White_Mage_Soul_Spurn_AOE_obj,
      "isMyClient", 144.0, "Soul Spurn (White Mage)" },
    { "maelstromOfFrost", HeroSiege::Objects::GameObject::Prophet_Maelstrom_obj,
      "isMyClient", 4320.0, "Maelstrom of Frost (Prophet)" },
    { "progeniesOfTheGreatCataclysm", HeroSiege::Objects::GameObject::Bard_Progenies_Amplifier_obj,
      nullptr, 2880.0, "Progenies of the Great Cataclysm (Bard)" },
    { "pickupRaid", HeroSiege::Objects::GameObject::Redneck_Pickup_Truck_obj,
      "isMyClient", 576.0, "Pickup Raid (Redneck)" },
    { "dissipatingTornado", HeroSiege::Objects::GameObject::Dissipating_Tornado_obj,
      nullptr, 432.0, "Dissipating Tornado (Nomad)" },
};
inline constexpr int kSkillTimerRowCount =
    (int)(sizeof(kSkillTimerRows) / sizeof(kSkillTimerRows[0]));

// ---- buff-carried skills (issue #55, session 12) --------------------------
// A row here has no cast object at all: its duration lives on the player's
// own buff list, `global.playerBuff[1][0][<buffId>]`, read by
// ModuleMain.cpp's SkillTimerBuffReadRow (outside every research block) -
// the same chain HhBuffAlive already walks for Headhunter, and the chain
// session 12's `tgprobe buffwatch` instrument measured live
// (docs/toggle-skills-research.md, "### Buff-carried countdown (session
// 12)"). `buffId` is game data measured on this build, not an address: the
// per-draw `buffType == buffId` identity check (AGENTS.md "Identify a thing
// by what it is") is what keeps a renumbered build from drawing a
// stranger's buff. Every runtime name the reader needs lives here, the same
// rule kSkillTimerRows above and kToggleSkillRows follow.
inline constexpr const char* kSkillTimerBuffArrayGlobal = "playerBuff";
inline constexpr int kSkillTimerBuffPlayerIndex = 1;
inline constexpr int kSkillTimerBuffSubIndex = 0;
inline constexpr const char* kSkillTimerBuffIdentityField = "buffType";

struct SkillTimerBuffRow {
    const char* abilityId;     // the talent struct's own `abilityId` string
    int buffId;                 // the measured slot index into playerBuff[1][0]
    double measuredFirst;       // session 12's recorded first reading - documentation only
    const char* displayName;    // "Name (Class)" - the no-name text test derives forbidden names from this
};

// Session 12's four `ship` rows (docs/toggle-skills-research.md,
// "### Buff-carried countdown (session 12)" -> "#### Results"), in that
// order. `counter` is the only row with a toggle twin (kToggleSkillRows'
// `PlayerBuff` row below): while Counter's Give No Quarter sub-talent reads
// Allocated, that twin's own read decides the toggle is ON and this row
// draws nothing instead (ctx "The Give No Quarter form split" of the
// workorder that shipped this).
inline constexpr SkillTimerBuffRow kSkillTimerBuffRows[] = {
    { "counter", 104, 1036.800000, "Counter (Shield Lancer)" },
    { "lastStand", 107, 3600.000000, "Last Stand (Shield Lancer)" },
    { "defensiveShout", 9, 14400.000000, "Defensive Shout (Viking)" },
    { "berserk", 1, 720.000000, "Berserk (Viking)" },
};
inline constexpr int kSkillTimerBuffRowCount =
    (int)(sizeof(kSkillTimerBuffRows) / sizeof(kSkillTimerBuffRows[0]));

// ---- rule-based coverage of untested skills (issue #55 follow-up, D-S4) ---
// Owner, 2026-09-21, verbatim: "lets ship untested following a rule - if it
// has a cooldown and a duration and if its not a companion type skill, it
// should support". Interpreted (told to the owner, who asked for it):
// eligible when the slot's talent reads `abilityDuration > 0` AND
// `abilityCooldown` above the no-cooldown floor below (Meteor Storm reads
// 0.25 and has none, per the owner), the object is not a companion (excluded
// structurally by the generator, never reaches this file), and a cast object
// resolves by NAME CONVENTION from the abilityId (SkillTimerNames.hpp,
// generated - never hand-typed, AGENTS.md "Never Call an Address You
// Resolved by Hand"). A measured deny-list always wins. The seven rows above
// stay explicit and win over the rule (D-R1): Soul Spurn reads
// `abilityDuration=0`, so the rule would not select it anyway.
//
// This tier ships UNTESTED, by the owner's decision - nothing below is a
// measurement claim.
inline constexpr double kSkillTimerCooldownFloor = 0.25;
// The runtime-built rule map's own bound (T2/T3): at most this many entries
// at once, so a draw's per-slot lookup and the walk's own storage are both
// bounded regardless of how many talents the sweep matches. Overflow counts
// `ruleCapped` rather than growing without limit.
inline constexpr int kSkillTimerRuleCap = 64;

// One class-prefixed hs-game-sdk object's generated-table entry: key (the
// SkillTimerNames.hpp naming convention, see that file's own header comment)
// and the object it names. No game API here - this struct, and the table
// built from it, are read by ModuleMain.cpp's rule walk and rule draw, never
// spelled as a literal `GameObject::` enumerator outside the seven explicit
// rows above and the generated header itself
// (test_no_hand_typed_object_name_reaches_the_rule_path).
struct SkillTimerNameEntry {
    const char* key;
    HeroSiege::Objects::GameObject object;
};

// The measured deny-list (docs/toggle-skills-research.md, "Duration sweep
// (session 8)" -> "Results", plus the Meteor Storm/Bushido toggle-upgrade
// session (session 9) and the Shaman Meteor Storm toggle-upgrade driver
// note): a talent the rule would otherwise select (or could select on a
// future patch) that a live measurement, or a structural argument, ruled
// out. Always wins over the rule (D-S4) - checked before the generated-table
// lookup, so a denied talent counts `ruleDenied` even when it has no object
// by name convention at all (the four toggle entries below).
struct SkillTimerDenyEntry {
    const char* abilityId;   // the talent struct's own `abilityId`, exact spelling
    const char* reason;
};
inline constexpr SkillTimerDenyEntry kSkillTimerRuleDeny[] = {
    { "submergedKnives",
      "counter-example: dur 2.5, cd 1.5, but its object's own timer is a per-projectile "
      "lifetime (first=32.4 against 140 draws), not the cast's duration" },
    { "crematus",
      "the damage object's own timer is a per-projectile lifetime (first=79.2 against 290 "
      "draws, maxInst=8); the controller's own timer is unreadable" },
    { "blizzard",
      "no spanning timer: the controller's own timer is unreadable and the shards read a "
      "constant -1" },
    { "arrowRain",
      "no spanning timer: both the ability and the individual arrows read a constant -1" },
    { "meteorStorm",
      "no spanning timer: the controller is unreadable and the meteors read a constant -1 "
      "(owner: this skill has no cooldown, despite reading 0.25)" },
    { "defensiveShout",
      "buff-carried: covered by kSkillTimerBuffRows instead (session 12), never by this rule" },
    { "berserk",
      "buff-carried: covered by kSkillTimerBuffRows instead (session 12), never by this rule" },
    { "arrowTurret",
      "companion (also excluded structurally; listed for the record)" },
    { "fireTotem",
      "companion (also excluded structurally; listed for the record)" },
    { "bushido",
      "base-form toggle; the countdown never draws a toggle" },
    { "holyForm",
      "toggle; the countdown never draws a toggle" },
    { "unholyForm",
      "toggle; the countdown never draws a toggle" },
    { "melonForm",
      "toggle; the countdown never draws a toggle" },
};
inline constexpr int kSkillTimerRuleDenyCount =
    (int)(sizeof(kSkillTimerRuleDeny) / sizeof(kSkillTimerRuleDeny[0]));

inline bool SkillTimerRuleDenied(const std::string& abilityId)
{
    for (int i = 0; i < kSkillTimerRuleDenyCount; ++i) {
        if (abilityId == kSkillTimerRuleDeny[i].abilityId) return true;
    }
    return false;
}

// D-R1: the seven object rows above stay explicit and win over the rule - a
// talent id matching one of them is never entered into the rule map at all.
// Session 12 adds the buff-carried rows to the same exclusion: a buff row
// has no object at all, so the rule (which only ever resolves an object by
// name convention) could never select it anyway, but excluding it here
// means it is never even attempted, and defensiveShout/berserk's presence on
// kSkillTimerRuleDeny stays a documented belt-and-braces, not the only
// thing keeping them out.
inline bool SkillTimerRuleIsExplicitRow(const std::string& abilityId)
{
    for (int i = 0; i < kSkillTimerRowCount; ++i) {
        if (abilityId == kSkillTimerRows[i].abilityId) return true;
    }
    for (int i = 0; i < kSkillTimerBuffRowCount; ++i) {
        if (abilityId == kSkillTimerBuffRows[i].abilityId) return true;
    }
    return false;
}

// One entry in the runtime-built rule map (T2): a talent id matched to a
// generated name-table index, with its own latch (the same one-latch-per-row
// shape kSkillTimerRows' explicit rows use, SkillTimerRowState) and a cached
// resolved object index (game-side; negative = not yet resolved). The rule
// walk rebuilds this table wholesale once per room (T2), so every entry's
// latch resets along with it - the same "an instance disappearing drops the
// latch" reasoning SkillTimerModel::Decide already applies within one room.
struct SkillTimerRuleEntry {
    int talentId = -1;
    int nameIndex = -1;          // index into ForgePact::kSkillTimerNames
    std::string abilityId;       // as read from the talent struct - for the stat line
    SkillTimerRowState state;    // this entry's own latch
    double objIdx = -1.0;        // resolved lazily, cached only once >= 0
};

class SkillTimerRuleModel {
public:
    // The pure decision (D-S4's rule, interpreted): both fields read as
    // numbers, a positive duration, a cooldown above the no-cooldown floor,
    // not denied, not one of the seven explicit rows. No game call and no
    // object name here - resolving (or not) an object by name is the
    // caller's job, once eligibility is decided. Pinned truth-table points
    // (context, "Eligibility, read once per room"): cooldown == floor is
    // ineligible, floor + a hair above is eligible; duration == 0 is
    // ineligible regardless of cooldown.
    static bool Eligible(double duration, double cooldown, bool readable, bool denied, bool isExplicitRow)
    {
        if (isExplicitRow) return false;
        if (denied) return false;
        if (!readable) return false;
        if (!(duration > 0.0)) return false;
        if (!(cooldown > kSkillTimerCooldownFloor)) return false;
        return true;
    }
};

} // namespace ForgePact
