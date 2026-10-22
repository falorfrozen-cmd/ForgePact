// Behavioral regression harness for the Mark 1 automation's mana cost
// (Mark1Cost.hpp).
//
// The Python runner injects the REAL ForgePact Mark1Cost code below. Nothing
// of the game is touched: each scenario hands the core what the set bonus's
// consumer in ModuleMain.cpp reads off one ReturnSpecificStat call (whether
// its self is the local player, its stat-id argument, whether the answer is a
// number, the answer itself), the proc route's column count and the id-37
// total it read by name, and reads back the answer the caller would get and
// the core's counts.
//
// Run with the argument `cases`, it instead reads one case a line from stdin
// and prints the core's answer for each, so the runner can hold the core to
// the hub's shared cases file (hs-game-sdk/curated/mark1_automation_cases.json):
//   drain <columns> <percent> <max_mana> <unit> <original>
//     -> drain <drain per second> <answer>
//   maxmana <columns> <cut_pct> <original>
//     -> maxmana <answer>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

// PRODUCTION_MARK1COST

using namespace ForgePact;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static std::string Num(double v) { std::ostringstream o; o << v; return o.str(); }
static bool Near(double a, double b) { return std::fabs(a - b) <= 1e-9 * (1 + std::fabs(b)); }

// One ReturnSpecificStat call through the core, the way the consumer runs it:
// the scope is asked first, the cost only for a call in scope.
struct Call {
    bool local = true;
    bool haveStatId = true;
    double statId = 42;
    bool isNumber = true;
    double answer = 30;
};
struct Seen {
    double answer = 0;   // what the caller reads
    bool changed = false;
    int asks = 0;        // how often the cost was asked
};
static Seen Run(Mark1Cost& cost, const Call& c, int procColumns, double maxMana) {
    Seen s;
    s.answer = c.answer;
    if (!Mark1Cost::Wants(c.local, c.haveStatId, c.statId)) return s;
    ++s.asks;
    s.changed = cost.Answer(c.statId, c.isNumber, s.answer, procColumns, maxMana);
    return s;
}

static int RunCases() {
    std::cout << std::setprecision(17);
    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string kind;
        if (!(in >> kind)) continue;
        if (kind == "drain") {
            int columns = 0;
            double percent = 0, maxMana = 0, unit = 0, original = 0;
            in >> columns >> percent >> maxMana >> unit >> original;
            std::cout << "drain " << Mark1DrainPerSecond(columns, percent, maxMana, unit) << " "
                      << Mark1DrainAnswer(original, columns, maxMana, percent, unit) << std::endl;
        } else if (kind == "maxmana") {
            int columns = 0;
            double cutPct = 0, original = 0;
            in >> columns >> cutPct >> original;
            std::cout << "maxmana " << Mark1MaxManaAnswer(original, columns, cutPct) << std::endl;
        } else {
            std::cout << "unknown " << kind << std::endl;
            return 2;
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "cases") return RunCases();

    // The model's assumed inputs (mark1_automation_cases.json): a 1500 pool
    // and a 2% baseline replenish, 30 a second.
    const double maxMana = 1500;
    const double replenish = 30;

    // ---- baseline: no proc column, or the cost off ---------------------------
    {
        // Press route, no proc column, cost off: ids 37 and 42 answered with
        // their originals and nothing counted.
        Mark1Cost cost;
        cost.SetCost(Mark1ManaCost::Off);
        const Seen r = Run(cost, Call{ true, true, 42, true, replenish }, 0, maxMana);
        const Seen m = Run(cost, Call{ true, true, 37, true, maxMana }, 0, maxMana);
        check("baseline/off_no_column_answers_originals", r.answer == replenish && m.answer == maxMana
              && !r.changed && !m.changed && cost.Stats().drained == 0,
              "42=" + Num(r.answer) + " 37=" + Num(m.answer));
    }
    {
        // The shipped cost (Drain) with no proc column: the press route's casts
        // pay the game's own mana, so nothing is answered.
        Mark1Cost cost;
        bool same = true;
        for (double id : { 37.0, 42.0 }) {
            const Seen s = Run(cost, Call{ true, true, id, true, id == 42 ? replenish : maxMana }, 0, maxMana);
            if (s.changed || s.answer != (id == 42 ? replenish : maxMana)) same = false;
        }
        Mark1Cost cut;
        cut.SetCost(Mark1ManaCost::MaxMana);
        const Seen m = Run(cut, Call{ true, true, 37, true, maxMana }, 0, maxMana);
        check("baseline/no_proc_column_answers_originals", same && !m.changed && m.answer == maxMana
              && cost.Stats().drained == 0 && cut.Stats().drained == 0);
    }
    {
        // The cost off with four proc columns: still nothing.
        Mark1Cost cost;
        cost.SetCost(Mark1ManaCost::Off);
        const Seen r = Run(cost, Call{ true, true, 42, true, replenish }, 4, maxMana);
        const Seen m = Run(cost, Call{ true, true, 37, true, maxMana }, 4, maxMana);
        check("baseline/off_with_columns_answers_originals", r.answer == replenish && m.answer == maxMana
              && cost.Stats().drained == 0);
    }
    {
        // Another self: the mercenary, an enemy. Never asked, even at four.
        Mark1Cost cost;
        const Seen s = Run(cost, Call{ false, true, 42, true, replenish }, 4, maxMana);
        check("baseline/other_self_untouched", s.answer == replenish && s.asks == 0 && cost.Stats().drained == 0);
    }
    {
        // Every other id of the local player's, mana's neighbours included
        // (43 the replenish's share of the pool, 46 per kill, 147 per hit, 340
        // mana cost): never the cost's.
        Mark1Cost cost;
        bool same = true;
        int asks = 0;
        for (double id : { 36.0, 38.0, 41.0, 43.0, 46.0, 147.0, 340.0, 0.0, -1.0, 42.5 }) {
            const Seen s = Run(cost, Call{ true, true, id, true, 77 }, 4, maxMana);
            if (s.answer != 77 || s.changed) same = false;
            asks += s.asks;
        }
        // And a direct ask with another id, past the scope test, changes nothing.
        double a = 77;
        const bool direct = !cost.Answer(64, true, a, 4, maxMana) && a == 77;
        check("baseline/other_stat_ids_untouched", same && asks == 0 && direct && cost.Stats().drained == 0);
    }
    {
        // No readable stat id: never the cost's.
        Mark1Cost cost;
        const Seen s = Run(cost, Call{ true, false, 42, true, replenish }, 4, maxMana);
        check("baseline/no_stat_id_untouched", s.answer == replenish && s.asks == 0);
    }
    {
        // Negative control for the scope tests above.
        check("baseline/scope_control", Mark1Cost::Wants(true, true, 42) && Mark1Cost::Wants(true, true, 37)
              && !Mark1Cost::Wants(false, true, 42) && !Mark1Cost::Wants(true, true, 43)
              && !Mark1Cost::Wants(true, false, 37));
    }
    {
        // Under Drain id 37 is left alone, under MaxMana id 42: one cost at a time.
        Mark1Cost drain;
        const Seen m = Run(drain, Call{ true, true, 37, true, maxMana }, 4, maxMana);
        Mark1Cost cut;
        cut.SetCost(Mark1ManaCost::MaxMana);
        const Seen r = Run(cut, Call{ true, true, 42, true, replenish }, 4, maxMana);
        check("baseline/one_cost_at_a_time", !m.changed && m.answer == maxMana && !r.changed && r.answer == replenish);
    }

    // ---- target: the drain ----------------------------------------------------
    {
        // 1% of 1500 a second per proc column, off the 30 baseline.
        bool ok = true;
        std::string seen;
        const double expected[] = { 30, 15, 0, -15, -30 };
        uint64_t changedCount = 0;
        for (int n = 0; n <= 4; ++n) {
            Mark1Cost cost;
            const Seen s = Run(cost, Call{ true, true, 42, true, replenish }, n, maxMana);
            if (!Near(s.answer, expected[n]) || s.changed != (n > 0)) ok = false;
            changedCount += cost.Stats().drained;
            seen += " " + std::to_string(n) + "->" + Num(s.answer);
        }
        check("target/drain_scales_with_proc_columns", ok && changedCount == 4, seen);
    }
    {
        // The drain is per column: four columns take four times one.
        const double one = Mark1DrainPerSecond(1, kMark1DrainPctPerSecond, maxMana);
        const double four = Mark1DrainPerSecond(4, kMark1DrainPctPerSecond, maxMana);
        check("target/drain_linear_in_columns", one == 15 && four == 60 && Mark1DrainPerSecond(0, 1, maxMana) == 0,
              "one=" + Num(one) + " four=" + Num(four));
    }
    {
        // The floor: a drain past the pool stops at minus the maximum.
        Mark1Cost cost;
        const bool set = cost.SetDrainPct(10);
        const Seen s = Run(cost, Call{ true, true, 42, true, 0 }, 4, 1000);
        const double big = Mark1DrainAnswer(50, 4, 100000, 30, 1);
        const double exact = Mark1DrainAnswer(0, 4, 1000, 25, 1);
        check("target/drain_never_below_minus_max_mana", set && Near(s.answer, -400) && big == -100000
              && exact == -1000 && Mark1DrainAnswer(-5000, 1, 1000, 1, 1) == -1000,
              "s=" + Num(s.answer) + " big=" + Num(big) + " exact=" + Num(exact));
    }
    {
        // No maximum read yet (0), or an unreadable one: nothing to drain from.
        Mark1Cost cost;
        const Seen zero = Run(cost, Call{ true, true, 42, true, replenish }, 4, 0);
        const Seen nan = Run(cost, Call{ true, true, 42, true, replenish }, 4, std::numeric_limits<double>::quiet_NaN());
        const Seen neg = Run(cost, Call{ true, true, 42, true, replenish }, 4, -1500);
        check("target/drain_needs_a_maximum", zero.answer == replenish && nan.answer == replenish
              && neg.answer == replenish && cost.Stats().drained == 0);
    }
    {
        // Repeated calls in one frame each answer from their own original.
        Mark1Cost cost;
        bool same = true;
        for (int i = 0; i < 5; ++i) {
            const Seen s = Run(cost, Call{ true, true, 42, true, replenish }, 4, maxMana);
            if (!Near(s.answer, -30)) same = false;
        }
        check("target/repeated_calls_never_compound", same && cost.Stats().drained == 5,
              "drained=" + std::to_string(cost.Stats().drained));
    }
    {
        // A column count past the bar counts as four.
        Mark1Cost cost;
        const Seen s = Run(cost, Call{ true, true, 42, true, replenish }, 9, maxMana);
        check("target/columns_capped_at_four", Near(s.answer, -30), "answer=" + Num(s.answer));
    }
    {
        // A non-number reaches the caller as the game gave it, counted apart.
        Mark1Cost cost;
        const Seen s = Run(cost, Call{ true, true, 42, false, replenish }, 4, maxMana);
        double inf = std::numeric_limits<double>::infinity();
        const bool infKept = !cost.Answer(42, true, inf, 4, maxMana) && std::isinf(inf);
        check("target/not_a_number_untouched_and_counted", !s.changed && s.answer == replenish && infKept
              && cost.Stats().notNumber == 2 && cost.Stats().drained == 0,
              "notNumber=" + std::to_string(cost.Stats().notNumber));
    }

    // ---- target: the maximum-mana cut ----------------------------------------
    {
        Mark1Cost cost;
        cost.SetCost(Mark1ManaCost::MaxMana);
        bool ok = true;
        std::string seen;
        const double expected[] = { 1500, 1200, 900, 600, 300 };
        for (int n = 0; n <= 4; ++n) {
            const Seen s = Run(cost, Call{ true, true, 37, true, maxMana }, n, maxMana);
            if (s.answer != expected[n]) ok = false;
            seen += " " + std::to_string(n) + "->" + Num(s.answer);
        }
        check("target/maxmana_cuts_twenty_percent_each", ok && cost.Stats().drained == 4, seen);
        check("target/maxmana_never_below_zero", Mark1MaxManaAnswer(1500, 4, 30) == 0
              && Mark1MaxManaAnswer(1500, 0, 30) == 1500);
    }

    // ---- the research overrides ------------------------------------------------
    {
        Mark1Cost cost;
        const bool shipped = cost.Cost() == kMark1ManaCost && cost.DrainPct() == kMark1DrainPctPerSecond;
        const bool okHalf = cost.SetDrainPct(0.75);
        const Seen s = Run(cost, Call{ true, true, 42, true, replenish }, 4, maxMana);
        const bool refused = !cost.SetDrainPct(10.5) && !cost.SetDrainPct(-0.1)
            && !cost.SetDrainPct(std::numeric_limits<double>::quiet_NaN()) && cost.DrainPct() == 0.75;
        const bool zero = cost.SetDrainPct(0);
        const Seen z = Run(cost, Call{ true, true, 42, true, replenish }, 4, maxMana);
        cost.SetCost(Mark1ManaCost::MaxMana);
        const bool switched = cost.Cost() == Mark1ManaCost::MaxMana;
        cost.ResetOverrides();
        const bool reset = cost.Cost() == kMark1ManaCost && cost.DrainPct() == kMark1DrainPctPerSecond;
        check("override/percent_and_cost", shipped && okHalf && Near(s.answer, -15) && refused && zero
              && z.answer == replenish && !z.changed && switched && reset,
              "half=" + Num(s.answer) + " zero=" + Num(z.answer));
    }
    {
        Mark1ManaCost c = Mark1ManaCost::Off;
        const bool parsed = Mark1ParseManaCost("drain", c) && c == Mark1ManaCost::Drain
            && Mark1ParseManaCost("maxmana", c) && c == Mark1ManaCost::MaxMana
            && Mark1ParseManaCost("off", c) && c == Mark1ManaCost::Off;
        c = Mark1ManaCost::Drain;
        const bool refused = !Mark1ParseManaCost("drains", c) && !Mark1ParseManaCost("", c)
            && !Mark1ParseManaCost("max", c) && !Mark1ParseManaCost(nullptr, c) && c == Mark1ManaCost::Drain;
        const bool named = std::string(Mark1ManaCostName(Mark1ManaCost::Drain)) == "drain"
            && std::string(Mark1ManaCostName(Mark1ManaCost::MaxMana)) == "maxmana"
            && std::string(Mark1ManaCostName(Mark1ManaCost::Off)) == "off";
        check("override/words", parsed && refused && named);
    }

    // ---- the shipped values -----------------------------------------------------
    check("route/constants", kMark1ManaCost == Mark1ManaCost::Drain && kMark1DrainPctPerSecond == 1.0
          && kMark1MaxManaCutPct == 20.0 && kMark1ReplenishUnit == 1.0 && kMark1MaxManaStatId == 37
          && kMark1ReplenishStatId == 42 && kMark1DrainPctMax == 10.0);

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
