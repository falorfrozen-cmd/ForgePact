// Behavioral regression harness for the Mark 1 set's movement levers
// (Mark1Movement.hpp).
//
// The Python runner injects the REAL ForgePact::Mark1Movement code below.
// Nothing of the game is touched: each scenario hands the core what the
// adapter in ModuleMain.cpp reads off one ReturnSpecificStat call (whether its
// self is the local player, its stat-id argument, whether the answer is a
// number, the answer itself) and whether the set's bonus is active, and reads
// back the answer the caller would get and the core's counts.
//
// The walk's step is proportional to the id-64 answer plus kStepOffset
// (measured, Mod live 1; hs_game_sdk/movement_speed_model.py), so a step
// ratio here is (answer' + offset) / (answer + offset), the model's
// step_ratio, with no frame scale needed.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// PRODUCTION_MARK1MOVEMENT

namespace M1m = ForgePact::Mark1Movement;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string Num(double v) { std::ostringstream o; o << v; return o.str(); }

// What the walk's step becomes, relative to the step at `answer`.
static double StepRatio(double changed, double answer) {
    return (changed + M1m::kStepOffset) / (answer + M1m::kStepOffset);
}

// One ReturnSpecificStat call through the core, the way the adapter runs it:
// the scope is asked first, the bonus only for a call in scope.
struct Call {
    bool local = true;
    bool haveStatId = true;
    double statId = 64;
    bool isNumber = true;
    double answer = 151;
};
struct Seen {
    double answer = 0;       // what the caller reads
    bool changed = false;
    int bonusAsks = 0;       // how often the bonus was asked
};
static Seen Run(M1m::Speed& speed, const Call& c, bool active) {
    Seen s;
    s.answer = c.answer;
    if (!M1m::Speed::Wants(c.local, c.haveStatId, c.statId)) return s;
    ++s.bonusAsks;
    s.changed = speed.Answer(active, c.isNumber, s.answer);
    return s;
}

int main() {
    // Sorak's id-64 answer every frame of Mod live 1's stat-trace, and the
    // sweep's range (the lowest and highest answers walked).
    const double sorak = 151;
    const std::vector<double> sweep = { -75.5, -50, -24.5, 0, 37.75, 75.5, 113.25, 151, 188.75, 226.5 };

    // ---- baseline: the bonus inactive, and every call not the lever's --------
    {
        M1m::Speed speed;
        bool same = true;
        int changed = 0;
        for (double a : sweep) {
            const Seen s = Run(speed, Call{ true, true, 64, true, a }, false);
            if (s.answer != a) same = false;
            if (s.changed) ++changed;
        }
        check("baseline/inactive_answer_untouched", same && changed == 0);
        check("baseline/inactive_nothing_counted", speed.Stats().halved == 0 && speed.Stats().notNumber == 0,
              "halved=" + std::to_string(speed.Stats().halved) + " notNumber=" + std::to_string(speed.Stats().notNumber));
    }
    {
        // Another self: the mercenary, an enemy, a projectile. Not asked about
        // the bonus at all, answer untouched, nothing counted, even while the
        // bonus is active.
        M1m::Speed speed;
        const Seen s = Run(speed, Call{ false, true, 64, true, sorak }, true);
        check("baseline/other_self_untouched", s.answer == sorak && !s.changed && s.bonusAsks == 0
              && speed.Stats().halved == 0, "answer=" + Num(s.answer));
    }
    {
        // Every other stat id of the local player's: projectile speed (75,
        // skill sliders' lever), its neighbours, 0 and a negative id.
        M1m::Speed speed;
        bool same = true;
        int asks = 0;
        for (double id : { 75.0, 63.0, 65.0, 0.0, -1.0, 64.5, 554.0 }) {
            const Seen s = Run(speed, Call{ true, true, id, true, sorak }, true);
            if (s.answer != sorak || s.changed) same = false;
            asks += s.bonusAsks;
        }
        check("baseline/other_stat_ids_untouched", same && asks == 0 && speed.Stats().halved == 0);
    }
    {
        // No readable stat id: never the lever's.
        M1m::Speed speed;
        const Seen s = Run(speed, Call{ true, false, 64, true, sorak }, true);
        check("baseline/no_stat_id_untouched", s.answer == sorak && s.bonusAsks == 0 && speed.Stats().halved == 0);
    }
    {
        // Negative control for the scope test above: the same call with the
        // local player and id 64 is the lever's.
        check("baseline/scope_control", M1m::Speed::Wants(true, true, 64) && !M1m::Speed::Wants(false, true, 64)
              && !M1m::Speed::Wants(true, true, 75) && !M1m::Speed::Wants(true, false, 64));
    }

    // ---- target: the half step -----------------------------------------------
    {
        M1m::Speed speed;
        const Seen s = Run(speed, Call{ true, true, 64, true, sorak }, true);
        check("target/sorak_answered_minus_24_5", s.changed && s.answer == -24.5, "answer=" + Num(s.answer));
        check("target/sorak_step_halved", StepRatio(s.answer, sorak) == 0.5, "ratio=" + Num(StepRatio(s.answer, sorak)));
    }
    {
        M1m::Speed speed;
        bool half = true;
        std::string worst;
        for (double a : sweep) {
            const Seen s = Run(speed, Call{ true, true, 64, true, a }, true);
            const double r = StepRatio(s.answer, a);
            if (!s.changed || std::fabs(r - 0.5) > 1e-12) { half = false; worst += " " + Num(a) + "->" + Num(r); }
        }
        check("target/half_step_across_the_sweep", half, worst);
        check("target/each_call_counted", speed.Stats().halved == sweep.size(),
              "halved=" + std::to_string(speed.Stats().halved));
    }
    {
        // Halving the answer itself is the proportional reading, which Mod
        // live 1 measured at 0.785: the affine model is why the lever
        // subtracts the offset.
        check("target/not_the_proportional_half", M1m::HalfWalkAnswer(sorak) != sorak / 2
              && StepRatio(sorak / 2, sorak) > 0.75);
    }
    {
        // Each call is answered from its own original: calls in the same
        // frame, repeated, never compound.
        M1m::Speed speed;
        bool same = true;
        for (int i = 0; i < 5; ++i) {
            const Seen s = Run(speed, Call{ true, true, 64, true, sorak }, true);
            if (s.answer != -24.5) same = false;
        }
        check("target/repeated_calls_never_compound", same && speed.Stats().halved == 5);
    }
    {
        // No cap was reached (kStepCapped false): a high answer is halved on
        // the same line, not stopped.
        check("target/no_cap", !M1m::kStepCapped && M1m::HalfWalkAnswer(1000) == 400);
    }
    {
        // An answer that is not a finite number reaches the caller as the
        // game gave it, never guessed at, and counted apart from the halved.
        M1m::Speed speed;
        const Seen s = Run(speed, Call{ true, true, 64, false, sorak }, true);
        double nan = std::numeric_limits<double>::quiet_NaN();
        const bool nanKept = !speed.Answer(true, true, nan) && std::isnan(nan);
        double inf = std::numeric_limits<double>::infinity();
        const bool infKept = !speed.Answer(true, true, inf) && std::isinf(inf);
        check("target/not_a_number_untouched_and_counted", !s.changed && s.answer == sorak && nanKept && infKept
              && speed.Stats().notNumber == 3 && speed.Stats().halved == 0,
              "notNumber=" + std::to_string(speed.Stats().notNumber));
    }
    {
        // Bonus off again: vanilla at once, the count kept.
        M1m::Speed speed;
        Run(speed, Call{ true, true, 64, true, sorak }, true);
        const Seen off = Run(speed, Call{ true, true, 64, true, sorak }, false);
        check("target/off_again_is_vanilla", off.answer == sorak && !off.changed && speed.Stats().halved == 1);
    }

    // ---- the routes ------------------------------------------------------------
    check("route/speed_is_stat_model", M1m::kSpeedRoute == "stat-model" && M1m::kSpeedLever);
    check("route/penalty_is_none_and_unavailable", M1m::kPenaltyRoute == "none" && !M1m::kPenaltyLever);
    check("route/constants", M1m::kMovementSpeedStatId == 64 && M1m::kStepOffset == 200 && M1m::kWalkRatio == 0.5);

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
