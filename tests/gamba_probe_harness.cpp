// Behavioral regression harness for gambaprobe's decision core
// (GambaProbe.hpp, ForgePact #134, research build only).
//
// The Python runner injects the REAL ForgePact::GambaProbe header below. No
// game is touched: a call's self is a plain object index, as the adapter
// hands it over, and the machine is the object index the adapter resolved by
// name (Slot_Machine_01_obj's, 4644 in hs-game-sdk; any number works here).
//
// Baseline: with the probe idle and the lever off, every RNG builtin call runs
// the game's own function, no self is read, and no counter but `calls` moves;
// armed with the lever off, the same calls are classified but still never
// answered. Target: with `rng <builtin> <value> <count> [args <text>]`, only a
// call of the target builtin whose self is a machine (and whose argument text
// matches, when the lever names one) is answered, `count` times and then the
// lever is off; another object's call, another self inside a machine's event,
// and a machine call of another builtin or with other arguments are untouched,
// the last two counted as passed; a lever its target never reached is named
// INERT; moving keys cannot spend a row's trace budget within a spin; the
// status text reads back every counter.
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// PRODUCTION_GAMBAPROBE

using namespace ForgePact::GambaProbe;

static constexpr int kMachine = 4644;
static constexpr int kPlayer = 3553;
static constexpr int kEnemy = 5004;
static constexpr int kScripts = 24;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "")
{
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static Probe make()
{
    Probe p;
    p.SetScriptRowCount(kScripts);
    p.SetMachineObject(kMachine);
    return p;
}

static long g_SelfReads = 0;
static long g_ArgsReads = 0;
static RngDecision rng(Probe& p, Builtin b, int self, bool inEvent = false, int argc = 1, const std::string& args = "")
{
    return p.DecideRng(b, [self]() { ++g_SelfReads; return self; }, inEvent, argc,
                       [&args]() { ++g_ArgsReads; return args; });
}
static Seen observe(Probe& p, int row, int self, bool inEvent = false)
{
    return p.Observe(row, [self]() { ++g_SelfReads; return self; }, inEvent);
}

static const Builtin kRngRows[] = { Builtin::Irandom, Builtin::IrandomRange, Builtin::Random, Builtin::RandomRange, Builtin::Choose };
static const Builtin kOtherRows[] = { Builtin::InstanceDestroy, Builtin::InstanceCreateDepth, Builtin::InstanceCreateLayer };

static std::string name(Builtin b) { return std::string(kBuiltins[static_cast<int>(b)].name); }
static bool onlyCalls(const Counters& c, uint64_t calls)
{
    return c.calls == calls && c.machineSelf == 0 && c.inEvent == 0 && c.otherSelf == 0 && c.logged == 0
        && c.answered == 0 && c.outOfRange == 0;
}
static bool contains(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

int main()
{
    // ---- the tables ----------------------------------------------------------
    {
        check("table/events", kEventCount == 5 && kEvents[0].event == "Create_0" && kEvents[1].event == "Alarm_0"
            && kEvents[2].event == "Alarm_9" && kEvents[3].event == "Step_0" && kEvents[4].event == "CleanUp_0"
            && kEvents[0].key == "create" && kEvents[2].key == "alarm9" && kEvents[3].key == "step");
        bool ok = kBuiltinCount == 8;
        std::string bad;
        for (Builtin b : kRngRows) if (kBuiltins[static_cast<int>(b)].kind == AnswerKind::NotRng) { ok = false; bad += name(b) + " "; }
        for (Builtin b : kOtherRows) if (kBuiltins[static_cast<int>(b)].kind != AnswerKind::NotRng) { ok = false; bad += name(b) + " "; }
        ok = ok && kBuiltins[static_cast<int>(Builtin::Choose)].kind == AnswerKind::ArgumentIndex
            && kBuiltins[static_cast<int>(Builtin::Irandom)].kind == AnswerKind::Value;
        check("table/rng_rows_and_kinds", ok, bad);
        check("table/row_layout", EventRowOf(Event::Create) == 0 && BuiltinRowOf(Builtin::Irandom) == kEventCount
            && ScriptRowOf(0) == kEventCount + kBuiltinCount && make().RowCount() == kEventCount + kBuiltinCount + kScripts);
        check("table/route_names", RouteName(Route::Detoured) == "detoured" && RouteName(Route::TableOnly) == "table-only"
            && RouteName(Route::Shared) == "shared" && RouteName(Route::Missing) == "missing"
            && RouteName(Route::DetouredUnder) == "detoured-under");
        check("table/trace_budget_is_named", kTraceLinesPerRow == 512 && kTraceLinesPerKey == 8 && kTraceKeysPerRow == 64
            && kTraceLinesPerRow == kTraceLinesPerKey * kTraceKeysPerRow && kRngMinCount == 1 && kRngMaxCount == 50);
        Builtin b = Builtin::InstanceDestroy;
        Builtin none = Builtin::Irandom;
        check("table/builtin_by_name", BuiltinByName("irandom", b) && b == Builtin::Irandom && BuiltinByName("choose", b)
            && b == Builtin::Choose && BuiltinByName("random_range", b) && b == Builtin::RandomRange
            && !BuiltinByName("IRANDOM", none) && !BuiltinByName("gamba", none) && none == Builtin::Irandom);
        check("table/args_key_trims_and_folds_whitespace", ArgsKey("  a0=100 \t  a1=5 ") == "a0=100 a1=5"
            && ArgsKey(" a0=1") == "a0=1" && ArgsKey("") == "" && ArgsKey("   ") == "", ArgsKey("  a0=100 \t  a1=5 "));
    }

    // ---- baseline: the probe idle, the lever off ------------------------------
    {
        Probe p = make();
        g_SelfReads = 0;
        bool real = true;
        std::string bad;
        for (Builtin b : kRngRows)
            for (int self : { kMachine, kPlayer, kEnemy })
                for (bool inEvent : { false, true }) {
                    const RngDecision d = rng(p, b, self, inEvent, 3);
                    if (d.answer || d.seen != Seen::Idle) { real = false; bad += name(b) + " "; }
                }
        check("baseline/idle_every_rng_answer_is_the_real_one", real, bad);
        bool only = true;
        for (Builtin b : kRngRows) if (!onlyCalls(p.RowCounters(BuiltinRowOf(b)), 6)) { only = false; bad += name(b) + " "; }
        for (Builtin b : kOtherRows) if (!onlyCalls(p.RowCounters(BuiltinRowOf(b)), 0)) only = false;
        check("baseline/idle_no_counter_but_calls_moves", only, bad);
        check("baseline/idle_reads_no_self", g_SelfReads == 0, "reads=" + std::to_string(g_SelfReads));
        // Every other row the same: an event, a script.
        observe(p, EventRowOf(Event::Step), kMachine);
        observe(p, ScriptRowOf(3), kMachine);
        check("baseline/idle_events_and_scripts_only_count", onlyCalls(p.RowCounters(EventRowOf(Event::Step)), 1)
            && onlyCalls(p.RowCounters(ScriptRowOf(3)), 1) && g_SelfReads == 0);
        check("baseline/idle_lever_and_status_off", !p.RngOn() && !p.Inert() && !p.Active()
            && p.StatusLine().rfind("gambaprobe: off", 0) == 0 && p.RngAnswered() == 0);
    }
    {
        // Armed, lever off: classified, never answered.
        Probe p = make();
        p.SetArmed(true);
        bool real = true;
        for (Builtin b : kRngRows) if (rng(p, b, kMachine, false, 3).answer) real = false;
        const Counters& c = p.RowCounters(BuiltinRowOf(Builtin::Irandom));
        check("baseline/armed_lever_off_answers_nothing", real && c.machineSelf == 1 && c.answered == 0);
        rng(p, Builtin::Irandom, kEnemy);
        rng(p, Builtin::Irandom, kPlayer, true);
        check("baseline/armed_classifies_machine_in_event_other", c.calls == 3 && c.machineSelf == 1 && c.inEvent == 1
            && c.otherSelf == 1, p.RowText(BuiltinRowOf(Builtin::Irandom)));
        check("baseline/armed_status_on", p.StatusLine().rfind("gambaprobe: on", 0) == 0);
    }

    // ---- the machine-self predicate ------------------------------------------
    {
        Probe p = make();
        check("predicate/only_the_machines_own_index", p.IsMachine(kMachine) && !p.IsMachine(kPlayer)
            && !p.IsMachine(kMachine + 1) && !p.IsMachine(-1));
        p.SetMachineObject(-1);
        check("predicate/unresolved_machine_is_nothing", !p.IsMachine(-1) && !p.IsMachine(kMachine));
        p.SetRng(Builtin::Irandom, 98, 5);
        const RngDecision d = rng(p, Builtin::Irandom, kMachine);
        check("predicate/unresolved_machine_answers_nothing", !d.answer && d.seen == Seen::Other);
    }

    // ---- target: the lever ------------------------------------------------------
    {
        Probe p = make();
        check("target/set_rng", p.SetRng(Builtin::Irandom, 98, 2) && p.RngOn() && p.Active() && p.RngRemaining() == 2
            && p.RngTarget() == Builtin::Irandom && p.RngArgs().empty());
        g_SelfReads = 0;
        const RngDecision other = rng(p, Builtin::Irandom, kEnemy);
        const RngDecision player = rng(p, Builtin::IrandomRange, kPlayer);
        const RngDecision inEvent = rng(p, Builtin::Random, kPlayer, true);
        check("target/another_objects_call_is_untouched", !other.answer && !player.answer && other.seen == Seen::Other
            && p.RngRemaining() == 2 && p.RowCounters(BuiltinRowOf(Builtin::Irandom)).otherSelf == 1);
        check("target/another_self_inside_a_machine_event_is_untouched", !inEvent.answer && inEvent.seen == Seen::InEvent
            && p.RowCounters(BuiltinRowOf(Builtin::Random)).inEvent == 1);
        check("target/levered_reads_the_self", g_SelfReads == 3);
        check("target/other_selves_are_not_counted_as_passed", p.RngPassedBuiltin() == 0 && p.RngPassedArgs() == 0);
        const RngDecision a = rng(p, Builtin::Irandom, kMachine);
        check("target/machine_self_answered_with_the_value", a.answer && a.value == 98.0 && a.seen == Seen::Machine
            && p.RngRemaining() == 1 && p.RngOn());
        // A machine-self call of another RNG builtin is not the lever's target.
        const RngDecision wrong = rng(p, Builtin::RandomRange, kMachine);
        check("target/another_builtins_machine_call_passes_untouched", !wrong.answer && wrong.seen == Seen::Machine
            && p.RngRemaining() == 1 && p.RngOn() && p.RngPassedBuiltin() == 1
            && p.RowCounters(BuiltinRowOf(Builtin::RandomRange)).passed == 1
            && p.RowCounters(BuiltinRowOf(Builtin::RandomRange)).answered == 0);
        const RngDecision b = rng(p, Builtin::Irandom, kMachine);
        check("target/count_calls_then_off", b.answer && b.value == 98.0 && !p.RngOn() && p.RngAnswered() == 2
            && p.RngRemaining() == 0);
        check("target/finished_is_read_once", p.TakeRngFinished() && !p.TakeRngFinished());
        const RngDecision c = rng(p, Builtin::Irandom, kMachine);
        check("target/after_count_the_real_one_again", !c.answer && !p.Active());
        check("target/answered_counted_per_row", p.RowCounters(BuiltinRowOf(Builtin::Irandom)).answered == 2
            && p.RowCounters(BuiltinRowOf(Builtin::RandomRange)).answered == 0);
    }
    {
        Probe p = make();
        p.SetRng(Builtin::Irandom, 3, 5);
        bool none = true;
        for (Builtin b : kOtherRows) if (rng(p, b, kMachine).answer) none = false;
        check("target/instance_rows_never_answered", none && p.RngRemaining() == 5);
        bool refused = true;
        for (Builtin b : kOtherRows) if (p.SetRng(b, 3, 1)) refused = false;
        check("target/an_instance_row_is_no_target", refused && p.RngTarget() == Builtin::Irandom && p.RngRemaining() == 5);
    }
    {
        // choose: the value names one of the call's own arguments.
        Probe p = make();
        p.SetRng(Builtin::Choose, 1, 1);
        const RngDecision in = rng(p, Builtin::Choose, kMachine, false, 3);
        check("target/choose_answers_an_argument_index", in.answer && in.value == 1.0 && !p.RngOn());
        p.SetRng(Builtin::Choose, 5, 1);
        const RngDecision out = rng(p, Builtin::Choose, kMachine, false, 3);
        check("target/choose_out_of_range_runs_the_original", !out.answer && p.RngOn() && p.RngRemaining() == 1
            && p.RngOutOfRange() == 1 && p.RowCounters(BuiltinRowOf(Builtin::Choose)).outOfRange == 1);
        p.SetRng(Builtin::Choose, 1.5, 1);
        const RngDecision frac = rng(p, Builtin::Choose, kMachine, false, 3);
        p.SetRng(Builtin::Choose, -1, 1);
        const RngDecision neg = rng(p, Builtin::Choose, kMachine, false, 3);
        check("target/choose_fraction_or_negative_runs_the_original", !frac.answer && !neg.answer);
        // irandom takes any value: the value is the operator's to choose.
        p.SetRng(Builtin::Irandom, -7.25, 1);
        const RngDecision any = rng(p, Builtin::Irandom, kMachine);
        check("target/value_rows_take_any_finite_value", any.answer && any.value == -7.25);
    }
    {
        Probe p = make();
        p.SetRng(Builtin::Irandom, 4, 3);
        const bool refused = !p.SetRng(Builtin::Irandom, 4, 0) && !p.SetRng(Builtin::Irandom, 4, kRngMaxCount + 1)
            && !p.SetRng(Builtin::Irandom, std::nan(""), 1) && !p.SetRng(Builtin::Irandom, INFINITY, 1)
            && !p.SetRng(Builtin::InstanceCreateDepth, 4, 1) && !p.SetRng(static_cast<Builtin>(kBuiltinCount), 4, 1);
        check("target/out_of_range_set_refused_nothing_changed", refused && p.RngOn() && p.RngValue() == 4.0
            && p.RngRemaining() == 3 && p.RngTarget() == Builtin::Irandom);
        rng(p, Builtin::Irandom, kMachine);
        rng(p, Builtin::Random, kMachine);
        p.SetRng(Builtin::Random, 9, 1, "a0=1");
        check("target/set_rng_starts_clean", p.RngAnswered() == 0 && p.RngRemaining() == 1 && p.RngValue() == 9.0
            && p.RngPassedBuiltin() == 0 && p.RngPassedArgs() == 0 && p.RngTarget() == Builtin::Random && p.RngArgs() == "a0=1");
    }

    // ---- aim: the lever answers the call step 4 identified, not its neighbours --------
    {
        // An idle per-frame call with other arguments, and a reel roll of
        // another builtin, run before the prize roll: neither takes the answer.
        Probe p = make();
        p.SetArmed(true);
        check("aim/set_with_args", p.SetRng(Builtin::Irandom, 98, 1, "  a0=100   a1=5 ") && p.RngArgs() == "a0=100 a1=5"
            && contains(p.RngLine(), "target=irandom args=\"a0=100 a1=5\""), p.RngLine());
        g_ArgsReads = 0;
        bool none = true;
        for (int frame = 0; frame < 600; ++frame) if (rng(p, Builtin::Irandom, kMachine, false, 1, " a0=3").answer) none = false;
        const RngDecision reel = rng(p, Builtin::IrandomRange, kMachine, false, 2, " a0=100 a1=5");
        check("aim/non_matching_machine_calls_are_untouched", none && !reel.answer && p.RngOn() && p.RngRemaining() == 1
            && p.RngPassedArgs() == 600 && p.RngPassedBuiltin() == 1 && p.Inert()
            && p.RowCounters(BuiltinRowOf(Builtin::Irandom)).passed == 600
            && p.RowCounters(BuiltinRowOf(Builtin::Irandom)).answered == 0, p.RngLine());
        check("aim/inert_line_counts_what_passed", contains(p.RngLine(), "INERT")
            && contains(p.RngLine(), "passed=601 (other builtin 1, other args 600)")
            && contains(p.RngLine(), "601 other machine-self RNG call(s) passed through untouched"), p.RngLine());
        // Another self making the very call is still not the machine's.
        const RngDecision enemy = rng(p, Builtin::Irandom, kEnemy, false, 2, " a0=100 a1=5");
        check("aim/args_text_is_read_only_for_the_targets_machine_calls", !enemy.answer && g_ArgsReads == 600);
        const RngDecision hit = rng(p, Builtin::Irandom, kMachine, false, 2, " a0=100  a1=5");
        check("aim/the_matching_call_is_answered", hit.answer && hit.value == 98.0 && !p.RngOn() && p.RngAnswered() == 1
            && g_ArgsReads == 601 && !p.Inert());
    }
    {
        // Without `args` the target builtin's every machine-self call matches,
        // and the argument text is never read.
        Probe p = make();
        p.SetRng(Builtin::Random, 0.5, 2);
        g_ArgsReads = 0;
        const RngDecision x = rng(p, Builtin::Random, kMachine, false, 1, " a0=7");
        const RngDecision y = rng(p, Builtin::Random, kMachine, false, 1, " a0=8");
        check("aim/no_args_filter_matches_any_arguments", x.answer && y.answer && g_ArgsReads == 0 && !p.RngOn());
    }

    // ---- inert ----------------------------------------------------------------------
    {
        Probe p = make();
        p.SetRng(Builtin::Irandom, 98, 1);
        rng(p, Builtin::Irandom, kEnemy);
        rng(p, Builtin::Irandom, kPlayer, true);
        check("inert/armed_lever_no_machine_call_reached_is_named", p.Inert() && contains(p.RngLine(), "INERT")
            && contains(p.RngLine(), "gambaprobe: rng answered 0 of 1"), p.RngLine());
        rng(p, Builtin::Irandom, kMachine);
        check("inert/an_answer_ends_it", !p.Inert() && !contains(p.RngLine(), "INERT")
            && contains(p.RngLine(), "gambaprobe: rng answered 1 of 1"), p.RngLine());
        Probe q = make();
        q.SetRng(Builtin::Irandom, 98, 1);
        q.RngOff();
        Probe r = make();
        check("inert/lever_off_or_never_set_is_not", !q.Inert() && !r.Inert() && !contains(r.RngLine(), "INERT"));
    }

    // ---- the trace budget -----------------------------------------------------------
    {
        Probe p = make();
        const int row = ScriptRowOf(0);
        int taken = 0;
        for (uint64_t i = 0; i < 1000; ++i) if (p.TakeTraceLine(row, i, i)) ++taken;   // a new key each line
        check("trace/at_most_the_budget_per_row", taken == kTraceLinesPerRow
            && p.RowCounters(row).logged == static_cast<uint64_t>(kTraceLinesPerRow) && p.RowCounters(row).keyCapped == 0);
        check("trace/budget_is_per_row", p.TakeTraceLine(ScriptRowOf(1), 0, 1));
        // One call shape a machine repeats every frame with a moving result
        // (an idle irandom, a timer key): its key writes kTraceLinesPerKey
        // lines and the rest of the row's budget stays for the spin.
        const int irnd = BuiltinRowOf(Builtin::Irandom);
        int idle = 0;
        for (uint64_t frame = 0; frame < 600; ++frame) if (p.TakeTraceLine(irnd, 7, frame)) ++idle;
        const bool spin = p.TakeTraceLine(irnd, 8, 1);
        check("trace/one_key_cannot_spend_the_row", idle == kTraceLinesPerKey && spin
            && p.RowCounters(irnd).logged == static_cast<uint64_t>(kTraceLinesPerKey + 1)
            && p.RowCounters(irnd).keyCapped == static_cast<uint64_t>(600 - kTraceLinesPerKey) && !p.BudgetSpent(irnd),
              p.RowText(irnd));
        check("trace/budget_spent_is_named", p.BudgetSpent(row) && contains(p.RowText(row), "BUDGET SPENT")
            && !contains(p.RowText(ScriptRowOf(1)), "BUDGET SPENT") && p.SpentRows() == 1
            && contains(p.StatusLine(), " spent-rows=1 "), p.RowText(row));
        const int gpv = ScriptRowOf(2);
        const bool first = p.TakeTraceLine(gpv, 17, 100);
        const bool repeat = p.TakeTraceLine(gpv, 17, 100);
        const bool otherKey = p.TakeTraceLine(gpv, 18, 100);
        const bool changed = p.TakeTraceLine(gpv, 17, 101);
        const bool back = p.TakeTraceLine(gpv, 17, 100);
        check("trace/a_repeat_for_the_same_key_spends_nothing", first && !repeat && otherKey && changed && back
            && p.RowCounters(gpv).logged == 4);
        const uint64_t calls = p.RowCounters(irnd).calls;
        p.ResetTrace();
        check("trace/hook_again_restores_the_budget", p.RowCounters(row).logged == 0 && p.TakeTraceLine(row, 0, 1)
            && p.TakeTraceLine(gpv, 17, 100) && !p.BudgetSpent(row) && p.SpentRows() == 0);
        check("trace/reset_restores_each_keys_budget", p.TakeTraceLine(irnd, 7, 1000)
            && p.RowCounters(irnd).calls == calls);
        check("trace/bad_row_refused", !p.TakeTraceLine(-1, 0, 0) && !p.TakeTraceLine(p.RowCount(), 0, 0));
    }
    {
        // A spin's reel animation: six protected-store keys change every
        // frame (each spends its kTraceLinesPerKey lines, then is key-capped
        // for the rest of the spin), then at the spin's end a seventh key - the
        // gold-spent or spin-count write - changes once. Its line is logged.
        Probe p = make();
        const int gpv = ScriptRowOf(1);
        int moving = 0;
        for (uint64_t frame = 0; frame < 120; ++frame)
            for (uint64_t key = 1; key <= 6; ++key) if (p.TakeTraceLine(gpv, key, frame)) ++moving;
        const bool spinEnd = p.TakeTraceLine(gpv, 7, 10000);
        check("trace/moving_keys_cannot_spend_the_row_within_a_spin", moving == 6 * kTraceLinesPerKey && spinEnd
            && !p.BudgetSpent(gpv) && p.RowCounters(gpv).keyCapped == static_cast<uint64_t>(6 * (120 - kTraceLinesPerKey))
            && !contains(p.RowText(gpv), "BUDGET SPENT"), p.RowText(gpv));
        // Every key of a full row writes all its lines; only a key past
        // kTraceKeysPerRow finds the row spent.
        Probe q = make();
        int all = 0;
        for (uint64_t key = 0; key < static_cast<uint64_t>(kTraceKeysPerRow); ++key)
            for (uint64_t line = 0; line < static_cast<uint64_t>(kTraceLinesPerKey); ++line)
                if (q.TakeTraceLine(gpv, key, line)) ++all;
        check("trace/every_key_of_a_full_row_writes_its_lines", all == kTraceLinesPerRow && q.BudgetSpent(gpv)
            && !q.TakeTraceLine(gpv, static_cast<uint64_t>(kTraceKeysPerRow), 0), q.RowText(gpv));
    }
    {
        // Two calls of one shape with one result in a window - a reel roll,
        // then the prize roll - write one line. The second must not vanish:
        // the row counts it, status names its key, and the key's next line
        // carries it. A call with another key and the same result is no
        // repeat (the negative control).
        Probe p = make();
        const int irnd = BuiltinRowOf(Builtin::Irandom);
        const std::string shape = " a0=real:100.000000 a1=real:5.000000";
        const bool reel = p.TakeTraceLine(irnd, 41, 7, shape);
        const uint64_t reelCarried = p.TakenRepeats();
        const bool prize = p.TakeTraceLine(irnd, 41, 7, shape);
        check("trace/a_same_shape_repeat_is_one_line_and_counted", reel && reelCarried == 0 && !prize
            && p.RowCounters(irnd).logged == 1 && p.RowCounters(irnd).repeats == 1
            && contains(p.RowText(irnd), " repeats=1 ") && contains(p.StatusLine(), " repeats=1 "), p.RowText(irnd));
        check("trace/status_names_the_key_a_repeat_folded_into",
              contains(p.RowText(irnd), "unlogged-repeats: +1 \"a0=real:100.000000 a1=real:5.000000\""), p.RowText(irnd));
        const bool other = p.TakeTraceLine(irnd, 42, 7, " a0=real:3.000000");
        check("trace/another_key_with_the_same_result_is_no_repeat", other && p.TakenRepeats() == 0
            && p.RowCounters(irnd).repeats == 1 && p.RowCounters(irnd).logged == 2, p.RowText(irnd));
        const bool next = p.TakeTraceLine(irnd, 41, 9, shape);
        check("trace/the_keys_next_line_carries_its_repeats", next && p.TakenRepeats() == 1
            && !contains(p.RowText(irnd), "unlogged-repeats") && p.RowCounters(irnd).repeats == 1, p.RowText(irnd));
        const bool after = p.TakeTraceLine(irnd, 41, 10, shape);
        const bool refused = p.TakeTraceLine(irnd, 41, 10, shape);
        check("trace/carried_repeats_are_taken_once", after && p.TakenRepeats() == 0 && !refused
            && p.RowCounters(irnd).repeats == 2, p.RowText(irnd));
        p.ResetTrace();
        check("trace/a_new_window_starts_repeats_over", p.RowCounters(irnd).repeats == 0 && p.TakenRepeats() == 0
            && !contains(p.RowText(irnd), "unlogged-repeats") && p.TakeTraceLine(irnd, 41, 10, shape)
            && p.TakenRepeats() == 0, p.RowText(irnd));
    }

    // ---- the status text reads back every counter ---------------------------------
    {
        Probe p = make();
        p.SetArmed(true);
        observe(p, EventRowOf(Event::Create), kMachine);
        observe(p, EventRowOf(Event::Alarm9), kMachine);
        for (int i = 0; i < 30; ++i) observe(p, EventRowOf(Event::Step), kMachine);
        check("status/events_by_key", p.EventsText() == "create=1 alarm0=0 alarm9=1 step=30 cleanup=0", p.EventsText());
        p.SetRng(Builtin::Choose, 2, 2);
        const int row = BuiltinRowOf(Builtin::Choose);
        rng(p, Builtin::Choose, kMachine, false, 3);    // answered
        p.SetRng(Builtin::Choose, 9, 1);
        rng(p, Builtin::Choose, kMachine, false, 3);    // out of range
        rng(p, Builtin::Choose, kPlayer, true, 3);      // in event
        rng(p, Builtin::Choose, kEnemy, false, 3);      // other
        rng(p, Builtin::Irandom, kMachine);             // passed: not the lever's target
        p.TakeTraceLine(row, 0, 1);
        const std::string text = p.RowText(row);
        check("status/row_reads_every_counter", text == "calls=4 machine-self=2 in-event=1 other-self=1 logged=1/512"
            " key-capped=0 repeats=0 answered=1 out-of-range=1 passed=0", text);
        check("status/passed_is_counted_on_its_own_row", p.RowText(BuiltinRowOf(Builtin::Irandom))
            == "calls=1 machine-self=1 in-event=0 other-self=0 logged=0/512 key-capped=0 repeats=0 answered=0 out-of-range=0 passed=1",
              p.RowText(BuiltinRowOf(Builtin::Irandom)));
        check("status/non_rng_rows_omit_the_lever_counters",
              p.RowText(EventRowOf(Event::Step)) == "calls=30 machine-self=30 in-event=0 other-self=0 logged=0/512 key-capped=0 repeats=0");
        const std::string line = p.StatusLine();
        check("status/line_sums_every_counter", line == "gambaprobe: on machine-object=4644 create=1 alarm0=0 alarm9=1"
            " step=30 cleanup=0 | calls=37 machine-self=35 in-event=1 other-self=1 logged=1 key-capped=0 repeats=0 spent-rows=0"
            " answered=1 out-of-range=1 passed=1", line);
        check("status/rng_line", contains(p.RngLine(), "gambaprobe: rng answered 0 of 1 target=choose value=9 remaining=1"
            " out-of-range=1 passed=1 (other builtin 1, other args 0) lever=on"), p.RngLine());
        check("status/number_text", NumberText(98) == "98" && NumberText(-7.25) == "-7.25" && NumberText(0.5) == "0.5");
    }

    // ---- off --------------------------------------------------------------------------
    {
        Probe p = make();
        p.SetArmed(true);
        p.SetRng(Builtin::Irandom, 98, 3);
        rng(p, Builtin::Irandom, kMachine);
        p.Off();
        const RngDecision d = rng(p, Builtin::Irandom, kMachine);
        check("off/disarms_and_lever_off_counts_stay", !p.Armed() && !p.RngOn() && !p.Active() && !d.answer
            && d.seen == Seen::Idle && p.RowCounters(BuiltinRowOf(Builtin::Irandom)).calls == 2
            && p.RowCounters(BuiltinRowOf(Builtin::Irandom)).answered == 1 && p.StatusLine().rfind("gambaprobe: off", 0) == 0);
    }

    // ---- the decision keys ------------------------------------------------------------
    {
        bool keys = kDecisionKeyCount == 6;
        const char* expected[] = { "roll-route", "explosion-rule", "drop-route", "counter-route", "fallback-drop", "pity-design" };
        for (int i = 0; i < kDecisionKeyCount && keys; ++i) keys = kDecisionKeys[i].key == expected[i];
        check("decision/six_keys_in_order", keys);
        check("decision/listed_labels_valid", DecisionLabelValid("roll-route", "builtin", true)
            && DecisionLabelValid("explosion-rule", "spins", true) && DecisionLabelValid("counter-route", "both", true)
            && DecisionLabelValid("fallback-drop", "not-run", true) && DecisionLabelValid("pity-design", "answer-roll", true)
            && DecisionLabelValid("drop-route", "not-observed", true));
        check("decision/pending_only_before_live_1", DecisionLabelValid("roll-route", "pending", false)
            && !DecisionLabelValid("roll-route", "pending", true) && DecisionLabelValid("drop-route", "pending", false)
            && !DecisionLabelValid("drop-route", "pending", true));
        check("decision/drop_route_takes_a_name", DecisionLabelValid("drop-route", "LootGroundCreateFromItem", true)
            && DecisionLabelValid("drop-route", "instance_create_depth", true) && !DecisionLabelValid("drop-route", "two words", true)
            && !DecisionLabelValid("drop-route", "", true));
        check("decision/unlisted_label_or_key_refused", !DecisionLabelValid("roll-route", "maybe", false)
            && !DecisionLabelValid("pity-design", "LootGroundCreateFromItem", true) && !DecisionLabelValid("no-such-key", "builtin", false)
            && !DecisionLabelValid("fallback-drop", "", false));
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
