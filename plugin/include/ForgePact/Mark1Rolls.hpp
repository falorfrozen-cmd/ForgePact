#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

namespace ForgePact::Mark1Rolls {

// ---- Mark 1: the pieces' rolls and the set bonus (ForgePact #166) ---------------
//
// The four Mark 1 pieces carry the owner's "Bulwark SS" stats (the owner,
// 2026-10-22, replacing the 2026-10-13 decision "no item stats"): each piece
// rolls its own stats inside fixed ranges and carries one or two fixed ones,
// and the set bonus adds lines at 2, 3 and 4 pieces worn.
//
//   - Where a roll lives: the item definition's seed `a`, as the flight charms
//     keep theirs (FlightCharms.hpp). A save keeps only the compact definition
//     (the hub's docs/RUNTIME_DATA_MODELS.md § 16.1) and the Custom Forge
//     dresses a piece on every build, so the rolls must come from something
//     the save keeps. Each piece owns a band of seeds holding exactly its roll
//     count, mixed radix in table order (the first rolled stat the major
//     digit).
//   - The bands start at 1,000,000,000, one million wide per piece: above every
//     natural seed Item Truth measured (485,148 item records, the highest
//     999,981,452; ForgePact's angelic placements draw 100,000,000 to
//     999,999,999) and inside a 32-bit signed integer. Whether the game builds
//     and saves such an `a` unchanged is Live 1's `roll-spawn` and `roll-save`.
//   - The legacy seeds 777500..777503, which every piece made before the
//     rolls carries, stay pieces of the same class and decode to every stat at
//     the bottom of its range and the lower socket count.
//   - The set bonus is answered through the game's stat dispatcher
//     (ReturnSpecificStat): the call's own original plus the value of each
//     line whose threshold the worn pieces meet, while the "Mark 1 set" switch
//     is on and the bonus is installed. Each call is answered from its own
//     original, so nothing compounds.
//
// Two numberings meet here and must not be confused. An item stat key (154
// Defense, 29 Enhanced Defense, ...) is what an item's stats map holds and
// what the Custom Forge writes; a dispatcher id (92 StatDefense, 161
// StatAllResistances, ...) is ReturnSpecificStat's argument. Both are pinned
// in the hub's hs-game-sdk/curated/item_stat_keys.json, which
// tests/test_mark1_rolls_behavior.py checks this header against. Neither is
// hs_game_sdk.stats.StatId's numbering (hub #546).
//
// This header is numbers and strings only - no instance, no RValue, no
// Windows header - so tests/mark1_rolls_harness.cpp compiles it whole. It
// includes nothing of Mark 1's; Mark1Set.hpp includes it.

// ---- the item stat keys the pieces write ----------------------------------------
// The item stat keys, confirmed against hero-siege-item-editor's
// hs_stat_semantics_s10.json (schema 1, generated 2026-09-02) and against the
// labels the game drew in Item Truth's captures (measured, build
// pe-6aaa6779-0cad4fc8).
inline constexpr int kKeySockets = 20;               // the `Sockets (N)` row after the stat lines
inline constexpr int kKeyEnhancedDefense = 29;       // percent, `Enhanced Defense`
inline constexpr int kKeyVitality = 48;              // flat, `to Vitality`
inline constexpr int kKeyAllAttributes = 51;         // flat, `to All Attributes`
inline constexpr int kKeyLife = 52;                  // flat, `to Life`
inline constexpr int kKeyLifeAfterEachKill = 59;     // flat, `Life After each Kill`
inline constexpr int kKeyDefense = 154;              // flat, the header `Defense: N`
inline constexpr int kKeyFasterHitRecovery = 170;    // percent, `Faster Hit Recovery`
inline constexpr int kKeyAllResistances = 173;       // percent, `to All Resistances`
inline constexpr int kKeyAllSkills = 201;            // flat, `to All Skills`

// ---- the four pieces' rolls -----------------------------------------------------

// The same order as Mark1Set.hpp's Piece (Helmet, Body, Boots, Gloves); the
// behavior test pins the order of both tables.
enum class Piece : int { Helmet, Body, Boots, Gloves };
inline constexpr int kPieceCount = 4;
inline constexpr int kMaxRolls = 5;
inline constexpr int kMaxFixed = 1;

// One rolled stat: its item stat key and the inclusive range of its value.
struct StatRoll {
    int key;
    int lo;
    int hi;
};

// One fixed stat: its item stat key and its value.
struct FixedStat {
    int key;
    int value;
};

struct PieceRolls {
    Piece      piece;
    long long  bandLo;      // the first seed of the piece's band
    long long  rollTotal;   // the product of the rolls' range sizes, written out so the assert pins it
    long long  legacySeed;  // the seed every piece made before the rolls carries
    int        rolledStats;  // how many stats roll
    StatRoll   rolls[kMaxRolls];
    int        fixedCount;
    FixedStat  fixed[kMaxFixed];
};

// The band layout: piece i's band starts at kBandBase + i * kBandWidth.
inline constexpr long long kBandBase = 1000000000LL;
inline constexpr long long kBandWidth = 1000000LL;
// The highest natural seed Item Truth's 184 journals hold (measured, 2026-09-27
// to 2026-10-21); the bands sit above it.
inline constexpr long long kHighestNaturalSeedMeasured = 999981452LL;
// The largest value a 32-bit signed integer holds; every band sits below it.
inline constexpr long long kInt32Max = 2147483647LL;
// The legacy seeds: the pieces made before the rolls (Mark1Set.hpp's seed
// column until this change), one per piece in piece order.
inline constexpr long long kLegacySeedLo = 777500LL;

// The owner's per-piece rolls (2026-10-22, "Bulwark SS"), inclusive ranges, in
// table order: the first rolled stat is the codec's major digit. The socket
// count is item stat key 20, which the Custom Forge writes after the game's
// own socket roll. No magic skill damage, cast speed, crit, attack speed,
// general damage reduction or movement speed on any piece (the owner's scope).
inline constexpr PieceRolls kPieceRolls[kPieceCount] = {
    // Helmet: 21 x 61 x 21 x 2 x 2 = 107,604 rolls; +35 Vitality fixed.
    { Piece::Helmet, kBandBase + 0 * kBandWidth, 107604LL, kLegacySeedLo + 0, 5,
      { { kKeyDefense, 60, 80 }, { kKeyEnhancedDefense, 120, 180 }, { kKeyAllResistances, 10, 30 },
        { kKeyAllSkills, 2, 3 }, { kKeySockets, 1, 2 } },
      1, { { kKeyVitality, 35 } } },
    // Body Armor: 31 x 61 x 101 x 2 x 2 = 763,964 rolls; 125% Faster Hit Recovery fixed.
    { Piece::Body, kBandBase + 1 * kBandWidth, 763964LL, kLegacySeedLo + 1, 5,
      { { kKeyDefense, 210, 240 }, { kKeyEnhancedDefense, 220, 280 }, { kKeyLife, 300, 400 },
        { kKeyAllSkills, 3, 4 }, { kKeySockets, 3, 4 } },
      1, { { kKeyFasterHitRecovery, 125 } } },
    // Boots: 19 x 51 x 11 x 11 = 117,249 rolls; nothing fixed, no sockets.
    { Piece::Boots, kBandBase + 2 * kBandWidth, 117249LL, kLegacySeedLo + 2, 4,
      { { kKeyDefense, 72, 90 }, { kKeyEnhancedDefense, 150, 200 }, { kKeyLifeAfterEachKill, 15, 25 },
        { kKeyAllResistances, 20, 30 }, { 0, 0, 0 } },
      0, { { 0, 0 } } },
    // Gloves: 23 x 26 x 2 = 1,196 rolls; +20 All Attributes fixed.
    { Piece::Gloves, kBandBase + 3 * kBandWidth, 1196LL, kLegacySeedLo + 3, 3,
      { { kKeyDefense, 46, 68 }, { kKeyEnhancedDefense, 75, 100 }, { kKeyAllSkills, 2, 3 },
        { 0, 0, 0 }, { 0, 0, 0 } },
      1, { { kKeyAllAttributes, 20 } } },
};

inline constexpr long long RollsIn(const PieceRolls& r)
{
    long long n = 1;
    for (int k = 0; k < r.rolledStats; ++k) n *= static_cast<long long>(r.rolls[k].hi - r.rolls[k].lo + 1);
    return n;
}

// Each band holds exactly its piece's roll count, fits inside its million, the
// bands run in piece order from kBandBase, sit above every measured natural
// seed and below 2^31 - 1, and the legacy seeds are 777500..777503 in piece
// order. A save keeps these seeds after release, so the layout is a format.
inline constexpr bool BandsExact()
{
    for (int i = 0; i < kPieceCount; ++i) {
        const PieceRolls& r = kPieceRolls[i];
        if (static_cast<int>(r.piece) != i) return false;
        if (r.rolledStats < 1 || r.rolledStats > kMaxRolls || r.fixedCount < 0 || r.fixedCount > kMaxFixed) return false;
        if (RollsIn(r) != r.rollTotal || r.rollTotal > kBandWidth) return false;
        if (r.bandLo != kBandBase + i * kBandWidth) return false;
        if (r.bandLo <= kHighestNaturalSeedMeasured || r.bandLo + kBandWidth - 1 > kInt32Max) return false;
        if (r.legacySeed != kLegacySeedLo + i) return false;
        for (int k = 0; k < r.rolledStats; ++k)
            if (r.rolls[k].lo > r.rolls[k].hi) return false;
    }
    return true;
}
static_assert(BandsExact(), "each piece's band holds exactly its rolls, in piece order from 1,000,000,000, legacy seeds 777500..777503");
static_assert(RollsIn(kPieceRolls[0]) == 107604 && RollsIn(kPieceRolls[1]) == 763964 &&
              RollsIn(kPieceRolls[2]) == 117249 && RollsIn(kPieceRolls[3]) == 1196,
              "the roll counts the owner's ranges give: helmet 107,604, body 763,964, boots 117,249, gloves 1,196");

inline const PieceRolls& RowOf(Piece p)
{
    const int i = static_cast<int>(p);
    return kPieceRolls[(i >= 0 && i < kPieceCount) ? i : 0];
}

// A decoded piece: which one, whether its seed is a legacy one, and its rolled
// values in table order.
struct Rolled {
    Piece piece = Piece::Helmet;
    bool  legacy = false;
    int   count = 0;
    int   key[kMaxRolls] = { 0, 0, 0, 0, 0 };
    int   value[kMaxRolls] = { 0, 0, 0, 0, 0 };
};

// The seed for a piece's roll, the values in table order (extra values are
// ignored, missing ones are an error), or -1 when a value is outside its range.
inline long long SeedFor(Piece p, std::initializer_list<int> values)
{
    const int i = static_cast<int>(p);
    if (i < 0 || i >= kPieceCount) return -1;
    const PieceRolls& r = kPieceRolls[i];
    if (static_cast<int>(values.size()) < r.rolledStats) return -1;
    long long offset = 0;
    int k = 0;
    for (int v : values) {
        if (k >= r.rolledStats) break;
        const StatRoll& s = r.rolls[k];
        if (v < s.lo || v > s.hi) return -1;
        offset = offset * (s.hi - s.lo + 1) + (v - s.lo);
        ++k;
    }
    return r.bandLo + offset;
}

// The best roll's seed (`mark1probe spawn <piece> top`) and the worst one's
// (`low`).
inline long long TopSeed(Piece p) { const PieceRolls& r = RowOf(p); return r.bandLo + r.rollTotal - 1; }
inline long long LowSeed(Piece p) { return RowOf(p).bandLo; }

// A seed for a uniformly drawn roll, u in [0, 1), as the flight charms' drops
// draw theirs (FlightCharms.hpp's SeedFromUniform). Every roll of the band is
// equally likely; u outside the range is clamped to its ends (NaN to the low).
inline long long SeedFromUniform(Piece p, double u)
{
    const PieceRolls& r = RowOf(p);
    if (!(u > 0.0)) return r.bandLo;
    long long k = static_cast<long long>(std::floor(u * static_cast<double>(r.rollTotal)));
    if (k >= r.rollTotal) k = r.rollTotal - 1;
    return r.bandLo + k;
}

// The seed alone: which piece, and its rolls. A band seed decodes mixed radix;
// a legacy seed decodes to every stat at the bottom of its range (the lower
// socket count included). Any other seed is no piece. The class check is the
// caller's (Mark1Set.hpp's IsPiece).
inline bool DecodeSeed(long long a, Rolled& out)
{
    for (const PieceRolls& r : kPieceRolls) {
        const bool legacy = a == r.legacySeed;
        if (!legacy && (a < r.bandLo || a >= r.bandLo + r.rollTotal)) continue;
        Rolled d;
        d.piece = r.piece;
        d.legacy = legacy;
        d.count = r.rolledStats;
        long long rest = legacy ? 0 : a - r.bandLo;
        for (int k = r.rolledStats - 1; k >= 0; --k) {
            const StatRoll& s = r.rolls[k];
            const long long span = s.hi - s.lo + 1;
            d.key[k] = s.key;
            d.value[k] = static_cast<int>(s.lo + rest % span);
            rest /= span;
        }
        out = d;
        return true;
    }
    return false;
}

// The stats the dressing writes into the Custom Forge entry, as (item stat
// key, whole value): the rolled stats in table order without the socket
// count, then the fixed ones, then the socket count (key 20) last, the way
// the forge writes it after the game's own socket roll. A piece with no
// socket roll (boots, gloves) writes no key 20.
inline std::vector<std::pair<int, int>> StatsFor(const Rolled& r)
{
    std::vector<std::pair<int, int>> out;
    const PieceRolls& row = RowOf(r.piece);
    int sockets = -1;
    for (int k = 0; k < r.count && k < kMaxRolls; ++k) {
        if (r.key[k] == kKeySockets) { sockets = r.value[k]; continue; }
        out.emplace_back(r.key[k], r.value[k]);
    }
    for (int k = 0; k < row.fixedCount && k < kMaxFixed; ++k) out.emplace_back(row.fixed[k].key, row.fixed[k].value);
    if (sockets >= 0) out.emplace_back(kKeySockets, sockets);
    return out;
}

// ---- the set bonus ----------------------------------------------------------------

// How a bonus line takes effect.
enum class Applied : int {
    Dispatcher,   // answered through ReturnSpecificStat at its dispatcher id
    Automation,   // the automation and the jump (Mark1Automation.hpp, Mark1Jump.hpp), unchanged
    HalfWalk,     // the half walk (Mark1Movement.hpp, dispatcher id 64), unchanged; this table never answers it
};

// ReturnSpecificStat's ids the bonus answers: a static reading of the
// dispatcher's case map (rss_cases.json, the 2026-10-04 indexed decompile).
inline constexpr int kIdAllSkills = 2;              // StatAllSkills
inline constexpr int kIdMaxLife = 36;               // StatMaxLife
inline constexpr int kIdLifeReplenish = 38;         // StatLifeReplenish
inline constexpr int kIdMovementSpeed = 64;         // StatMovementSpeed (the half walk's)
inline constexpr int kIdDefense = 92;               // StatDefense
inline constexpr int kIdPhysicalDamageReduction = 97; // StatPhysicalDamageReduction
inline constexpr int kIdAllResistances = 161;       // StatAllResistances
// The element resistances: Fire, Cold, Poison, Lightning, Arcane.
inline constexpr int kIdElementResistances[5] = { 162, 163, 164, 165, 167 };

// Whether the element resistances include the all-resistance answer through
// the dispatcher is not read. Route token `allres-route: dispatcher` answers
// id 161 only; `allres-route: elements` also answers each element id +35.
// Live 1's `allres-route` decides; the build ships `dispatcher`.
inline constexpr std::string_view kAllResRoute = "dispatcher";
inline constexpr bool kAllResAnswersElements = kAllResRoute == "elements";

// The tooltip's bonus row colours: hs-game-sdk/curated/item_info.json,
// tooltip.set_section.colours (measured).
inline constexpr int kColourLit = 7844807;    // bonus_active
inline constexpr int kColourGrey = 4210752;   // bonus_inactive

// The game's set-line formats and styles (DrawInventoryStatsNew's arguments 5
// and 6, measured from Item Truth's captures; item_stat_keys.json's
// `set_lines`).
inline constexpr int kFormatPercent = 5;
inline constexpr int kFormatFlat = 6;
inline constexpr int kStyleValueFirst = 8;   // value first, signed
inline constexpr int kStyleLabelFirst = 9;   // label first

struct BonusLine {
    int              threshold;   // distinct pieces worn for the line to be on
    Applied          applied;
    int              id;          // the dispatcher id answered (Dispatcher), the lever's id (HalfWalk), 0 otherwise
    double           value;       // added to the call's own original (Dispatcher); the drawn value otherwise
    std::string_view label;       // the game's label for the line (item_stat_keys.json's `set_lines`)
    int              format;
    int              style;
    std::string_view words;       // the row's exact words, as the game draws a set line
};

// The owner's set bonus (2026-10-22, "Bulwark SS"), cumulative, in tooltip
// order. The words are value then label, as the game draws a set line
// (measured, item_stat_keys.json's `set_lines`).
inline constexpr int kBonusLineCount = 8;
inline constexpr BonusLine kBonusLines[kBonusLineCount] = {
    { 2, Applied::Dispatcher, kIdDefense, 500, "Defense", kFormatFlat, kStyleValueFirst, "+500 Defense" },
    { 3, Applied::Dispatcher, kIdAllResistances, 35, "to All Resistances", kFormatPercent, kStyleValueFirst, "+35% to All Resistances" },
    { 3, Applied::Dispatcher, kIdLifeReplenish, 25, "Replenish Life", kFormatPercent, kStyleLabelFirst, "Replenish Life 25%" },
    { 4, Applied::Automation, 0, 1, "to Automation", kFormatFlat, kStyleValueFirst, "+1 to Automation" },
    { 4, Applied::Dispatcher, kIdAllSkills, 5, "to All Skills", kFormatFlat, kStyleValueFirst, "+5 to All Skills" },
    { 4, Applied::Dispatcher, kIdMaxLife, 750, "to Life", kFormatFlat, kStyleValueFirst, "+750 to Life" },
    { 4, Applied::Dispatcher, kIdPhysicalDamageReduction, 20, "Physical Damage Taken Reduced by", kFormatPercent, kStyleLabelFirst, "Physical Damage Taken Reduced by 20%" },
    { 4, Applied::HalfWalk, kIdMovementSpeed, -50, "Movement Speed", kFormatPercent, kStyleValueFirst, "-50% Movement Speed" },
};

// The thresholds, in order (the stat line's `lit=` reads one flag per threshold).
inline constexpr int kThresholds[3] = { 2, 3, 4 };

// Whether the bonus is on at all: a count of at least the lowest threshold,
// the switch on and the bonus installed.
inline bool BonusActive(int piecesWorn, bool switchOn, bool installed)
{
    return switchOn && installed && piecesWorn >= kThresholds[0];
}

// Which lines are lit for a count of distinct pieces worn: a line is lit while
// at least its threshold is worn, the switch is on and the bonus installed.
// Every line is grey otherwise.
inline std::array<bool, kBonusLineCount> LitLines(int piecesWorn, bool switchOn, bool installed)
{
    std::array<bool, kBonusLineCount> lit{};
    for (int i = 0; i < kBonusLineCount; ++i)
        lit[i] = switchOn && installed && piecesWorn >= kBonusLines[i].threshold;
    return lit;
}

// Is this dispatcher id one the bonus answers? Asked before anything else, so
// no other call costs more. The element ids belong only under the `elements`
// route.
inline bool BonusWants(double statId)
{
    for (const BonusLine& l : kBonusLines)
        if (l.applied == Applied::Dispatcher && statId == static_cast<double>(l.id)) return true;
    if (kAllResAnswersElements)
        for (int id : kIdElementResistances)
            if (statId == static_cast<double>(id)) return true;
    return false;
}

// What the lit lines add to one dispatcher id's answer at a count (0 when none
// applies). Under the `elements` route an element id gets All Resistances'.
inline double BonusAddend(double statId, int piecesWorn, bool switchOn, bool installed, bool answerElements = kAllResAnswersElements)
{
    if (!switchOn || !installed) return 0.0;
    double add = 0.0;
    for (const BonusLine& l : kBonusLines) {
        if (l.applied != Applied::Dispatcher || piecesWorn < l.threshold) continue;
        if (statId == static_cast<double>(l.id)) add += l.value;
        if (answerElements && l.id == kIdAllResistances)
            for (int id : kIdElementResistances)
                if (statId == static_cast<double>(id)) add += l.value;
    }
    return add;
}

// One dispatcher call's answer: its own original plus every lit line's value
// for its id. Inactive, another id, or an original that is not a finite number
// (the adapter counts that `not-number=`): the original, unchanged. A pure
// function of its arguments, so repeated calls never compound.
inline double BonusAnswer(double statId, int piecesWorn, bool switchOn, bool installed, double original,
                          bool answerElements = kAllResAnswersElements)
{
    if (!std::isfinite(original)) return original;
    return original + BonusAddend(statId, piecesWorn, switchOn, installed, answerElements);
}

} // namespace ForgePact::Mark1Rolls
