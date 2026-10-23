#pragma once

#include <cmath>
#include <cstdint>

namespace ForgePact {

// ---- Mark 1: the automation's mana cost (ForgePact #166, Amendment 2) -----------
//
// On the proc route the automation fires its skills the way the game fires an
// item proc, and a proc costs no mana. The owner asked for a price on that
// (2026-10-22): "constant mana drain per each automated skill or reduce player
// mana by 20% per each automated skill ... i'd lean into first option ... it
// cant be impossible to maintain while not being trivial to maintain at 4
// automated skills". The price applies to proc columns only: a toggle keeps the
// press and pays its own mana, and the press route's casts pay the game's own.
//
// Mana itself is never written. The game keeps it in its protected store and
// checks it against its own history (static reading, the hub's RDM § 7.7 and
// § 7.8), so the cost goes through the stat dispatcher (ReturnSpecificStat)
// alone, as one more answer the set bonus's consumer gives the local player:
//   - Drain (shipped): id 42, the mana replenish, is answered with its original
//     minus columns x kMark1DrainPctPerSecond % of maximum mana divided by
//     kMark1ReplenishUnit (the mana a second one unit of the answer is), never
//     below minus the maximum, so mana falls through the game's own update by
//     about 1% of the maximum a second per column.
//   - MaxMana (the documented alternative, still built): id 37, maximum mana,
//     is answered with its original less kMark1MaxManaCutPct % per column,
//     never below 0.
//   - Off: nothing is answered.
// The arithmetic is the hub's hs_game_sdk/mark1_automation_model.py
// (replenish_answer, cut_max_mana; spec docs/models/mark1-automation-spec.md),
// and tests/test_mark1_cost_behavior.py checks this header against every
// `drain` and `maxmana` case of hs-game-sdk/curated/mark1_automation_cases.json.
//
// This header decides; it never touches the game. The consumer in
// ModuleMain.cpp asks Wants for each of the local player's dispatcher calls
// and hands Answer the call's id and answer, the proc route's column count and
// the id-37 total it read by name. Numbers in and numbers out, so
// tests/mark1_cost_harness.cpp compiles it whole.

enum class Mark1ManaCost { Drain, MaxMana, Off };

// Which cost ships: the drain, the owner's choice (2026-10-22, "id really want
// to go with mana drain option instead of flat 20% mana decrease per automated
// skill"; decision key `cost: drain`). Stats live 2 backs it: its
// `drain-applies` saw the lowered id-42 answer reach the game's regen (the 3 s
// gain halved with half the answer), and its `proc-mana` that procs are free,
// so a cost is needed. The maximum-mana cut, which Stats live 1's
// `maxmana-applies` measured, stays built behind `mark1probe autocost maxmana`.
inline constexpr Mark1ManaCost kMark1ManaCost = Mark1ManaCost::Drain;
// The drain per automated (proc) skill, in % of maximum mana a second: half the
// assumed 2% baseline replenish, so one or two skills sustain themselves and
// four empty a full pool in about 50 s at baseline (the hub model's
// DRAIN_PCT_PER_SECOND). Adopted by the owner (2026-10-22); Stats live 2's
// numbers did not argue otherwise (Sorak's replenish 257 with Mana Orb unbound:
// two columns take 47 of it, four 94).
inline constexpr double kMark1DrainPctPerSecond = 1.0;
// The alternative's cut per automated skill, in % of maximum mana: the owner's
// own figure (2026-10-22, "20% per each automated skill"); 20% left at four.
inline constexpr double kMark1MaxManaCutPct = 20.0;
// The mana a second one drained unit of id 42's answer is: 0.745, from Stats
// live 3 in process, S7 minus S8 (the regen at the answer's own 140 was 122.417
// mana a second, with 47.04 units drained it was 87.37: 35.05 over 47.04).
// S7 alone read 0.874 a unit of the answer, Live 2's halving about 0.877 and
// S9's below-zero window 0.887; the readings disagree. The drain divides by
// it, so each column takes about kMark1DrainPctPerSecond % of maximum mana a
// second (the owner's choices of 2026-10-23, "Scale to 1%/s", then 0.745);
// that rate is not measured in a game session yet. The 1.0 build Live 3
// measured took about 0.745% a second per column, read directly (S7 minus
// S8); the 0.87% recorded for it elsewhere is inferred from S7's 0.874. The
// hub model's REPLENISH_UNIT is the same.
inline constexpr double kMark1ReplenishUnit = 0.745;

// The dispatcher ids the cost answers (rss_cases.json, static reading).
inline constexpr double kMark1MaxManaStatId = 37.0;    // StatMaxMana
inline constexpr double kMark1ReplenishStatId = 42.0;  // StatManaReplenish
// The range `mark1probe autodrain <percent>` accepts.
inline constexpr double kMark1DrainPctMin = 0.0;
inline constexpr double kMark1DrainPctMax = 10.0;
// More columns than the bar has never count beyond it.
inline constexpr int kMark1CostMaxColumns = 4;

// Whether `unit` can scale the drain: a positive finite number.
inline bool Mark1ReplenishUnitUsable(double unit) { return std::isfinite(unit) && unit > 0; }

// How much the drain takes off id 42's answer, in units of that answer:
// `columns` automated skills, each `percent` % of `maxMana` a second, divided
// by `unit` (the mana a second one unit is). The product is taken before the
// division, so whole-percent cases at unit 1.0 are exact. No column, or a unit
// that is not a positive finite number, drains nothing.
inline double Mark1DrainPerSecond(int columns, double percent, double maxMana,
                                  double unit = kMark1ReplenishUnit)
{
    if (columns <= 0 || !Mark1ReplenishUnitUsable(unit)) return 0.0;
    return static_cast<double>(columns) * percent * maxMana / (100.0 * unit);
}

// The drain's answer for id 42: `original` less Mark1DrainPerSecond, never
// below -maxMana. No column, a maximum that is not a positive finite number,
// or a unit that is not one, leaves the original alone.
inline double Mark1DrainAnswer(double original, int columns, double maxMana,
                               double percent = kMark1DrainPctPerSecond,
                               double unit = kMark1ReplenishUnit)
{
    if (columns <= 0 || !std::isfinite(maxMana) || maxMana <= 0 || !Mark1ReplenishUnitUsable(unit))
        return original;
    const double lowered = original - Mark1DrainPerSecond(columns, percent, maxMana, unit);
    return lowered < -maxMana ? -maxMana : lowered;
}

// The alternative's answer for id 37: `original` less `cutPct` % per column,
// never below 0. No column leaves it alone.
inline double Mark1MaxManaAnswer(double original, int columns, double cutPct = kMark1MaxManaCutPct)
{
    if (columns <= 0) return original;
    const double cut = original * (100.0 - cutPct * static_cast<double>(columns)) / 100.0;
    return cut < 0 ? 0.0 : cut;
}

// The answer the consumer gives for one dispatcher call, from (id, original,
// proc columns, max mana, cost, percent, unit): id 42 under Drain, id 37 under
// MaxMana, every other id, every other cost and a zero column count left as
// the game gave it.
inline double Mark1CostAnswer(double statId, double original, int columns, double maxMana,
                              Mark1ManaCost cost, double percent, double unit = kMark1ReplenishUnit)
{
    if (columns <= 0) return original;
    if (cost == Mark1ManaCost::Drain && statId == kMark1ReplenishStatId)
        return Mark1DrainAnswer(original, columns, maxMana, percent, unit);
    if (cost == Mark1ManaCost::MaxMana && statId == kMark1MaxManaStatId)
        return Mark1MaxManaAnswer(original, columns, kMark1MaxManaCutPct);
    return original;
}

struct Mark1CostCounters {
    uint64_t drained = 0;    // answers the cost changed, id 42 or id 37 (`drained=`)
    uint64_t notNumber = 0;  // wanted calls whose answer was not a finite number: left alone
};

class Mark1Cost {
public:
    // Is this call one the cost may answer: the local player's own, asking
    // for id 37 or id 42? Asked before anything else, so no other call costs
    // anything more.
    static bool Wants(bool localPlayer, bool haveStatId, double statId)
    {
        return localPlayer && haveStatId
            && (statId == kMark1MaxManaStatId || statId == kMark1ReplenishStatId);
    }

    // One wanted call's answer, as the caller will read it, with the cost,
    // percent and unit in force. `procColumns` is the number of columns the proc route
    // fires (0 on the press route or with the bonus inactive), `maxMana` the
    // id-37 total last read by name. A finite answer the cost changes is
    // replaced and counted `drained`; with a cost in force and a proc column,
    // a non-number is left alone, never guessed at, and counted `notNumber`.
    // Each call is answered from its own original, so repeated calls never
    // compound. True when it changed.
    bool Answer(double statId, bool answerIsNumber, double& answer, int procColumns, double maxMana)
    {
        const int columns = procColumns > kMark1CostMaxColumns ? kMark1CostMaxColumns : procColumns;
        if (columns <= 0 || cost_ == Mark1ManaCost::Off) return false;
        if (!answerIsNumber || !std::isfinite(answer)) { ++counters_.notNumber; return false; }
        const double changed = Mark1CostAnswer(statId, answer, columns, maxMana, cost_, percent_, unit_);
        if (changed == answer) return false;
        answer = changed;
        ++counters_.drained;
        return true;
    }

    // ---- research overrides (`mark1probe autocost`, `autodrain`, `autounit`) ----
    // They change the values in force for this session, from the next answer
    // on; ResetOverrides puts the shipped constants back.
    void SetCost(Mark1ManaCost cost) { cost_ = cost; }
    // False, and nothing changed, outside kMark1DrainPctMin..kMark1DrainPctMax.
    bool SetDrainPct(double percent)
    {
        if (!std::isfinite(percent) || percent < kMark1DrainPctMin || percent > kMark1DrainPctMax) return false;
        percent_ = percent;
        return true;
    }
    // The unit the drain divides by (Amendment 11, so a live session can tune
    // the drain without a rebuild). False, and nothing changed, for what
    // Mark1ReplenishUnitUsable refuses: 0, a negative, NaN, infinity.
    bool SetUnit(double unit)
    {
        if (!Mark1ReplenishUnitUsable(unit)) return false;
        unit_ = unit;
        return true;
    }
    // `mark1probe autounit off`: back to kMark1ReplenishUnit.
    void ResetUnit() { unit_ = kMark1ReplenishUnit; }
    void ResetOverrides() { cost_ = kMark1ManaCost; percent_ = kMark1DrainPctPerSecond; unit_ = kMark1ReplenishUnit; }

    Mark1ManaCost Cost() const { return cost_; }
    double DrainPct() const { return percent_; }
    double Unit() const { return unit_; }
    const Mark1CostCounters& Stats() const { return counters_; }

private:
    Mark1ManaCost cost_ = kMark1ManaCost;
    double percent_ = kMark1DrainPctPerSecond;
    double unit_ = kMark1ReplenishUnit;
    Mark1CostCounters counters_;
};

// The cost's word, as `mark1probe autocost` and the stat line print it.
inline const char* Mark1ManaCostName(Mark1ManaCost cost)
{
    switch (cost) {
    case Mark1ManaCost::Drain: return "drain";
    case Mark1ManaCost::MaxMana: return "maxmana";
    case Mark1ManaCost::Off: return "off";
    }
    return "off";
}

// Parses `drain`, `maxmana` or `off`; false, `out` untouched, for anything else.
inline bool Mark1ParseManaCost(const char* word, Mark1ManaCost& out)
{
    if (!word) return false;
    auto same = [word](const char* w) {
        const char* a = word;
        while (*a && *w && *a == *w) { ++a; ++w; }
        return *a == 0 && *w == 0;
    };
    if (same("drain")) { out = Mark1ManaCost::Drain; return true; }
    if (same("maxmana")) { out = Mark1ManaCost::MaxMana; return true; }
    if (same("off")) { out = Mark1ManaCost::Off; return true; }
    return false;
}

} // namespace ForgePact
