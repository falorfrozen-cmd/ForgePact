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
// answered. Target: with `rng <value> <count>`, only a call whose self is a
// machine is answered, `count` times and then the lever is off; another
// object's call, or another self inside a machine's event, is untouched; a
// lever no machine call reached is named INERT; the trace budget and the
// status text read back every counter.
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
static RngDecision rng(Probe& p, Builtin b, int self, bool inEvent = false, int argc = 1)
{
    return p.DecideRng(b, [self]() { ++g_SelfReads; return self; }, inEvent, argc);
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
        check("table/trace_budget_is_named", kTraceLinesPerRow == 40 && kTraceLinesPerKey == 8
            && kTraceLinesPerKey < kTraceLinesPerRow && kRngMinCount == 1 && kRngMaxCount == 50);
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
        p.SetRng(98, 5);
        const RngDecision d = rng(p, Builtin::Irandom, kMachine);
        check("predicate/unresolved_machine_answers_nothing", !d.answer && d.seen == Seen::Other);
    }

    // ---- target: the lever ------------------------------------------------------
    {
        Probe p = make();
        check("target/set_rng", p.SetRng(98, 2) && p.RngOn() && p.Active() && p.RngRemaining() == 2);
        g_SelfReads = 0;
        const RngDecision other = rng(p, Builtin::Irandom, kEnemy);
        const RngDecision player = rng(p, Builtin::IrandomRange, kPlayer);
        const RngDecision inEvent = rng(p, Builtin::Random, kPlayer, true);
        check("target/another_objects_call_is_untouched", !other.answer && !player.answer && other.seen == Seen::Other
            && p.RngRemaining() == 2 && p.RowCounters(BuiltinRowOf(Builtin::Irandom)).otherSelf == 1);
        check("target/another_self_inside_a_machine_event_is_untouched", !inEvent.answer && inEvent.seen == Seen::InEvent
            && p.RowCounters(BuiltinRowOf(Builtin::Random)).inEvent == 1);
        check("target/levered_reads_the_self", g_SelfReads == 3);
        const RngDecision a = rng(p, Builtin::Irandom, kMachine);
        check("target/machine_self_answered_with_the_value", a.answer && a.value == 98.0 && a.seen == Seen::Machine
            && p.RngRemaining() == 1 && p.RngOn());
        const RngDecision b = rng(p, Builtin::RandomRange, kMachine);
        check("target/count_calls_then_off", b.answer && b.value == 98.0 && !p.RngOn() && p.RngAnswered() == 2
            && p.RngRemaining() == 0);
        check("target/finished_is_read_once", p.TakeRngFinished() && !p.TakeRngFinished());
        const RngDecision c = rng(p, Builtin::Irandom, kMachine);
        check("target/after_count_the_real_one_again", !c.answer && !p.Active());
        check("target/answered_counted_per_row", p.RowCounters(BuiltinRowOf(Builtin::Irandom)).answered == 1
            && p.RowCounters(BuiltinRowOf(Builtin::RandomRange)).answered == 1);
    }
    {
        Probe p = make();
        p.SetRng(3, 5);
        bool none = true;
        for (Builtin b : kOtherRows) if (rng(p, b, kMachine).answer) none = false;
        check("target/instance_rows_never_answered", none && p.RngRemaining() == 5);
    }
    {
        // choose: the value names one of the call's own arguments.
        Probe p = make();
        p.SetRng(1, 1);
        const RngDecision in = rng(p, Builtin::Choose, kMachine, false, 3);
        check("target/choose_answers_an_argument_index", in.answer && in.value == 1.0 && !p.RngOn());
        p.SetRng(5, 1);
        const RngDecision out = rng(p, Builtin::Choose, kMachine, false, 3);
        check("target/choose_out_of_range_runs_the_original", !out.answer && p.RngOn() && p.RngRemaining() == 1
            && p.RngOutOfRange() == 1 && p.RowCounters(BuiltinRowOf(Builtin::Choose)).outOfRange == 1);
        p.SetRng(1.5, 1);
        const RngDecision frac = rng(p, Builtin::Choose, kMachine, false, 3);
        p.SetRng(-1, 1);
        const RngDecision neg = rng(p, Builtin::Choose, kMachine, false, 3);
        check("target/choose_fraction_or_negative_runs_the_original", !frac.answer && !neg.answer);
        // irandom takes any value: the lever is blunt on purpose.
        p.SetRng(-7.25, 1);
        const RngDecision any = rng(p, Builtin::Irandom, kMachine);
        check("target/value_rows_take_any_finite_value", any.answer && any.value == -7.25);
    }
    {
        Probe p = make();
        p.SetRng(4, 3);
        const bool refused = !p.SetRng(4, 0) && !p.SetRng(4, kRngMaxCount + 1) && !p.SetRng(std::nan(""), 1)
            && !p.SetRng(INFINITY, 1);
        check("target/out_of_range_set_refused_nothing_changed", refused && p.RngOn() && p.RngValue() == 4.0
            && p.RngRemaining() == 3);
        rng(p, Builtin::Irandom, kMachine);
        p.SetRng(9, 1);
        check("target/set_rng_starts_clean", p.RngAnswered() == 0 && p.RngRemaining() == 1 && p.RngValue() == 9.0);
    }

    // ---- inert ----------------------------------------------------------------------
    {
        Probe p = make();
        p.SetRng(98, 1);
        rng(p, Builtin::Irandom, kEnemy);
        rng(p, Builtin::Irandom, kPlayer, true);
        check("inert/armed_lever_no_machine_call_reached_is_named", p.Inert() && contains(p.RngLine(), "INERT")
            && contains(p.RngLine(), "gambaprobe: rng answered 0 of 1"), p.RngLine());
        rng(p, Builtin::Irandom, kMachine);
        check("inert/an_answer_ends_it", !p.Inert() && !contains(p.RngLine(), "INERT")
            && contains(p.RngLine(), "gambaprobe: rng answered 1 of 1"), p.RngLine());
        Probe q = make();
        q.SetRng(98, 1);
        q.RngOff();
        Probe r = make();
        check("inert/lever_off_or_never_set_is_not", !q.Inert() && !r.Inert() && !contains(r.RngLine(), "INERT"));
    }

    // ---- the trace budget -----------------------------------------------------------
    {
        Probe p = make();
        const int row = ScriptRowOf(0);
        int taken = 0;
        for (uint64_t i = 0; i < 100; ++i) if (p.TakeTraceLine(row, i, i)) ++taken;   // a new key each line
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

    // ---- the status text reads back every counter ---------------------------------
    {
        Probe p = make();
        p.SetArmed(true);
        observe(p, EventRowOf(Event::Create), kMachine);
        observe(p, EventRowOf(Event::Alarm9), kMachine);
        for (int i = 0; i < 30; ++i) observe(p, EventRowOf(Event::Step), kMachine);
        check("status/events_by_key", p.EventsText() == "create=1 alarm0=0 alarm9=1 step=30 cleanup=0", p.EventsText());
        p.SetRng(2, 2);
        const int row = BuiltinRowOf(Builtin::Choose);
        rng(p, Builtin::Choose, kMachine, false, 3);    // answered
        p.SetRng(9, 1);
        rng(p, Builtin::Choose, kMachine, false, 3);    // out of range
        rng(p, Builtin::Choose, kPlayer, true, 3);      // in event
        rng(p, Builtin::Choose, kEnemy, false, 3);      // other
        p.TakeTraceLine(row, 0, 1);
        const std::string text = p.RowText(row);
        check("status/row_reads_every_counter", text == "calls=4 machine-self=2 in-event=1 other-self=1 logged=1/40"
            " key-capped=0 answered=1 out-of-range=1", text);
        check("status/non_rng_rows_omit_the_lever_counters",
              p.RowText(EventRowOf(Event::Step)) == "calls=30 machine-self=30 in-event=0 other-self=0 logged=0/40 key-capped=0");
        const std::string line = p.StatusLine();
        check("status/line_sums_every_counter", line == "gambaprobe: on machine-object=4644 create=1 alarm0=0 alarm9=1"
            " step=30 cleanup=0 | calls=36 machine-self=34 in-event=1 other-self=1 logged=1 key-capped=0 spent-rows=0"
            " answered=1 out-of-range=1", line);
        check("status/rng_line", contains(p.RngLine(), "gambaprobe: rng answered 0 of 1 value=9 remaining=1 out-of-range=1"
            " lever=on"), p.RngLine());
        check("status/number_text", NumberText(98) == "98" && NumberText(-7.25) == "-7.25" && NumberText(0.5) == "0.5");
    }

    // ---- off --------------------------------------------------------------------------
    {
        Probe p = make();
        p.SetArmed(true);
        p.SetRng(98, 3);
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
