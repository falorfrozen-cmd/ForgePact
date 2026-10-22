// Behavioral regression harness for the Mark 1 pieces' rolls and the set
// bonus (Mark1Rolls.hpp).
//
// The Python runner injects the REAL ForgePact::Mark1Rolls code below. Nothing
// of the game is touched: each scenario hands the core a definition seed, a
// roll, a uniform draw, or one ReturnSpecificStat call's id and original with
// the worn count, the switch and the install, and reads back what the
// dressing, the drops and the dispatcher's caller would get.
//
// Besides the PASS/FAIL lines it prints the tables (ROLL, FIXED, BONUSLINE
// and ROUTE lines) so test_mark1_rolls_behavior.py can check them against the
// hub's hs-game-sdk/curated/item_stat_keys.json.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// PRODUCTION_MARK1ROLLS

namespace M1r = ForgePact::Mark1Rolls;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string Num(double v) { std::ostringstream o; o << v; return o.str(); }

using Stats = std::vector<std::pair<int, int>>;

static std::string Show(const Stats& s) {
    std::string out;
    for (const auto& kv : s) out += (out.empty() ? "" : ",") + std::to_string(kv.first) + "=" + std::to_string(kv.second);
    return out;
}

static Stats StatsOfSeed(long long a, bool& ok) {
    M1r::Rolled r;
    ok = M1r::DecodeSeed(a, r);
    return ok ? M1r::StatsFor(r) : Stats{};
}

static const char* PieceWord(M1r::Piece p) {
    switch (p) {
    case M1r::Piece::Helmet: return "helmet";
    case M1r::Piece::Body: return "body";
    case M1r::Piece::Boots: return "boots";
    case M1r::Piece::Gloves: return "gloves";
    }
    return "?";
}

static const char* AppliedWord(M1r::Applied a) {
    switch (a) {
    case M1r::Applied::Dispatcher: return "dispatcher";
    case M1r::Applied::Automation: return "automation";
    case M1r::Applied::HalfWalk: return "halfwalk";
    }
    return "?";
}

// The values of one roll, by a corner mask: bit k set takes roll k's top.
static std::vector<int> Corner(const M1r::PieceRolls& r, unsigned mask) {
    std::vector<int> v;
    for (int k = 0; k < r.rolledStats; ++k) v.push_back((mask >> k) & 1u ? r.rolls[k].hi : r.rolls[k].lo);
    return v;
}

static long long SeedOf(M1r::Piece p, const std::vector<int>& v) {
    switch (v.size()) {
    case 3: return M1r::SeedFor(p, { v[0], v[1], v[2] });
    case 4: return M1r::SeedFor(p, { v[0], v[1], v[2], v[3] });
    case 5: return M1r::SeedFor(p, { v[0], v[1], v[2], v[3], v[4] });
    }
    return -2;
}

int main() {
    // ---- the tables, for the curated-file test ---------------------------------
    for (const M1r::PieceRolls& r : M1r::kPieceRolls) {
        for (int k = 0; k < r.rolledStats; ++k)
            std::cout << "ROLL " << PieceWord(r.piece) << " " << r.rolls[k].key << " " << r.rolls[k].lo << " " << r.rolls[k].hi << std::endl;
        for (int k = 0; k < r.fixedCount; ++k)
            std::cout << "FIXED " << PieceWord(r.piece) << " " << r.fixed[k].key << " " << r.fixed[k].value << std::endl;
        std::cout << "BAND " << PieceWord(r.piece) << " " << r.bandLo << " " << r.rollTotal << " " << r.legacySeed << std::endl;
    }
    for (const M1r::BonusLine& l : M1r::kBonusLines)
        std::cout << "BONUSLINE " << l.threshold << " " << AppliedWord(l.applied) << " " << l.id << " " << Num(l.value) << " "
                  << l.format << " " << l.style << "|" << l.label << "|" << l.words << std::endl;
    std::cout << "ROUTE allres-route: " << M1r::kAllResRoute << std::endl;

    // ---- baseline: the legacy seeds stay pieces, at the bottom roll ------------
    {
        bool all = true, bottom = true;
        std::string detail;
        for (int i = 0; i < M1r::kPieceCount; ++i) {
            const M1r::PieceRolls& row = M1r::kPieceRolls[i];
            M1r::Rolled r;
            if (!M1r::DecodeSeed(777500 + i, r) || static_cast<int>(r.piece) != i || !r.legacy) { all = false; continue; }
            for (int k = 0; k < row.rolledStats; ++k)
                if (r.key[k] != row.rolls[k].key || r.value[k] != row.rolls[k].lo) bottom = false;
            // The legacy roll is the band's own lowest roll.
            M1r::Rolled low;
            if (!M1r::DecodeSeed(M1r::LowSeed(row.piece), low) || M1r::StatsFor(low) != M1r::StatsFor(r)) bottom = false;
            detail += std::string(PieceWord(row.piece)) + "[" + Show(M1r::StatsFor(r)) + "] ";
        }
        check("baseline/legacy_seeds_decode_to_their_piece", all);
        check("baseline/legacy_seeds_roll_the_bottom_of_every_range", bottom, detail);
        bool ok = false;
        const Stats helmet = StatsOfSeed(777500, ok);
        check("baseline/legacy_helmet_has_the_lower_socket_count",
              ok && helmet == Stats{ { 154, 60 }, { 29, 120 }, { 173, 10 }, { 201, 2 }, { 48, 35 }, { 20, 1 } }, Show(helmet));
        const Stats body = StatsOfSeed(777501, ok);
        check("baseline/legacy_body_has_the_lower_socket_count",
              ok && body == Stats{ { 154, 210 }, { 29, 220 }, { 52, 300 }, { 201, 3 }, { 170, 125 }, { 20, 3 } }, Show(body));
        M1r::Rolled band;
        check("baseline/a_band_seed_is_not_legacy", M1r::DecodeSeed(M1r::LowSeed(M1r::Piece::Boots), band) && !band.legacy);
    }

    // ---- baseline: natural seeds are no pieces -----------------------------------
    {
        const std::vector<long long> natural = {
            0, 1, -1, 777001, 777003, 777100, 777200, 777343, 777400, 777490, 777499, 777504, 779000,
            100000000, 999981452, 999999999,
            1000000000LL - 1,
            1000000000LL + 107604,              // one past the helmet's band
            1001000000LL + 763964,              // one past the body's
            1002000000LL + 117249,              // one past the boots'
            1003000000LL + 1196,                // one past the gloves'
            1003999999LL, 1004000000LL, 2147483647LL, 4000000000LL,
        };
        std::string hit;
        for (long long a : natural) {
            M1r::Rolled r;
            if (M1r::DecodeSeed(a, r)) hit += std::to_string(a) + " ";
        }
        check("baseline/natural_seeds_are_not_pieces", hit.empty(), hit);
        check("baseline/bands_sit_above_every_natural_seed_measured",
              M1r::kPieceRolls[0].bandLo > M1r::kHighestNaturalSeedMeasured && M1r::kHighestNaturalSeedMeasured == 999981452);
        check("baseline/bands_fit_a_32_bit_signed_integer",
              M1r::TopSeed(M1r::Piece::Gloves) <= 2147483647LL && M1r::kPieceRolls[3].bandLo + M1r::kBandWidth - 1 <= 2147483647LL);
    }

    // ---- target: the codec round-trips every corner, and every seed --------------
    {
        bool corners = true, bounds = true, exhaustive = true, outside = true;
        std::string detail;
        for (const M1r::PieceRolls& row : M1r::kPieceRolls) {
            for (unsigned mask = 0; mask < (1u << row.rolledStats); ++mask) {
                const std::vector<int> v = Corner(row, mask);
                const long long seed = SeedOf(row.piece, v);
                M1r::Rolled r;
                if (seed < 0 || !M1r::DecodeSeed(seed, r) || r.piece != row.piece || r.legacy) { corners = false; continue; }
                for (int k = 0; k < row.rolledStats; ++k)
                    if (r.value[k] != v[k] || r.key[k] != row.rolls[k].key) corners = false;
            }
            if (SeedOf(row.piece, Corner(row, 0)) != row.bandLo) bounds = false;
            if (SeedOf(row.piece, Corner(row, (1u << row.rolledStats) - 1)) != row.bandLo + row.rollTotal - 1) bounds = false;
            if (M1r::LowSeed(row.piece) != row.bandLo || M1r::TopSeed(row.piece) != row.bandLo + row.rollTotal - 1) bounds = false;
            // Every seed of the band decodes inside its ranges and encodes back to itself.
            for (long long a = row.bandLo; a < row.bandLo + row.rollTotal; ++a) {
                M1r::Rolled r;
                if (!M1r::DecodeSeed(a, r) || r.piece != row.piece) { exhaustive = false; break; }
                std::vector<int> v(r.value, r.value + r.count);
                for (int k = 0; k < r.count; ++k)
                    if (v[k] < row.rolls[k].lo || v[k] > row.rolls[k].hi) exhaustive = false;
                if (SeedOf(row.piece, v) != a) { exhaustive = false; detail = "seed " + std::to_string(a); break; }
            }
            // A value outside its range, or too few values, has no seed.
            std::vector<int> v = Corner(row, 0);
            v[0] = row.rolls[0].lo - 1;
            if (SeedOf(row.piece, v) != -1) outside = false;
            v = Corner(row, (1u << row.rolledStats) - 1);
            v[row.rolledStats - 1] = row.rolls[row.rolledStats - 1].hi + 1;
            if (SeedOf(row.piece, v) != -1) outside = false;
        }
        if (M1r::SeedFor(M1r::Piece::Helmet, { 60, 120 }) != -1) outside = false;
        check("target/codec_round_trips_every_corner", corners);
        check("target/codec_bounds_are_the_band_ends", bounds);
        check("target/codec_round_trips_every_seed", exhaustive, detail);
        check("target/codec_refuses_a_value_outside_its_range", outside);
        // The first rolled stat is the major digit: one more Defense moves the
        // helmet's seed by the product of the other ranges (61 x 21 x 2 x 2).
        check("target/codec_first_stat_is_the_major_digit",
              M1r::SeedFor(M1r::Piece::Helmet, { 61, 120, 10, 2, 1 }) - M1r::LowSeed(M1r::Piece::Helmet) == 61 * 21 * 2 * 2 &&
              M1r::SeedFor(M1r::Piece::Helmet, { 60, 120, 10, 2, 2 }) - M1r::LowSeed(M1r::Piece::Helmet) == 1);
    }

    // ---- target: the seed for a uniform draw --------------------------------------
    {
        bool ok = true;
        for (const M1r::PieceRolls& row : M1r::kPieceRolls) {
            const M1r::Piece p = row.piece;
            if (M1r::SeedFromUniform(p, 0.0) != row.bandLo) ok = false;
            if (M1r::SeedFromUniform(p, -0.5) != row.bandLo) ok = false;
            if (M1r::SeedFromUniform(p, std::numeric_limits<double>::quiet_NaN()) != row.bandLo) ok = false;
            if (M1r::SeedFromUniform(p, 1.0) != M1r::TopSeed(p)) ok = false;
            if (M1r::SeedFromUniform(p, 2.0) != M1r::TopSeed(p)) ok = false;
            if (M1r::SeedFromUniform(p, std::nextafter(1.0, 0.0)) != M1r::TopSeed(p)) ok = false;
            if (M1r::SeedFromUniform(p, 0.5) != row.bandLo + row.rollTotal / 2) ok = false;
            M1r::Rolled r;
            for (double u : { 0.1, 0.25, 0.333, 0.75, 0.9 })
                if (!M1r::DecodeSeed(M1r::SeedFromUniform(p, u), r) || r.piece != p) ok = false;
        }
        check("target/uniform_draw_spans_the_band", ok);
    }

    // ---- target: the dressed stats per piece --------------------------------------
    {
        bool ok = false;
        Stats s = StatsOfSeed(M1r::TopSeed(M1r::Piece::Helmet), ok);
        check("target/stats_helmet_top", ok && s == Stats{ { 154, 80 }, { 29, 180 }, { 173, 30 }, { 201, 3 }, { 48, 35 }, { 20, 2 } }, Show(s));
        s = StatsOfSeed(M1r::LowSeed(M1r::Piece::Helmet), ok);
        check("target/stats_helmet_low", ok && s == Stats{ { 154, 60 }, { 29, 120 }, { 173, 10 }, { 201, 2 }, { 48, 35 }, { 20, 1 } }, Show(s));
        s = StatsOfSeed(M1r::TopSeed(M1r::Piece::Body), ok);
        check("target/stats_body_top", ok && s == Stats{ { 154, 240 }, { 29, 280 }, { 52, 400 }, { 201, 4 }, { 170, 125 }, { 20, 4 } }, Show(s));
        s = StatsOfSeed(M1r::SeedFor(M1r::Piece::Body, { 225, 250, 350, 3, 4 }), ok);
        check("target/stats_body_mid", ok && s == Stats{ { 154, 225 }, { 29, 250 }, { 52, 350 }, { 201, 3 }, { 170, 125 }, { 20, 4 } }, Show(s));
        s = StatsOfSeed(M1r::TopSeed(M1r::Piece::Boots), ok);
        check("target/stats_boots_top_no_sockets_nothing_fixed", ok && s == Stats{ { 154, 90 }, { 29, 200 }, { 59, 25 }, { 173, 30 } }, Show(s));
        s = StatsOfSeed(M1r::LowSeed(M1r::Piece::Boots), ok);
        check("target/stats_boots_low", ok && s == Stats{ { 154, 72 }, { 29, 150 }, { 59, 15 }, { 173, 20 } }, Show(s));
        s = StatsOfSeed(M1r::TopSeed(M1r::Piece::Gloves), ok);
        check("target/stats_gloves_top", ok && s == Stats{ { 154, 68 }, { 29, 100 }, { 201, 3 }, { 51, 20 } }, Show(s));
        s = StatsOfSeed(M1r::LowSeed(M1r::Piece::Gloves), ok);
        check("target/stats_gloves_low", ok && s == Stats{ { 154, 46 }, { 29, 75 }, { 201, 2 }, { 51, 20 } }, Show(s));
        // The owner's scope: no magic skill damage, cast speed, crit, attack
        // speed, general damage reduction or movement speed. Of the keys
        // item_stat_keys.json names, movement speed (25) and the cast-slowdown
        // reducer (294) are the ones near this set; neither appears.
        bool none = true;
        for (const M1r::PieceRolls& row : M1r::kPieceRolls) {
            for (int k = 0; k < row.rolledStats; ++k) if (row.rolls[k].key == 25 || row.rolls[k].key == 294) none = false;
            for (int k = 0; k < row.fixedCount; ++k) if (row.fixed[k].key == 25 || row.fixed[k].key == 294) none = false;
        }
        check("target/stats_no_movement_speed_on_any_piece", none);
    }

    // ---- baseline: the bonus is nothing below two pieces, or switched off --------
    {
        const std::vector<double> ids = { 2, 21, 36, 37, 38, 42, 64, 92, 97, 103, 106, 161, 162, 163, 164, 165, 167 };
        bool untouched = true;
        std::string detail;
        for (double id : ids) {
            for (int worn : { 0, 1 }) {
                for (bool on : { false, true }) {
                    for (bool installed : { false, true }) {
                        const double a = M1r::BonusAnswer(id, worn, on, installed, 151.0);
                        if (a != 151.0) { untouched = false; detail += Num(id) + "@" + std::to_string(worn) + " "; }
                    }
                }
            }
            for (int worn : { 2, 3, 4 }) {
                if (M1r::BonusAnswer(id, worn, false, true, 151.0) != 151.0) { untouched = false; detail += "off " + Num(id) + " "; }
                if (M1r::BonusAnswer(id, worn, true, false, 151.0) != 151.0) { untouched = false; detail += "uninstalled " + Num(id) + " "; }
            }
        }
        check("bonus_baseline/nothing_below_two_or_switched_off_or_not_installed", untouched, detail);
        bool grey = true;
        for (int worn : { 0, 1, 2, 3, 4 })
            for (bool lit : M1r::LitLines(worn, false, true)) if (lit) grey = false;
        for (int worn : { 0, 1, 2, 3, 4 })
            for (bool lit : M1r::LitLines(worn, true, false)) if (lit) grey = false;
        for (int worn : { 0, 1 })
            for (bool lit : M1r::LitLines(worn, true, true)) if (lit) grey = false;
        check("bonus_baseline/every_line_grey_below_two_or_switched_off", grey);
        check("bonus_baseline/not_active_below_two_or_switched_off",
              !M1r::BonusActive(1, true, true) && !M1r::BonusActive(4, false, true) && !M1r::BonusActive(4, true, false) &&
              M1r::BonusActive(2, true, true));
        // Ids the bonus never answers stay out of scope, at any count.
        bool scope = true;
        for (double id : { 21.0, 37.0, 42.0, 64.0, 103.0, 106.0, 0.0, -1.0, 92.5 }) {
            if (M1r::BonusWants(id)) scope = false;
            if (M1r::BonusAnswer(id, 4, true, true, 151.0) != 151.0) scope = false;
        }
        check("bonus_baseline/other_ids_untouched_even_with_four", scope);
    }

    // ---- target: the thresholds and the answers ------------------------------------
    {
        struct Want { double id; int worn; double answer; };
        const std::vector<Want> wants = {
            { 92, 2, 151 + 500 }, { 92, 3, 151 + 500 }, { 92, 4, 151 + 500 },
            { 161, 2, 151 }, { 161, 3, 151 + 35 }, { 161, 4, 151 + 35 },
            { 38, 2, 151 }, { 38, 3, 151 + 25 }, { 38, 4, 151 + 25 },
            { 2, 3, 151 }, { 2, 4, 151 + 5 },
            { 36, 3, 151 }, { 36, 4, 151 + 750 },
            { 97, 3, 151 }, { 97, 4, 151 + 20 },
            { 92, 7, 151 + 500 }, { 97, 9, 151 + 20 },   // more than four counts as four
        };
        bool ok = true;
        std::string detail;
        for (const Want& w : wants) {
            const double a = M1r::BonusAnswer(w.id, w.worn, true, true, 151.0);
            if (a != w.answer) { ok = false; detail += Num(w.id) + "@" + std::to_string(w.worn) + "=" + Num(a) + " "; }
        }
        check("bonus_target/answers_at_each_threshold", ok, detail);
        // Each call is answered from its own original: answering the same
        // original twice gives the same answer, and a negative original too.
        const double once = M1r::BonusAnswer(36, 4, true, true, 1200.0);
        const double again = M1r::BonusAnswer(36, 4, true, true, 1200.0);
        check("bonus_target/each_call_from_its_own_original",
              once == 1950.0 && again == 1950.0 && M1r::BonusAnswer(161, 3, true, true, -125.0) == -90.0, Num(once));
        // A non-number original is left as it is.
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const double inf = std::numeric_limits<double>::infinity();
        check("bonus_target/non_finite_original_left_alone",
              std::isnan(M1r::BonusAnswer(92, 4, true, true, nan)) && M1r::BonusAnswer(92, 4, true, true, inf) == inf);
        // The dispatcher ids the bonus wants: the six it answers, nothing else.
        bool wanted = true;
        for (double id : { 2.0, 36.0, 38.0, 92.0, 97.0, 161.0 }) if (!M1r::BonusWants(id)) wanted = false;
        check("bonus_target/wants_the_six_answered_ids", wanted);
        // The route token: `dispatcher` answers 161 only; `elements` adds the
        // element ids, exercised here through the core's own switch.
        bool route = M1r::kAllResRoute == "dispatcher" && !M1r::kAllResAnswersElements;
        for (int id : M1r::kIdElementResistances) {
            if (M1r::BonusWants(id) || M1r::BonusAnswer(id, 4, true, true, 40.0) != 40.0) route = false;
            if (M1r::BonusAnswer(id, 3, true, true, 40.0, true) != 75.0) route = false;
            if (M1r::BonusAnswer(id, 2, true, true, 40.0, true) != 40.0) route = false;
        }
        check("bonus_target/allres_route_dispatcher_answers_161_only", route);
        // The lit lines per count, in tooltip order.
        auto bits = [](int worn) {
            std::string s;
            for (bool lit : M1r::LitLines(worn, true, true)) s += lit ? '1' : '0';
            return s;
        };
        check("bonus_target/lit_lines_by_count",
              bits(2) == "10000000" && bits(3) == "11100000" && bits(4) == "11111111" && bits(5) == "11111111",
              bits(2) + "/" + bits(3) + "/" + bits(4));
        // The words are the value then the label (or the label then the value
        // for style 9), as the game draws a set line.
        bool words = true;
        for (const M1r::BonusLine& l : M1r::kBonusLines) {
            const std::string w(l.words), label(l.label);
            const bool percent = l.format == M1r::kFormatPercent;
            const std::string value = (l.value >= 0 && l.style == M1r::kStyleValueFirst ? "+" : "") + Num(l.value) + (percent ? "%" : "");
            const std::string want = l.style == M1r::kStyleValueFirst ? value + " " + label : label + " " + value;
            if (w != want) { words = false; std::cout << "  words " << w << " != " << want << std::endl; }
        }
        check("bonus_target/words_are_value_and_label", words);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << std::endl;
    return failures ? 1 : 0;
}
