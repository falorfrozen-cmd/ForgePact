// Behavioral regression harness for gambapity's decision core
// (GambaPity.hpp, ForgePact #134 phase 2, player build).
//
// The Python runner injects the REAL ForgePact::GambaPity header below. No
// game is touched: a call's self is a plain bool the adapter hands over (its
// machine-self predicate), and the charm identifier lives in ModuleMain.cpp's
// adapter, never here.
//
// Baseline: off, or on with a count not yet reached, every prize roll runs the
// game's own roll, nothing but a spin moves the counter, and nothing is ever
// forced. Target: with the threshold reached, the next prize roll forces the
// charm and resets; a spin before the threshold never forces; a natural charm
// drop resets; a spin on another object's call never counts; the gold
// equivalent is count * 10000; and the status line reads back the state.
#include <cstdint>
#include <iostream>
#include <string>

// PRODUCTION_GAMBAPITY

using namespace ForgePact::GambaPity;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

int main()
{
    // ---- the constants and the accessor -------------------------------------
    {
        check("table/gold_per_spin", kGoldPerSpin == 10000);
        Pity p;
        check("core/gold_equivalent_is_count_times_10000", p.GoldEquivalent() == 0 && p.Count() == 0);
        p.SetEnabled(true);
        p.OnSpin(true);
        p.OnSpin(true);
        p.OnSpin(true);
        check("core/gold_equivalent_tracks_the_count", p.Count() == 3 && p.GoldEquivalent() == 30000, p.StatusLine());
    }

    // ---- baseline: off, nothing happens -------------------------------------
    {
        Pity p;
        p.SetThreshold(100);
        bool counted = false;
        for (int i = 0; i < 100; ++i) counted = counted || p.OnSpin(true);
        check("baseline/off_a_spin_never_counts", !counted && p.Count() == 0);
        bool forced = false;
        for (int i = 0; i < 5; ++i) forced = forced || p.OnPrizeRoll(true);
        check("baseline/off_a_prize_roll_never_forces", !forced && p.Count() == 0);
        p.OnNaturalDrop();
        check("baseline/off_a_natural_drop_leaves_the_count", p.Count() == 0);
        check("baseline/off_status", p.StatusLine() == "gambapity: off count=0 threshold=100 gold=0", p.StatusLine());
    }
    {
        // On, but the count has not reached the threshold: the game's own roll runs.
        Pity p;
        p.SetThreshold(3);
        p.SetEnabled(true);
        p.OnSpin(true);
        p.OnSpin(true);
        const bool forced = p.OnPrizeRoll(true);
        check("baseline/on_below_the_threshold_runs_the_games_own_roll", !forced && p.Count() == 2, p.StatusLine());
        check("baseline/on_below_the_threshold_status", p.StatusLine() == "gambapity: on count=2 threshold=3 gold=20000", p.StatusLine());
    }

    // ---- the counter: nothing but a spin moves it ---------------------------
    {
        Pity p;
        p.SetThreshold(10);
        p.SetEnabled(true);
        // A prize roll before any spin does not move the counter.
        p.OnPrizeRoll(true);
        p.OnPrizeRoll(false);
        // A spin on another object's call never counts.
        const bool other = p.OnSpin(false);
        check("counter/another_objects_spin_never_counts", !other && p.Count() == 0);
        const bool machine = p.OnSpin(true);
        check("counter/only_a_machine_self_spin_counts", machine && p.Count() == 1);
        // A natural drop resets.
        p.OnNaturalDrop();
        check("counter/a_natural_drop_resets", p.Count() == 0);
        // `off` keeps the counter.
        p.OnSpin(true);
        p.OnSpin(true);
        p.Off();
        check("counter/off_keeps_the_counter", p.Count() == 2 && !p.Enabled(), p.StatusLine());
    }

    // ---- target: the threshold reached forces and resets --------------------
    {
        Pity p;
        p.SetThreshold(3);
        p.SetEnabled(true);
        bool before = false;
        p.OnSpin(true); // 1
        before = before || p.OnPrizeRoll(true);
        p.OnSpin(true); // 2
        before = before || p.OnPrizeRoll(true);
        check("target/a_spin_before_the_threshold_never_forces", !before && p.Count() == 2, p.StatusLine());
        p.OnSpin(true); // 3 -> the threshold
        const bool forced = p.OnPrizeRoll(true);
        check("target/the_next_prize_roll_forces_the_charm", forced && p.Count() == 0, p.StatusLine());
        // After the reset, the guarantee starts over.
        const bool again = p.OnPrizeRoll(true);
        check("target/after_the_reset_the_roll_is_the_games_own_again", !again && p.Count() == 0, p.StatusLine());
    }
    {
        // A prize roll for another self never forces, even at the threshold.
        Pity p;
        p.SetThreshold(2);
        p.SetEnabled(true);
        p.OnSpin(true);
        p.OnSpin(true);
        const bool other = p.OnPrizeRoll(false);
        check("target/another_selfs_roll_never_forces", !other && p.Count() == 2);
        const bool machine = p.OnPrizeRoll(true);
        check("target/the_machines_own_roll_forces_at_the_threshold", machine && p.Count() == 0);
    }
    {
        // The threshold reached by more spins than the threshold still forces
        // exactly once and resets.
        Pity p;
        p.SetThreshold(5);
        p.SetEnabled(true);
        for (int i = 0; i < 12; ++i) p.OnSpin(true);
        check("target/count_past_the_threshold_still_forces", p.OnPrizeRoll(true) && p.Count() == 0
            && !p.OnPrizeRoll(true) && p.Count() == 0, p.StatusLine());
    }
    {
        // A zero threshold never forces: it is the unset state, not a count.
        Pity p;
        p.SetEnabled(true);
        check("target/a_zero_threshold_never_forces", !p.OnPrizeRoll(true) && p.Count() == 0);
    }
    {
        // A natural drop at the threshold also resets, without a force.
        Pity p;
        p.SetThreshold(2);
        p.SetEnabled(true);
        p.OnSpin(true);
        p.OnSpin(true);
        p.OnNaturalDrop();
        check("target/a_natural_drop_resets_the_threshold_reached", p.Count() == 0 && !p.OnPrizeRoll(true));
    }

    // ---- the status line reads back the state -------------------------------
    {
        Pity p;
        p.SetThreshold(100);
        p.SetEnabled(true);
        p.OnSpin(true);
        check("status/line_names_count_threshold_and_gold",
              p.StatusLine() == "gambapity: on count=1 threshold=100 gold=10000", p.StatusLine());
        p.Off();
        check("status/off_keeps_the_count_in_the_line",
              p.StatusLine() == "gambapity: off count=1 threshold=100 gold=10000", p.StatusLine());
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
