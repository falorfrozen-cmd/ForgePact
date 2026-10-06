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
//
// Phase 4, the explosion watch (Watch): baseline, no window line without a
// transition or a `window` command, and one first-sight line per machine;
// target, a sprite change or a vanished machine opens one window that replays
// the ring's look-back, logs forward until its span ends, stops at its own caps
// and is extended by a second transition.
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
static const Builtin kOtherRows[] = { Builtin::InstanceDestroy, Builtin::InstanceCreateDepth, Builtin::InstanceCreateLayer,
                                      Builtin::InstanceChange, Builtin::LayerDestroyInstances,
                                      Builtin::InstanceDeactivateObject, Builtin::RoomGoto,
                                      Builtin::GetVariable, Builtin::SetVariable, Builtin::SetVariableToUndefined };

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
        bool ok = kBuiltinCount == 15 && kBuiltins[static_cast<int>(Builtin::InstanceChange)].name == "instance_change"
            && kBuiltins[static_cast<int>(Builtin::LayerDestroyInstances)].name == "layer_destroy_instances"
            && kBuiltins[static_cast<int>(Builtin::InstanceDeactivateObject)].name == "instance_deactivate_object"
            && kBuiltins[static_cast<int>(Builtin::RoomGoto)].name == "room_goto";
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
            " answered=1 out-of-range=1 passed=1 machine-arg=0 caller-walks=0 walks-skipped=0", line);
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

    // ---- the by-argument rule (replan 1) ---------------------------------------------
    {
        // Baseline: the rule is off for every row but the three that take a
        // target, and an idle probe or a machine-self call is never a
        // machine-arg call.
        bool rows = BuiltinChecksArgument(Builtin::InstanceDestroy) && BuiltinChecksArgument(Builtin::InstanceChange)
            && BuiltinChecksArgument(Builtin::InstanceDeactivateObject);
        for (Builtin b : { Builtin::Irandom, Builtin::Choose, Builtin::InstanceCreateDepth, Builtin::InstanceCreateLayer,
                           Builtin::LayerDestroyInstances, Builtin::RoomGoto })
            if (BuiltinChecksArgument(b)) rows = false;
        check("machinearg/only_the_target_taking_rows", rows);
        Probe p = make();
        const ArgTarget machine{ ArgKind::Instance, kMachine, false };
        const ArgTarget machineObject{ ArgKind::Object, kMachine, false };
        const ArgTarget parent{ ArgKind::Object, 959, true };
        const ArgTarget all{ ArgKind::All, -1, false };
        const ArgTarget player{ ArgKind::Instance, kPlayer, false };
        const ArgTarget enemyObject{ ArgKind::Object, kEnemy, false };
        const ArgTarget none{};
        check("machinearg/names_a_machine", p.ArgNamesMachine(machine) && p.ArgNamesMachine(machineObject)
            && p.ArgNamesMachine(parent) && p.ArgNamesMachine(all));
        // Negative control: another instance, another object, nothing read.
        check("machinearg/names_nothing_else", !p.ArgNamesMachine(player) && !p.ArgNamesMachine(enemyObject)
            && !p.ArgNamesMachine(none) && !p.ArgNamesMachine(ArgTarget{ ArgKind::Instance, -1, false }));
        Probe u = make();
        u.SetMachineObject(-1);
        check("machinearg/unresolved_machine_names_nothing", !u.ArgNamesMachine(machine) && !u.ArgNamesMachine(all)
            && !u.ArgNamesMachine(parent));
        const int row = BuiltinRowOf(Builtin::InstanceDestroy);
        const Seen idle = observe(p, row, kPlayer);
        const bool idleNoted = p.NoteMachineArg(row, idle, true);
        p.SetArmed(true);
        const Seen other = observe(p, row, kPlayer);
        const bool otherNoted = p.NoteMachineArg(row, other, true);
        const Seen inEvent = observe(p, row, kPlayer, true);
        const bool inEventNoted = p.NoteMachineArg(row, inEvent, true);
        const Seen self = observe(p, row, kMachine);
        const bool selfNoted = p.NoteMachineArg(row, self, true);
        const bool unrelated = p.NoteMachineArg(row, observe(p, row, kEnemy), false);
        check("machinearg/another_selfs_call_naming_a_machine_is_counted", !idleNoted && otherNoted && inEventNoted
            && !selfNoted && !unrelated && p.RowCounters(row).machineArg == 2 && p.RowCounters(row).machineSelf == 1
            && p.RowCounters(row).calls == 5, p.RowText(row));
        check("machinearg/status_reads_it_back", contains(p.RowText(row), " machine-arg=2")
            && !contains(p.RowText(BuiltinRowOf(Builtin::RoomGoto)), "machine-arg")
            && !contains(p.RowText(BuiltinRowOf(Builtin::Irandom)), "machine-arg")
            && contains(p.StatusLine(), " machine-arg=2 "), p.RowText(row));
        check("machinearg/bad_row_refused", !p.NoteMachineArg(-1, Seen::Other, true)
            && !p.NoteMachineArg(p.RowCount(), Seen::Other, true));
    }

    // ---- the caller walk (replan 1) ---------------------------------------------------
    {
        check("walk/budgets", CallerWalkBudget(Event::CleanUp) == 4 && CallerWalkBudget(Event::Alarm9) == 4
            && CallerWalkBudget(Event::Create) == 1 && CallerWalkBudget(Event::Step) == 0
            && CallerWalkBudget(Event::Alarm0) == 0 && kCallerWalkFrames == 24 && kCallerWalksPerRow == 4);
        Probe p = make();
        int cleanups = 0, creates = 0, steps = 0;
        for (int i = 0; i < 10; ++i) {
            if (p.TakeCallerWalk(Event::CleanUp)) ++cleanups;
            if (p.TakeCallerWalk(Event::Create)) ++creates;
            if (p.TakeCallerWalk(Event::Step)) ++steps;
        }
        const Counters& c = p.RowCounters(EventRowOf(Event::CleanUp));
        check("walk/first_four_per_row_then_counted", cleanups == 4 && creates == 1 && steps == 0 && c.walked == 4
            && c.walkSkipped == 6 && p.RowCounters(EventRowOf(Event::Create)).walkSkipped == 9
            && p.RowCounters(EventRowOf(Event::Step)).walkSkipped == 0, p.RowText(EventRowOf(Event::CleanUp)));
        check("walk/status_reads_it_back", contains(p.RowText(EventRowOf(Event::CleanUp)), " caller-walks=4/4 walks-skipped=6")
            && contains(p.RowText(EventRowOf(Event::Create)), " caller-walks=1/1 walks-skipped=9")
            && !contains(p.RowText(EventRowOf(Event::Step)), "caller-walks")
            && contains(p.StatusLine(), " caller-walks=5 walks-skipped=15"), p.StatusLine());
        p.ResetTrace();
        check("walk/a_new_window_starts_the_walks_over", p.TakeCallerWalk(Event::CleanUp) && p.TakeCallerWalk(Event::Create)
            && !p.TakeCallerWalk(Event::Create) && p.RowCounters(EventRowOf(Event::CleanUp)).walked == 1
            && p.RowCounters(EventRowOf(Event::CleanUp)).walkSkipped == 6);
    }
    {
        // The frame formatter: a pure function of the frame, the function the
        // unwind table puts it in, the sorted rows and the module spans.
        std::vector<CodeRow> rows = { { 0x14000a000, "gml_Object_Slot_Machine_01_obj_CleanUp_0" },
                                      { 0x140001000, "gml_Script_InitPV" },
                                      { 0x140005000, "gml_Object_Zone_obj_Step_0" } };
        SortCodeRows(rows);
        const CodeModule game{ 0x140000000, 0x150000000, "Hero_Siege.exe" };
        const CodeModule plugin{ 0x7ff800000000, 0x7ff800100000, "BloodPactPlugin.dll" };
        const CodeModule kernel{ 0x7ffa00000000, 0x7ffa00200000, "KERNEL32.DLL" };
        const CodeModule none{};
        check("walk/rows_sorted_by_function", rows[0].function == 0x140001000 && rows[2].function == 0x14000a000
            && NearestRow(rows, 0x140005010) == &rows[1] && NearestRow(rows, 0x140000fff) == nullptr
            && NearestRow(rows, 0x140005000) == &rows[1]);
        const std::string gml = FrameText(3, 0x140005234, 0, rows, game, plugin, none);
        const std::string gmlUnwind = FrameText(3, 0x140005234, 0x140005000, rows, game, plugin, none);
        check("walk/frame_in_a_gml_row", gml == "  #3 gml:gml_Object_Zone_obj_Step_0+0x234" && gmlUnwind == gml, gml);
        const std::string runner = FrameText(4, 0x140007000, 0x140006f00, rows, game, plugin, none);
        check("walk/runner_frame_above_a_row_is_exe", runner == "  #4 exe+0x7000 (runner code; nearest row below"
            " gml:gml_Object_Zone_obj_Step_0+0x2000)", runner);
        const std::string below = FrameText(5, 0x140000500, 0, rows, game, plugin, none);
        check("walk/runner_frame_below_every_row_is_exe", below == "  #5 exe+0x500", below);
        const std::string own = FrameText(0, 0x7ff800012345, 0, rows, game, plugin, none);
        check("walk/plugin_frame", own == "  #0 forgepact+0x12345", own);
        const std::string module = FrameText(9, 0x7ffa00001010, 0, rows, game, plugin, kernel);
        check("walk/other_module_frame", module == "  #9 KERNEL32.DLL+0x1010", module);
        const std::string unknown = FrameText(10, 0x50000, 0, rows, game, plugin, none);
        const std::string emptyRows = FrameText(1, 0x140005234, 0, {}, game, plugin, none);
        check("walk/unknown_frame_and_no_rows", unknown == "  #10 ?" && emptyRows == "  #1 exe+0x5234", unknown + " | " + emptyRows);
    }

    // ---- the by-name route (replan 1) -----------------------------------------------
    {
        const uintptr_t fn = 0x140001000;
        const NameLookup script{ true, kScriptIndexBase + 17, 0, false };
        const NameLookup unresolved{};
        const NameLookup routine{ true, 2900, 0x140200000, true };
        const NameLookup ownRoutine{ true, 2900, fn, true };
        const NameLookup foreign{ true, 2900, 0x7ff800001000, false };
        const NameLookup noPointer{ true, 2900, 0, false };
        check("byname/words", ByNameWord(ByName::Same) == "same" && ByNameWord(ByName::Detoured) == "detoured"
            && ByNameWord(ByName::Shared) == "shared" && ByNameWord(ByName::Missing) == "missing"
            && ByNameWord(ByName::NotRead) == "not-read" && kScriptIndexBase == 100000);
        check("byname/index_kinds", NamesScript(script) && !NamesRoutine(script) && NamesRoutine(routine)
            && !NamesScript(routine) && !NamesScript(unresolved) && !NamesRoutine(unresolved));
        check("byname/both_names_the_script_is_same", ClassifyByName(script, script, fn) == ByName::Same);
        check("byname/a_routine_that_is_the_rows_own_function_is_same", ClassifyByName(script, ownRoutine, fn) == ByName::Same);
        check("byname/another_game_routine_is_detoured", ClassifyByName(script, routine, fn) == ByName::Detoured
            && ClassifyByName(routine, script, fn) == ByName::Detoured);
        check("byname/full_name_unresolved_is_missing", ClassifyByName(script, unresolved, fn) == ByName::Missing
            && ClassifyByName(routine, unresolved, fn) == ByName::Missing);
        check("byname/an_undetourable_routine_is_missing", ClassifyByName(script, foreign, fn) == ByName::Missing
            && ClassifyByName(noPointer, script, fn) == ByName::Missing);
        check("byname/several_rows_on_one_routine_share_it", WithSharing(ByName::Detoured, 3) == ByName::Shared
            && WithSharing(ByName::Detoured, 1) == ByName::Detoured && WithSharing(ByName::Same, 3) == ByName::Same
            && WithSharing(ByName::Missing, 2) == ByName::Missing);
        const std::string text = ByNameText(ByName::Same, script, NameLookup{ true, kScriptIndexBase + 17, 0, false });
        const std::string missing = ByNameText(ByName::Missing, script, unresolved);
        check("byname/status_text_ends_with_the_word", text == "idx=100017/100017 byname=same"
            && missing == "idx=100017/none byname=missing"
            && ByNameText(ByName::Shared, routine, routine) == "idx=2900/2900 byname=shared", text + " | " + missing);
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

    // ---- phase 4: the explosion watch -------------------------------------------------
    // A machine is a number id and the sprite name the adapter read; a call
    // is the text the adapter formatted. Frames are the end-of-frame poll's:
    // a step's calls carry the previous poll's frame, so a build in the
    // transition's own step is one frame behind the poll that notices it.
    {
        check("watch/limits", kWatchRingSize == 64 && kWatchInstanceRingSize == 256 && kWindowLookBackFrames == 2
            && kWindowSpanDefault == 600 && kWindowSpanMax == 3600 && kWindowBuildLineCap == 400
            && kWindowInstanceLineCap == 800 && kWindowInstanceLinesPerObject == 32
            && kWindowBuildLineCap != kTraceLinesPerRow && kWindowBuildLineCap != kTraceLinesPerKey);
        check("watch/window_builtin_rows", BuiltinInWindow(Builtin::InstanceCreateLayer) && BuiltinInWindow(Builtin::InstanceCreateDepth)
            && BuiltinInWindow(Builtin::InstanceDestroy) && !BuiltinInWindow(Builtin::Irandom) && !BuiltinInWindow(Builtin::Choose)
            && !BuiltinInWindow(Builtin::InstanceChange) && !BuiltinInWindow(Builtin::GetVariable));
        check("watch/span_default_and_cap", WindowSpan(0) == kWindowSpanDefault && WindowSpan(-5) == kWindowSpanDefault
            && WindowSpan(1) == 1 && WindowSpan(1800) == 1800 && WindowSpan(3600) == 3600 && WindowSpan(100000) == kWindowSpanMax);
    }
    {
        // The fixed lines, byte for byte (the contract test and the live
        // procedure read them).
        const std::string args = " a0=real:0 a1=real:72 a2=bool:true";
        bool lines = MachineFirstLine(494624, "Slot_Machine_01_spr", 10) == "gambaprobe machine id=494624 sprite=Slot_Machine_01_spr frame=10";
        lines = lines && MachineChangeLine(494624, "Slot_Machine_01_spr", "Slot_Machine_01_Destroyed_spr", 11)
            == "gambaprobe machine id=494624 sprite Slot_Machine_01_spr -> Slot_Machine_01_Destroyed_spr frame=11";
        lines = lines && MachineGoneLine(494624, 12) == "gambaprobe machine id=494624 gone frame=12";
        lines = lines && WindowOpenLine(WindowReason::Sprite, 494624, 11, 3, 600)
            == "gambaprobe window open reason=sprite id=494624 frame=11 replayed=3 span=600";
        lines = lines && WindowOpenLine(WindowReason::Gone, 7, 11, 0, 600) == "gambaprobe window open reason=gone id=7 frame=11 replayed=0 span=600";
        lines = lines && WindowOpenLine(WindowReason::Command, -1, 11, 0, 1800)
            == "gambaprobe window open reason=command id=- frame=11 replayed=0 span=1800";
        lines = lines && WindowCallLine("CreateDefaultParams", "machine id=494624", 3, args, 11)
            == "gambaprobe window CreateDefaultParams self=machine id=494624 argc=3 a0=real:0 a1=real:72 a2=bool:true frame=11";
        lines = lines && WindowCallLine("instance_destroy", "Coin_obj id=9", 0, "", 4) == "gambaprobe window instance_destroy self=Coin_obj id=9 argc=0 frame=4";
        lines = lines && WindowReplayLine("CreateDefaultParams", "machine id=494624", 3, args, 10)
            == "gambaprobe window replay CreateDefaultParams self=machine id=494624 argc=3 a0=real:0 a1=real:72 a2=bool:true frame=10";
        lines = lines && WindowBuiltLine("10", "0", "98", "1", "4", "Goburin's Head", "machine id=494624", 11)
            == "gambaprobe window built itemType=10 j=0 b=98 c=1 rarity=4 name=Goburin's Head self=machine id=494624 frame=11";
        lines = lines && WindowExtendedLine(WindowReason::Gone, 42, 611, 311)
            == "gambaprobe window extended reason=gone id=42 end=611 frame=311";
        lines = lines && WindowClosedLine(12, 2, 30, 4, 1, 311)
            == "gambaprobe window closed build-lines=12 build-dropped=2 instance-lines=30 instance-dropped=4 capped=1 frame=311";
        lines = lines && WindowCappedLine("instance_create_layer real:777", 40)
            == "gambaprobe window capped instance_create_layer real:777 frame=40";
        lines = lines && OptionalNumberText(true, 98.0) == "98" && OptionalNumberText(false, 98.0) == "?"
            && OptionalNumberText(true, 0.5) == "0.5";
        check("watch/fixed_lines", lines);
        Watch w;
        const std::string closedStatus = w.StatusLine(5);
        // While a window is open, status names its end, the current frame and
        // what remains, so an operator can re-send `window` before it ends.
        w.Open(WindowReason::Command, -1, 100, 3600);
        const std::string openStatus = w.StatusLine(1000);
        const std::string lateStatus = w.StatusLine(3800);
        check("watch/status_line", closedStatus == "gambaprobe watch: machines-seen=0 transitions=0 windows=0 window=closed ring=0"
              " instance-ring=0 build-dropped=0 instance-dropped=0 capped=0"
            && openStatus == "gambaprobe watch: machines-seen=0 transitions=0 windows=1 window=open end=3700 frame=1000 remaining=2700"
              " ring=0 instance-ring=0 build-dropped=0 instance-dropped=0 capped=0"
            && contains(lateStatus, " window=open end=3700 frame=3800 remaining=0 "), openStatus + " | " + lateStatus);
    }
    {
        // Baseline: no transition and no `window` command - no window line,
        // and the trace budgets count exactly as they did without the watch.
        Probe p = make();
        Probe q = make();
        Watch w;
        const int cdp = ScriptRowOf(21);
        int windowLines = 0;
        for (int64_t frame = 0; frame < 900; ++frame) {
            for (const std::string& line : w.Poll(frame, { { 1, "Slot_Machine_01_spr" } }))
                if (contains(line, "gambaprobe window")) ++windowLines;
            if (!w.Tick(frame).empty()) ++windowLines;
            observe(p, cdp, kEnemy);
            observe(q, cdp, kEnemy);
            w.Push({ "CreateDefaultParams", "Enemy id=1", 3, " a0=real:0", frame });
            w.Push({ "instance_create_layer", "Enemy id=1", 4, " a3=real:900", frame, CallKind::Instance, "instance_create_layer real:900" });
            if (w.TakeBuildLine(frame)) ++windowLines;
            if (w.TakeInstanceLine(frame, "instance_create_layer real:900")) ++windowLines;
            p.TakeTraceLine(cdp, static_cast<uint64_t>(frame % 3), static_cast<uint64_t>(frame));
            q.TakeTraceLine(cdp, static_cast<uint64_t>(frame % 3), static_cast<uint64_t>(frame));
        }
        check("watch/baseline_no_window_without_a_transition", windowLines == 0 && !w.WindowOpen() && w.Windows() == 0
            && w.BuildLines() == 0 && w.BuildDropped() == 0 && w.InstanceLines() == 0 && w.InstanceDropped() == 0 && !w.InWindow(899));
        check("watch/baseline_trace_budgets_unchanged", p.RowText(cdp) == q.RowText(cdp) && p.StatusLine() == q.StatusLine()
            && p.RowCounters(cdp).logged == static_cast<uint64_t>(3 * kTraceLinesPerKey), p.RowText(cdp));
        check("watch/ring_is_bounded", w.RingSize() == kWatchRingSize && w.InstanceRingSize() == kWatchInstanceRingSize);
        Watch s;
        const std::vector<std::string> a = s.Poll(5, { { 42, "Slot_Machine_01_spr" } });
        const std::vector<std::string> b = s.Poll(6, { { 42, "Slot_Machine_01_spr" } });
        const std::vector<std::string> c = s.Poll(7, { { 42, "Slot_Machine_01_spr" } });
        check("watch/baseline_same_sprite_one_first_sight_line", a.size() == 1
            && a[0] == "gambaprobe machine id=42 sprite=Slot_Machine_01_spr frame=5" && b.empty() && c.empty()
            && s.MachinesSeen() == 1 && s.Transitions() == 0 && s.Windows() == 0 && !s.WindowOpen());
    }
    {
        // Target: a sprite change is one change line and opens a window that
        // replays both rings' last kWindowLookBackFrames frames, in call
        // order - the instance create of the transition's own step included.
        Watch w;
        w.Poll(100, { { 42, "Slot_Machine_01_spr" } });
        w.Push({ "CreateDefaultParams", "Enemy id=1", 3, " a0=real:0", 96 });   // older than the look-back
        w.Push({ "CreateDefaultParams", "machine id=42", 3, " a0=real:0 a1=real:72", 98 });
        w.Push({ "CreateItemNew", "machine id=42", 1, " a0=real:15", 99 });
        w.Push({ "instance_create_layer", "machine id=42", 4, " a3=real:777", 99, CallKind::Instance, "instance_create_layer real:777" });
        w.Push({ "LootGroundCreate", "Player_obj id=3", 4, " a0=real:1", 100 });
        const std::vector<std::string> out = w.Poll(100, { { 42, "Slot_Machine_01_Destroyed_spr" } });
        const bool shape = out.size() == 6
            && out[0] == "gambaprobe machine id=42 sprite Slot_Machine_01_spr -> Slot_Machine_01_Destroyed_spr frame=100"
            && out[1] == "gambaprobe window open reason=sprite id=42 frame=100 replayed=4 span=600"
            && out[2] == "gambaprobe window replay CreateDefaultParams self=machine id=42 argc=3 a0=real:0 a1=real:72 frame=98"
            && out[3] == "gambaprobe window replay CreateItemNew self=machine id=42 argc=1 a0=real:15 frame=99"
            && out[4] == "gambaprobe window replay instance_create_layer self=machine id=42 argc=4 a3=real:777 frame=99"
            && out[5] == "gambaprobe window replay LootGroundCreate self=Player_obj id=3 argc=4 a0=real:1 frame=100";
        std::string all;
        for (const std::string& line : out) all += line + " | ";
        check("watch/sprite_change_opens_a_window_and_replays_the_ring", shape && w.WindowOpen() && w.Windows() == 1
            && w.Transitions() == 1 && w.BuildLines() == 3 && w.InstanceLines() == 1, all);
        // Forward calls are window lines until the span ends; then the
        // window closes once and reports both kinds of lines.
        const bool in = w.InWindow(100) && w.InWindow(699) && !w.InWindow(700);
        const bool f1 = w.TakeBuildLine(150);
        const bool late = w.TakeBuildLine(700);
        const std::string open = w.Tick(699);
        const std::string closed = w.Tick(700);
        const std::string again = w.Tick(701);
        check("watch/forward_lines_until_the_span_ends", in && f1 && !late && open.empty()
            && closed == "gambaprobe window closed build-lines=4 build-dropped=0 instance-lines=1 instance-dropped=0 capped=0 frame=700"
            && again.empty() && !w.WindowOpen()
            && w.StatusLine(701) == "gambaprobe watch: machines-seen=1 transitions=1 windows=1 window=closed ring=4 instance-ring=1"
                                    " build-dropped=0 instance-dropped=0 capped=0", closed + " / " + w.StatusLine(701));
    }
    {
        // A machine the refresh no longer finds: one gone line and a window;
        // it is forgotten, so it is not gone twice.
        Watch w;
        w.Poll(10, { { 42, "Slot_Machine_01_spr" }, { 43, "Slot_Machine_01_spr" } });
        const std::vector<std::string> out = w.Poll(11, { { 43, "Slot_Machine_01_spr" } });
        const std::vector<std::string> next = w.Poll(12, { { 43, "Slot_Machine_01_spr" } });
        check("watch/a_machine_that_disappears_is_one_gone_line_and_a_window", out.size() == 2
            && out[0] == "gambaprobe machine id=42 gone frame=11"
            && out[1] == "gambaprobe window open reason=gone id=42 frame=11 replayed=0 span=600" && next.empty()
            && w.Transitions() == 1 && w.Windows() == 1 && w.MachinesSeen() == 2, out.empty() ? "" : out[0]);
    }
    {
        // Two caps of the window's own: instance lines spend theirs (and
        // their object's), never the build cap; each counts what it dropped,
        // and the per-row trace budget never moves.
        Probe p = make();
        Watch w;
        const std::vector<std::string> open = w.Open(WindowReason::Command, -1, 0, 3600);
        int oneObject = 0;
        for (int i = 0; i < kWindowInstanceLinesPerObject + 8; ++i) if (w.TakeInstanceLine(1, "instance_create_layer real:5")) ++oneObject;
        int manyObjects = 0;
        for (int i = 0; i < kWindowInstanceLineCap; ++i) if (w.TakeInstanceLine(2, "obj " + std::to_string(i))) ++manyObjects;
        int built = 0;
        for (int i = 0; i < kWindowBuildLineCap + 50; ++i) if (w.TakeBuildLine(1 + i % 100)) ++built;
        check("watch/instance_lines_stop_per_object_and_at_their_cap", oneObject == kWindowInstanceLinesPerObject
            && manyObjects == kWindowInstanceLineCap - kWindowInstanceLinesPerObject && w.InstanceDropped() == 8 + kWindowInstanceLinesPerObject,
              std::to_string(oneObject) + " " + std::to_string(manyObjects));
        const std::string closed = w.Tick(3600);
        check("watch/build_lines_keep_their_own_cap_and_count_what_they_dropped", built == kWindowBuildLineCap && w.Windows() == 1
            && closed == "gambaprobe window closed build-lines=400 build-dropped=50 instance-lines=800 instance-dropped=40 capped=1 frame=3600"
            && p.RowCounters(ScriptRowOf(21)).logged == 0 && p.SpentRows() == 0, closed);
        check("watch/command_opens_the_same_window", open.size() == 1
            && open[0] == "gambaprobe window open reason=command id=- frame=0 replayed=0 span=3600", open.empty() ? "" : open[0]);
    }
    {
        // `window` on an open window tops both caps up; the closed line keeps
        // the window's totals.
        Watch w;
        w.Open(WindowReason::Command, -1, 0, 1000);
        for (int i = 0; i < kWindowBuildLineCap + 1; ++i) w.TakeBuildLine(1);
        for (int i = 0; i < kWindowInstanceLinesPerObject + 1; ++i) w.TakeInstanceLine(1, "k");
        const bool spent = !w.TakeBuildLine(2) && !w.TakeInstanceLine(2, "k");
        w.Open(WindowReason::Command, -1, 10, 1000);
        const bool again = w.TakeBuildLine(11) && w.TakeInstanceLine(11, "k");
        const std::string closed = w.Tick(1010);
        check("watch/a_command_on_an_open_window_tops_up_both_caps", spent && again && w.Windows() == 1
            && closed == "gambaprobe window closed build-lines=401 build-dropped=2 instance-lines=33 instance-dropped=2 capped=1 frame=1010",
              closed);
    }
    {
        // A burst of instance calls cannot evict a build from its ring: the
        // window still replays the build, and the burst only to its
        // per-object cap.
        Watch w;
        w.Push({ "CreateDefaultParams", "machine id=42", 3, " a0=real:0", 99 });
        for (int i = 0; i < 300; ++i)
            w.Push({ "instance_create_layer", "machine id=42", 4, " a3=real:12", 99 + i % 2, CallKind::Instance, "instance_create_layer real:12" });
        const std::vector<std::string> out = w.Open(WindowReason::Command, -1, 100, 600);
        check("watch/an_instance_burst_cannot_evict_a_build", w.RingSize() == 1 && w.InstanceRingSize() == kWatchInstanceRingSize
            && out.size() == static_cast<size_t>(3 + kWindowInstanceLinesPerObject)
            && out.back() == "gambaprobe window capped instance_create_layer real:12 frame=100"
            && out[0] == "gambaprobe window open reason=command id=- frame=100 replayed=33 span=600"
            && out[1] == "gambaprobe window replay CreateDefaultParams self=machine id=42 argc=3 a0=real:0 frame=99"
            && w.BuildLines() == 1 && w.InstanceLines() == static_cast<uint64_t>(kWindowInstanceLinesPerObject)
            && w.InstanceDropped() == static_cast<uint64_t>(kWatchInstanceRingSize - kWindowInstanceLinesPerObject),
              out.empty() ? "" : out[0]);
    }
    {
        // The first refusal of an object's share names it, once per window
        // (a top-up does not name it again); a refusal by the overall cap
        // names nothing; a new window names it again.
        Watch w;
        w.Open(WindowReason::Command, -1, 0, 1000);
        for (int i = 0; i < kWindowInstanceLinesPerObject; ++i) w.TakeInstanceLine(1, "instance_create_layer Coin_obj");
        const bool none = w.TakeCappedLines(1).empty();
        const bool refused = !w.TakeInstanceLine(1, "instance_create_layer Coin_obj");
        const std::vector<std::string> first = w.TakeCappedLines(1);
        w.TakeInstanceLine(2, "instance_create_layer Coin_obj");
        const bool once = w.TakeCappedLines(2).empty();
        w.Open(WindowReason::Command, -1, 3, 1000);   // a top-up
        for (int i = 0; i < kWindowInstanceLinesPerObject + 1; ++i) w.TakeInstanceLine(4, "instance_create_layer Coin_obj");
        const bool notAgain = w.TakeCappedLines(4).empty() && w.Capped() == 1;
        const std::string status = w.StatusLine(5);
        w.Tick(1003);
        w.Open(WindowReason::Command, -1, 1004, 100);
        for (int i = 0; i < kWindowInstanceLinesPerObject + 1; ++i) w.TakeInstanceLine(1005, "instance_create_layer Coin_obj");
        const std::vector<std::string> next = w.TakeCappedLines(1005);
        check("watch/a_capped_object_is_named_once_per_window", none && refused && first.size() == 1
            && first[0] == "gambaprobe window capped instance_create_layer Coin_obj frame=1" && once && notAgain
            && contains(status, " capped=1") && next.size() == 1, first.empty() ? status : first[0]);
    }
    {
        // A second transition while a window is open extends it: no second
        // open line, no second window, and the old end moves out.
        Watch w;
        w.Poll(0, { { 42, "a" }, { 43, "b" } });
        w.Poll(10, { { 42, "a2" }, { 43, "b" } });
        const std::vector<std::string> second = w.Poll(200, { { 42, "a2" }, { 43, "b2" } });
        const bool extended = w.InWindow(750) && !w.InWindow(800) && w.WindowEnd() == 800;
        const std::vector<std::string> cmd = w.Open(WindowReason::Command, -1, 300, 1000);
        check("watch/a_transition_while_open_extends_the_window", second.size() == 2
            && second[0] == "gambaprobe machine id=43 sprite b -> b2 frame=200"
            && second[1] == "gambaprobe window extended reason=sprite id=43 end=800 frame=200" && extended && w.Windows() == 1
            && w.Transitions() == 2 && cmd.size() == 1
            && cmd[0] == "gambaprobe window extended reason=command id=- end=1300 frame=300" && w.WindowEnd() == 1300
            && w.Windows() == 1, second.empty() ? "" : second[0]);
        // A call the open window already counted is not replayed by the next
        // window's look-back.
        w.Push({ "CreateItemNew", "Enemy id=1", 1, "", 1299 });
        w.Push({ "instance_destroy", "Enemy id=1", 0, "", 1299, CallKind::Instance, "destroy 5004" });
        w.Tick(1300);
        const std::vector<std::string> reopen = w.Open(WindowReason::Command, -1, 1300, 10);
        check("watch/a_call_a_window_counted_is_not_replayed", reopen.size() == 1
            && reopen[0] == "gambaprobe window open reason=command id=- frame=1300 replayed=0 span=10" && w.Windows() == 2,
              reopen.empty() ? "" : reopen[0]);
    }
    {
        // `off`: the record is forgotten and an open window closes with its
        // line; the counts stay.
        Watch w;
        w.Poll(0, { { 42, "a" } });
        w.Open(WindowReason::Command, -1, 5, 100);
        w.TakeBuildLine(6);
        const std::string closed = w.Off(7);
        const std::vector<std::string> after = w.Poll(8, { { 42, "a" } });
        check("watch/off_closes_and_forgets_the_machines",
              closed == "gambaprobe window closed build-lines=1 build-dropped=0 instance-lines=0 instance-dropped=0 capped=0 frame=7"
            && !w.WindowOpen() && after.size() == 1 && after[0] == "gambaprobe machine id=42 sprite=a frame=8"
            && w.Off(9).empty() && w.Windows() == 1, closed);
    }

    // ---- phase 5: N2, the `full` line, and N1, the by-name mark -------------------------
    {
        // Baseline: a window that never reaches a cap prints exactly what it
        // printed before - no `full` line, no `capped` line.
        Watch w;
        w.Open(WindowReason::Command, -1, 0, 100);
        for (int i = 0; i < 10; ++i) w.TakeBuildLine(1);
        for (int i = 0; i < 10; ++i) w.TakeInstanceLine(1, "obj " + std::to_string(i));
        const std::vector<std::string> pending = w.TakeCappedLines(2);
        const std::string closed = w.Tick(100);
        check("watch/baseline_a_window_below_its_caps_prints_no_full_line", pending.empty()
            && closed == "gambaprobe window closed build-lines=10 build-dropped=0 instance-lines=10 instance-dropped=0 capped=0 frame=100",
              closed);
    }
    {
        // The first refusal of each overall cap prints one `full` line per
        // window, at the refused call's frame; a top-up does not print it
        // again; a new window does.
        Watch w;
        w.Open(WindowReason::Command, -1, 0, 1000);
        for (int i = 0; i < kWindowBuildLineCap; ++i) w.TakeBuildLine(1);
        const bool noneYet = w.TakeCappedLines(1).empty();
        const bool refused = !w.TakeBuildLine(7);
        const std::vector<std::string> build = w.TakeCappedLines(8);
        const bool buildOnce = !w.TakeBuildLine(9) && w.TakeCappedLines(9).empty();
        for (int i = 0; i < kWindowInstanceLineCap; ++i) w.TakeInstanceLine(10, "obj " + std::to_string(i));
        w.TakeInstanceLine(11, "one more");
        w.TakeInstanceLine(12, "and another");
        const std::vector<std::string> instance = w.TakeCappedLines(12);
        w.Open(WindowReason::Command, -1, 20, 1000);   // a top-up
        for (int i = 0; i < kWindowBuildLineCap + 1; ++i) w.TakeBuildLine(21);
        const bool notAgain = w.TakeCappedLines(21).empty();
        w.Tick(1020);
        w.Open(WindowReason::Command, -1, 1021, 100);
        for (int i = 0; i < kWindowBuildLineCap + 1; ++i) w.TakeBuildLine(1022);
        const std::vector<std::string> next = w.TakeCappedLines(1022);
        check("watch/the_first_refusal_of_each_cap_prints_one_full_line_per_window", noneYet && refused && buildOnce
            && build.size() == 1 && build[0] == "gambaprobe window full build frame=7"
            && instance.size() == 1 && instance[0] == "gambaprobe window full instance frame=11"
            && notAgain && next.size() == 1 && next[0] == "gambaprobe window full build frame=1022",
              build.empty() ? "" : build[0]);
        // A per-object refusal is a `capped` line, never a `full` one.
        Watch o;
        o.Open(WindowReason::Command, -1, 0, 100);
        for (int i = 0; i < kWindowInstanceLinesPerObject + 1; ++i) o.TakeInstanceLine(1, "Coin_obj");
        const std::vector<std::string> capped = o.TakeCappedLines(1);
        check("watch/a_per_object_refusal_is_not_a_full_line", capped.size() == 1
            && capped[0] == "gambaprobe window capped Coin_obj frame=1", capped.empty() ? "" : capped[0]);
        check("watch/full_line_text", WindowFullLine(CallKind::Build, 5) == "gambaprobe window full build frame=5"
            && WindowFullLine(CallKind::Instance, 6) == "gambaprobe window full instance frame=6");
    }
    {
        // N1: the by-name mark is consumed by the first call the slot routes;
        // a later matching call inside the same original is its own, and a
        // call the slot does not route leaves the mark.
        int mark = 3;
        const bool other = ConsumeFedMark(mark, false);
        const bool kept = mark == 3;
        const bool first = ConsumeFedMark(mark, true);
        const bool second = ConsumeFedMark(mark, true);
        int none = -1;
        const bool unmarked = ConsumeFedMark(none, true);
        check("watch/the_by_name_mark_feeds_only_the_first_matching_call",
              !other && kept && first && mark == -1 && !second && !unmarked && none == -1);
    }
    {
        // F1: a mark consumed inside a nested call stays consumed. Slots 3 and
        // 5 both route script 10; script 20 is routed by neither. `script`
        // stands for GpFedByName: true means skipped as already fed.
        FedMark* cur = nullptr;
        auto script = [&cur](int s) {
            return cur && cur->slot >= 0 && ConsumeFedMark(cur->slot, s == 10);
        };
        bool direct = false, directSibling = true, viaScript = false, scriptSibling = true;
        bool viaUnfed = false, unfedSibling = true, ownMark = false, outerSibling = false, unfedNoMark = true;
        {
            FedMarkScope a(cur, true, 3);                // A, a fed by-name call
            direct = script(10);                         // B consumes A's mark
            directSibling = script(10);                  // B' is fed as its own
        }
        const bool cleared = cur == nullptr;
        {
            FedMarkScope a(cur, true, 3);
            const bool x = script(20);                   // X routes nowhere: the mark stays
            viaScript = !x && script(10);                // B, inside X, consumes it
            scriptSibling = script(10);                  // B', X's sibling, after X returned
        }
        {
            FedMarkScope a(cur, true, 3);
            {
                FedMarkScope a2(cur, false, 5);          // an unfed by-name call inside A
                viaUnfed = script(10);                   // B consumes A's mark through it
            }
            unfedSibling = script(10);                   // A2's scope never brings it back
        }
        {
            FedMarkScope a(cur, true, 3);
            {
                FedMarkScope a2(cur, true, 5);           // a fed by-name call: a mark of its own
                ownMark = script(10);
            }
            outerSibling = script(10);                   // A's mark was untouched: this call consumes it
        }
        {
            FedMarkScope a2(cur, false, 5);              // unfed, with no outer mark
            unfedNoMark = script(10);
        }
        // Negative control: restoring the slot by value (the scope this
        // replaced) brings the consumed mark back for B'.
        int byValue = 3;
        { const int prev = byValue; ConsumeFedMark(byValue, true); byValue = prev; }
        const bool byValueBack = ConsumeFedMark(byValue, true);
        check("watch/a_consumed_by_name_mark_never_comes_back",
              direct && !directSibling && cleared && viaScript && !scriptSibling && viaUnfed && !unfedSibling
              && ownMark && outerSibling && !unfedNoMark && cur == nullptr && byValueBack);
    }

    std::cout << (failures ? "RESULT FAIL " + std::to_string(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
