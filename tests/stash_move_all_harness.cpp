// Behavioral harness for the "Move all into the stash" core (ForgePact #68).
//
// The Python runner splices the REAL plugin/include/ForgePact/StashMoveAllMod.hpp
// in at the marker below (its #pragma and #include lines removed). The header is
// game-independent by contract - it names no runtime interface - so it compiles
// here with no runtime stub.
//
// The core decides what the player build's adapter acts on: the switch (off by
// default), the hotkey's edge, the plan over the shown bag tab's occupied cells
// (each item once, row-major, a multi-cell item by its first cell), the route
// per item for the shown stash tab (a grid tab through the tab routine, or a
// stack when one of the item's identity is there; a special tab only for its
// own class, through the stack routine; Unique and anything unknown refused),
// the outcome of each item from the adapter's re-reads (moved, skipped with the
// game's answer, or unconfirmed), the rule that a skip continues and an
// unconfirmed item stops the run and turns the mod off for the session, and
// the lines.
//
// Baseline: off by default; off plans nothing whatever the bag holds; a key
// press with the switch off is nothing. Target: every item of a mixed tab is
// planned once in order; a tab that answers "no room" leaves the rest in the
// bag and the run keeps asking; a stackable plans a stack when a stack of its
// identity exists; a refused item is skipped and the next continues; an
// unconfirmed item stops and turns the mod off; the lines name what moved and
// what stayed.
//
// Red first: with these scenarios written and the header holding only its
// namespace, this file did not compile. The first error line was
// `error C2039: 'StashMoveAllMod': is not a member of 'ForgePact'` (on the
// first using-declaration), 2026-09-28.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// PRODUCTION_STASHMOVEALL

using ForgePact::StashMoveAllMod;
using ForgePact::StashMoveCell;
using ForgePact::StashMoveItem;
using ForgePact::StashMoveOutcome;
using ForgePact::StashMovePlan;
using ForgePact::StashMoveReport;
using ForgePact::StashMoveResult;
using ForgePact::StashMoveRoute;
using ForgePact::StashMoveTab;
using ForgePact::StashMoveTally;
using ForgePact::StashMoveView;

static int g_Failures = 0;

static void Check(const std::string& label, bool ok, const std::string& detail)
{
    if (ok) std::cout << "PASS " << label << "\n";
    else { std::cout << "FAIL " << label << " " << detail << "\n"; ++g_Failures; }
}

static StashMoveCell Cell(int x, int y, const std::string& key, int itemClass,
                          bool stackable = false, long long count = 1, bool destinationHasStack = false)
{
    StashMoveCell c;
    c.x = x;
    c.y = y;
    c.key = key;
    c.itemClass = itemClass;
    c.stackable = stackable;
    c.count = count;
    c.destinationHasStack = destinationHasStack;
    return c;
}

// A bag page as the adapter would read it: one 2x2 non-stackable (four cells,
// one key), a ring, a material stack and a socketable, given out of order.
static StashMoveView MixedView(int stashTab)
{
    StashMoveView v;
    v.stashListed = true;
    v.bagTab = 0;
    v.stashTab = stashTab;
    v.cells = {
        Cell(4, 1, "0-0-40-14", 14, true, 7),
        Cell(1, 0, "0-0-10-18", 18),
        Cell(2, 0, "0-0-10-18", 18),
        Cell(1, 1, "0-0-10-18", 18),
        Cell(2, 1, "0-0-10-18", 18),
        Cell(0, 0, "0-0-20-7", 7),
        Cell(0, 2, "0-0-30-15", 15, true, 3),
    };
    return v;
}

static std::string Keys(const StashMovePlan& plan)
{
    std::string s;
    for (const StashMoveItem& i : plan.items) s += (s.empty() ? "" : ",") + i.cell.key;
    return s;
}

// The adapter's report for a cell move the game placed and the re-read confirmed.
static StashMoveReport PlacedCell(int x, int y)
{
    StashMoveReport r;
    r.answered = true;
    r.accepted = true;
    r.answer = "success=true";
    r.sourceHasKey = 0;
    r.destinationHasKey = 1;
    r.destinationX = x;
    r.destinationY = y;
    return r;
}

// The adapter's report for an answer the game gave before changing anything.
static StashMoveReport Refused(const std::string& answer)
{
    StashMoveReport r;
    r.answered = true;
    r.accepted = false;
    r.answer = answer;
    r.sourceHasKey = 1;
    r.destinationHasKey = 0;
    return r;
}

static StashMoveReport Stacked(long long before, long long after, int sourceHasKey = 0)
{
    StashMoveReport r;
    r.answered = true;
    r.accepted = true;
    r.answer = "true";
    r.sourceHasKey = sourceHasKey;
    r.stackBefore = before;
    r.stackAfter = after;
    return r;
}

static bool Has(const std::vector<std::string>& lines, const std::string& text)
{
    for (const std::string& l : lines) if (l == text) return true;
    return false;
}

static std::string Joined(const std::vector<std::string>& lines)
{
    std::string s;
    for (const std::string& l : lines) s += "[" + l + "]";
    return s;
}

// ---- baseline: off is vanilla ----------------------------------------------

static void BaselineOffByDefault()
{
    StashMoveAllMod mod;
    Check("baseline/off_by_default", !mod.IsEnabled() && !mod.OffThisSession()
          && mod.StateLine() == "stashmoveall: state=off key=F4",
          mod.StateLine());
}

static void BaselineOffPlansNothing()
{
    StashMoveAllMod mod;
    bool ok = true;
    std::string detail;
    for (int tab : {0, 1, 19, -4, -2, -5, 42}) {
        StashMovePlan p = mod.Plan(MixedView(tab));
        if (!p.refused || p.reason != "off" || !p.items.empty()) { ok = false; detail += " tab " + std::to_string(tab); }
    }
    StashMoveView empty = MixedView(1);
    empty.cells.clear();
    StashMovePlan p = mod.Plan(empty);
    ok = ok && p.refused && p.reason == "off";
    ok = ok && StashMoveAllMod::RefusalLine("stashmoveall", "off") == "stashmoveall: refused - off; nothing was called";
    // Negative control: the same view, on, plans every item.
    mod.SetEnabled(true);
    StashMovePlan on = mod.Plan(MixedView(1));
    ok = ok && !on.refused && on.items.size() == 4;
    Check("baseline/off_plans_nothing_whatever_the_bag_holds", ok, detail + " on=" + Keys(on));
}

static void BaselineKeyOffIsNothing()
{
    StashMoveAllMod mod;
    bool ok = true;
    for (int i = 0; i < 3; ++i) ok = ok && !mod.KeyEdge(true, true, true);
    ok = ok && !mod.KeyEdge(false, true, true) && !mod.KeyEdge(true, true, true);
    // Negative control: on, the same presses start one run per press. The key
    // was last seen held while off, so turning on starts nothing until it is
    // released and pressed again.
    mod.SetEnabled(true);
    bool heldAtSwitchOn = mod.KeyEdge(true, true, true);
    mod.KeyEdge(false, true, true);
    bool first = mod.KeyEdge(true, true, true);
    bool held = mod.KeyEdge(true, true, true);
    bool released = mod.KeyEdge(false, true, true);
    bool again = mod.KeyEdge(true, true, true);
    mod.KeyEdge(false, true, true);
    bool background = mod.KeyEdge(true, false, true);
    mod.KeyEdge(false, true, true);
    bool noStash = mod.KeyEdge(true, true, false);
    Check("baseline/key_press_with_the_switch_off_is_nothing",
          ok && !heldAtSwitchOn && first && !held && !released && again && !background && !noStash,
          "off=" + std::to_string(ok) + " heldAtSwitchOn=" + std::to_string(heldAtSwitchOn) + " first=" + std::to_string(first) + " held=" + std::to_string(held)
          + " again=" + std::to_string(again) + " background=" + std::to_string(background)
          + " noStash=" + std::to_string(noStash));
}

// ---- target: the plan -----------------------------------------------------

static void TargetMixedTabPlannedOnceInOrder()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMovePlan p = mod.Plan(MixedView(1));
    bool ok = !p.refused && Keys(p) == "0-0-20-7,0-0-10-18,0-0-40-14,0-0-30-15";
    // The 2x2 item is planned by its first cell, (1, 0).
    ok = ok && p.items.size() == 4 && p.items[1].cell.x == 1 && p.items[1].cell.y == 0;
    ok = ok && p.bagTab == 0 && p.stashTab == 1;
    for (const StashMoveItem& i : p.items) ok = ok && i.route == StashMoveRoute::Cell;
    Check("target/mixed_tab_every_item_planned_once_in_order", ok, Keys(p));
}

static void TargetRefusedRuns()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMoveView v = MixedView(1);
    bool ok = true;
    std::string detail;
    auto expect = [&](const StashMoveView& view, const std::string& reason) {
        StashMovePlan p = mod.Plan(view);
        if (!p.refused || p.reason != reason || !p.items.empty()) { ok = false; detail += " want " + reason + " got " + p.reason; }
    };
    StashMoveView noStash = v; noStash.stashListed = false; expect(noStash, "no stash window");
    StashMoveView unique = v; unique.stashTab = -5; expect(unique, "unsupported stash tab -5");
    StashMoveView other = v; other.stashTab = 20; expect(other, "unsupported stash tab 20");
    StashMoveView bagSub = v; bagSub.bagTab = -4; expect(bagSub, "unsupported bag tab -4");
    StashMoveView unknown = v; unknown.stashTab = StashMoveAllMod::kUnreadTab; expect(unknown, "no shown tab");
    StashMoveView empty = v; empty.cells.clear(); expect(empty, "nothing to move");
    ok = ok && mod.IsEnabled();
    Check("target/refused_runs_name_the_reason_and_plan_nothing", ok, detail);
}

static void TargetStackablePlansStack()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    bool ok = true;
    std::string detail;
    // A grid tab: a stackable with a stack of its identity there goes to the
    // stack; without one, to a cell; a non-stackable never to a stack.
    StashMoveView grid;
    grid.stashListed = true; grid.bagTab = 0; grid.stashTab = 3;
    grid.cells = { Cell(0, 0, "a", 14, true, 5, true), Cell(1, 0, "b", 14, true, 5, false), Cell(2, 0, "c", 18, false, 1, true) };
    StashMovePlan g = mod.Plan(grid);
    ok = ok && g.items.size() == 3 && g.items[0].route == StashMoveRoute::Stack
        && g.items[1].route == StashMoveRoute::Cell && g.items[2].route == StashMoveRoute::Cell;
    // The Materials tab takes class 14 through the stack routine and nothing
    // else; the Socketable tab class 15. Another class is planned as a skip
    // that calls nothing.
    StashMoveView mats = MixedView(-4);
    StashMovePlan m = mod.Plan(mats);
    for (const StashMoveItem& i : m.items) {
        bool mine = i.cell.itemClass == 14;
        if (mine != (i.route == StashMoveRoute::Stack)) { ok = false; detail += " mats " + i.cell.key; }
        if (!mine && (i.route != StashMoveRoute::None || i.refusal != "not taken by the Materials tab")) { ok = false; detail += " matsref " + i.refusal; }
    }
    StashMovePlan s = mod.Plan(MixedView(-2));
    for (const StashMoveItem& i : s.items) {
        bool mine = i.cell.itemClass == 15;
        if (mine != (i.route == StashMoveRoute::Stack)) { ok = false; detail += " sock " + i.cell.key; }
        if (!mine && i.refusal != "not taken by the Socketable tab") { ok = false; detail += " sockref " + i.refusal; }
    }
    ok = ok && StashMoveAllMod::TabOf(0) == StashMoveTab::Grid && StashMoveAllMod::TabOf(19) == StashMoveTab::Grid
        && StashMoveAllMod::TabOf(-4) == StashMoveTab::Materials && StashMoveAllMod::TabOf(-2) == StashMoveTab::Socketable
        && StashMoveAllMod::TabOf(-5) == StashMoveTab::Unique && StashMoveAllMod::TabOf(-3) == StashMoveTab::Unsupported;
    // A skip planned before any call is a skip, and the run goes on.
    StashMoveTally t = mod.Begin(m);
    StashMoveResult r = StashMoveAllMod::NotAttempted(m.items[0]);
    ok = ok && r.outcome == StashMoveOutcome::Skipped && mod.Record(t, r) && mod.IsEnabled();
    Check("target/stackable_plans_a_stack_when_its_identity_is_there", ok, detail);
}

// ---- target: the outcome --------------------------------------------------

static void TargetNoRoomLeavesTheRest()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMoveView v;
    v.stashListed = true; v.bagTab = 2; v.stashTab = 5;
    v.cells = { Cell(0, 0, "k1", 18), Cell(1, 0, "k2", 18), Cell(2, 0, "k3", 18) };
    StashMovePlan p = mod.Plan(v);
    StashMoveTally t = mod.Begin(p);
    bool ok = p.items.size() == 3;
    ok = ok && mod.Record(t, StashMoveAllMod::Decide(p.items[0], PlacedCell(4, 2)));
    ok = ok && mod.Record(t, StashMoveAllMod::Decide(p.items[1], Refused("success=false")));
    ok = ok && mod.Record(t, StashMoveAllMod::Decide(p.items[2], Refused("success=false")));
    ok = ok && t.moved == 1 && t.skipped == 2 && !t.stopped && mod.IsEnabled();
    std::string sum = StashMoveAllMod::SummaryLine(t);
    ok = ok && sum == "stashmoveall: moved 1 of 3 from bag tab 2 to stash tab 5; skipped 2";
    Check("target/no_room_skips_and_the_rest_stay_in_the_bag", ok, sum + " " + Joined(t.lines));
}

static void TargetRefusedItemSkippedNextContinues()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMovePlan p = mod.Plan(MixedView(1));
    StashMoveTally t = mod.Begin(p);
    StashMoveResult first = StashMoveAllMod::Decide(p.items[0], Refused("undefined"));
    bool ok = first.outcome == StashMoveOutcome::Skipped && first.answer == "undefined";
    ok = ok && mod.Record(t, first);
    StashMoveResult second = StashMoveAllMod::Decide(p.items[1], PlacedCell(0, 0));
    ok = ok && second.outcome == StashMoveOutcome::Moved && second.route == StashMoveRoute::Cell;
    ok = ok && mod.Record(t, second) && t.moved == 1 && t.skipped == 1 && mod.IsEnabled();
    Check("target/refused_item_is_skipped_and_the_next_continues", ok, Joined(t.lines));
}

static void TargetConfirmationRule()
{
    StashMoveItem cellItem;
    cellItem.cell = Cell(0, 0, "k", 18);
    cellItem.route = StashMoveRoute::Cell;
    StashMoveItem stackItem;
    stackItem.cell = Cell(0, 0, "m", 14, true, 7, true);
    stackItem.route = StashMoveRoute::Stack;
    bool ok = true;
    std::string detail;
    auto want = [&](const std::string& name, const StashMoveItem& item, const StashMoveReport& r, StashMoveOutcome o) {
        StashMoveResult res = StashMoveAllMod::Decide(item, r);
        if (res.outcome != o) { ok = false; detail += " " + name; }
    };
    want("cell ok", cellItem, PlacedCell(3, 1), StashMoveOutcome::Moved);
    StashMoveReport stillThere = PlacedCell(3, 1); stillThere.sourceHasKey = 1;
    want("cell source kept", cellItem, stillThere, StashMoveOutcome::Unconfirmed);
    StashMoveReport notThere = PlacedCell(3, 1); notThere.destinationHasKey = 0;
    want("cell dest missing", cellItem, notThere, StashMoveOutcome::Unconfirmed);
    StashMoveReport unread = PlacedCell(3, 1); unread.destinationHasKey = -1;
    want("cell dest unread", cellItem, unread, StashMoveOutcome::Unconfirmed);
    want("stack ok", stackItem, Stacked(10, 17), StashMoveOutcome::Moved);
    want("stack new", stackItem, Stacked(0, 7), StashMoveOutcome::Moved);
    want("stack wrong amount", stackItem, Stacked(10, 16), StashMoveOutcome::Unconfirmed);
    want("stack source kept", stackItem, Stacked(10, 17, 1), StashMoveOutcome::Unconfirmed);
    want("stack unread", stackItem, Stacked(-1, 17), StashMoveOutcome::Unconfirmed);
    // A refusal is a skip only when both sides read unchanged.
    want("refusal", cellItem, Refused("success=false"), StashMoveOutcome::Skipped);
    StashMoveReport halfRefused = Refused("success=false"); halfRefused.destinationHasKey = 1;
    want("refusal but placed", cellItem, halfRefused, StashMoveOutcome::Unconfirmed);
    StashMoveReport refusedUnread = Refused("success=false"); refusedUnread.sourceHasKey = -1;
    want("refusal unread", cellItem, refusedUnread, StashMoveOutcome::Unconfirmed);
    StashMoveReport stackRefused = Refused("false"); stackRefused.destinationHasKey = -1;
    stackRefused.stackBefore = 10; stackRefused.stackAfter = 10;
    want("stack refusal", stackItem, stackRefused, StashMoveOutcome::Skipped);
    StashMoveReport stackRefusedMoved = stackRefused; stackRefusedMoved.stackAfter = 11;
    want("stack refusal but rose", stackItem, stackRefusedMoved, StashMoveOutcome::Unconfirmed);
    StashMoveReport notDispatched; notDispatched.answered = false; notDispatched.answer = "not dispatched";
    notDispatched.sourceHasKey = 1; notDispatched.destinationHasKey = 0;
    want("not dispatched", cellItem, notDispatched, StashMoveOutcome::Skipped);
    Check("target/moved_only_when_both_sides_confirm", ok, detail);
}

static void TargetUnconfirmedStopsAndTurnsOff()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMovePlan p = mod.Plan(MixedView(1));
    StashMoveTally t = mod.Begin(p);
    bool ok = mod.Record(t, StashMoveAllMod::Decide(p.items[0], PlacedCell(0, 0)));
    StashMoveReport half = PlacedCell(1, 0);
    half.sourceHasKey = 1;
    StashMoveResult loss = StashMoveAllMod::Decide(p.items[1], half);
    ok = ok && loss.outcome == StashMoveOutcome::Unconfirmed;
    bool cont = mod.Record(t, loss);
    ok = ok && !cont && t.stopped && !mod.IsEnabled() && mod.OffThisSession();
    ok = ok && !mod.SetEnabled(true) && !mod.IsEnabled();
    ok = ok && mod.SetEnabled(false);
    ok = ok && Has(t.lines, "stashmoveall: off for this session - item 0-0-10-18: the game answered success=true "
                            "but the bag cell still holds it; turn it on again after restarting the game");
    // The rest of the plan is not attempted, and a new run is refused.
    StashMovePlan after = mod.Plan(MixedView(1));
    ok = ok && after.refused && after.reason == "off for this session";
    ok = ok && StashMoveAllMod::SummaryLine(t) == "stashmoveall: moved 1 of 4 from bag tab 0 to stash tab 1; skipped 0; stopped";
    ok = ok && !mod.KeyEdge(true, true, true);
    Check("target/unconfirmed_item_stops_the_run_and_turns_the_mod_off", ok,
          StashMoveAllMod::SummaryLine(t) + " " + Joined(t.lines));
}

static void TargetLines()
{
    StashMoveAllMod mod;
    bool ok = mod.SwitchLine(true) == "stashmoveall: on"
        && mod.SwitchLine(false) == "stashmoveall: off - the stash and bag are unchanged";
    mod.SetEnabled(true);
    ok = ok && mod.StateLine() == "stashmoveall: state=on key=F4";
    StashMovePlan p = mod.Plan(MixedView(-4));
    StashMoveTally t = mod.Begin(p);
    // Planned order: 0-0-20-7 (class 7), 0-0-10-18, 0-0-40-14 (stack), 0-0-30-15.
    for (const StashMoveItem& i : p.items) {
        if (i.route == StashMoveRoute::None) mod.Record(t, StashMoveAllMod::NotAttempted(i));
        else mod.Record(t, StashMoveAllMod::Decide(i, Stacked(3, 10)));
    }
    ok = ok && Has(t.lines, "stashmoveall: item 0-0-40-14 -> stack")
        && Has(t.lines, "stashmoveall: item 0-0-20-7 -> skipped: not taken by the Materials tab")
        && Has(t.lines, "stashmoveall: item 0-0-30-15 -> skipped: not taken by the Materials tab");
    ok = ok && StashMoveAllMod::SummaryLine(t) == "stashmoveall: moved 1 of 4 from bag tab 0 to stash tab -4; skipped 3";
    StashMoveItem one;
    one.cell = Cell(2, 3, "0-0-9-18", 18);
    one.route = StashMoveRoute::Cell;
    StashMoveResult moved = StashMoveAllMod::Decide(one, PlacedCell(5, 6));
    ok = ok && t.lines.size() == 4 && StashMoveAllMod::ItemLine(moved) == "stashmoveall: item 0-0-9-18 -> cell 5,6";
    ok = ok && StashMoveAllMod::SingleLine(moved) == "stashmove: moved 0-0-9-18 -> cell 5,6";
    StashMoveResult no = StashMoveAllMod::Decide(one, Refused("success=false"));
    ok = ok && StashMoveAllMod::SingleLine(no) == "stashmove: not-taken - success=false; the item stays in the bag";
    ok = ok && StashMoveAllMod::RefusalLine("stashmove", "no stash window") == "stashmove: refused - no stash window; nothing was called";
    Check("target/lines_name_what_moved_and_what_stayed", ok, Joined(t.lines) + " " + StashMoveAllMod::SummaryLine(t));
}

int main()
{
    BaselineOffByDefault();
    BaselineOffPlansNothing();
    BaselineKeyOffIsNothing();
    TargetMixedTabPlannedOnceInOrder();
    TargetRefusedRuns();
    TargetStackablePlansStack();
    TargetNoRoomLeavesTheRest();
    TargetRefusedItemSkippedNextContinues();
    TargetConfirmationRule();
    TargetUnconfirmedStopsAndTurnsOff();
    TargetLines();
    std::cout << (g_Failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return g_Failures ? 1 : 0;
}
