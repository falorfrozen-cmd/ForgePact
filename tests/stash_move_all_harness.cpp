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
// what stayed. Never overflow (the owner's 2026-09-28 rule, D4): a full shown
// tab calls nothing and leaves the item in the bag and every other tab as it
// was, against a stand-in routine that would spill into the next tab; a shown
// stash tab that changed during the move, or could not be re-read, is
// unconfirmed (the other tabs have no container readable by name, so the
// shown tab is the only one re-read; owner, 2026-09-28, "Accept").
//
// The routes Live 1e decided (docs/stash-move-research.md § Decision): a new
// material identity is placed in a cell of the Materials tab and a whole stack
// merges by its count (both measured by name); the Socketable tab and the
// bag's Socket view have no measured by-name shape and are refused. Each rule
// is pinned for the state recorded, with the other state as its negative
// control. The bag's Materials view is a source for the Materials tab only;
// a stash page from a bag sub-tab is refused (baseline).
//
// Red first: with these scenarios written and the header holding only its
// namespace, this file did not compile. The first error line was
// `error C2039: 'StashMoveAllMod': is not a member of 'ForgePact'` (on the
// first using-declaration), 2026-09-28. The D4 scenarios were written the same
// way, before the header had a room check or an other-tab read; the first error
// line was `error C2039: 'keyOnOtherTab': is not a member of
// 'ForgePact::StashMoveReport'`, 2026-09-28. The route and shown-tab scenarios
// (Phase B, B0) too, before the header had them; the first error line was
// `error C2039: 'StashMoveGrid': is not a member of 'ForgePact'` (on its
// using-declaration), 2026-09-28.
//
// The round-2 review (C0): an item's route was fixed by the plan, from the
// shown tab's stack sums read before the run, so a second bag item of one
// stackable identity the tab lacked went down the cell route after the first
// had made the stack, the game merged one unit and the bag cell stayed - a
// duplicate. The route is now decided again at the point of use from the sum
// re-read just before the item's call (RouteAtUse); a held modifier is no key
// edge (Alt+F4 must not start a run as the game closes); an owner step that
// did not take is unconfirmed; and after a loss the state line says so. The
// in-game button's decisions (whether its node should exist, and a press
// consumed once under the key's guard) are pinned here too. Written before
// the header had any of them; the first error line was `error C2039:
// 'StashMoveButtonStep': is not a member of 'ForgePact'` (on its
// using-declaration), 2026-09-28.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// PRODUCTION_STASHMOVEALL

using ForgePact::StashMoveAllMod;
using ForgePact::StashMoveButtonStep;
using ForgePact::StashMoveCell;
using ForgePact::StashMoveItem;
using ForgePact::StashMoveOutcome;
using ForgePact::StashMovePlan;
using ForgePact::StashMoveGrid;
using ForgePact::StashMoveReport;
using ForgePact::StashMoveResult;
using ForgePact::StashMoveRoute;
using ForgePact::StashMoveRoutes;
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
    r.shownTabChanged = 0;
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
    r.shownTabChanged = 0;
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
    r.shownTabChanged = 0;
    return r;
}

// A stash of grid tabs as the keys each tab holds and how many it has room
// for, and a stand-in for the game's own quick move as the static reading of
// the processor's tab walk describes it: into the tab it is given, or, when
// that tab is full, into the first tab in order that has room. The stand-in is
// what D4 guards against, so it is written to spill.
struct FakeStash {
    std::vector<std::vector<std::string>> tabs;
    std::vector<int>                      capacity;
};

static int FakeRoom(const FakeStash& s, int tab)
{
    return (int)s.tabs[tab].size() < s.capacity[tab] ? 1 : 0;
}

// The tab the stand-in placed the key on, or -1 when every tab is full.
static int FakeQuickMove(FakeStash& s, int tab, const std::string& key)
{
    if (FakeRoom(s, tab)) { s.tabs[tab].push_back(key); return tab; }
    for (int t = 0; t < (int)s.tabs.size(); ++t)
        if (FakeRoom(s, t)) { s.tabs[t].push_back(key); return t; }
    return -1;
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

static StashMoveRoutes Flipped(bool socketNew, bool socketMerge, bool newMaterial, bool wholeStackMerge);

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
    for (int i = 0; i < 3; ++i) ok = ok && !mod.KeyEdge(true, true, true, false);
    ok = ok && !mod.KeyEdge(false, true, true, false) && !mod.KeyEdge(true, true, true, false);
    // Negative control: on, the same presses start one run per press. The key
    // was last seen held while off, so turning on starts nothing until it is
    // released and pressed again.
    mod.SetEnabled(true);
    bool heldAtSwitchOn = mod.KeyEdge(true, true, true, false);
    mod.KeyEdge(false, true, true, false);
    bool first = mod.KeyEdge(true, true, true, false);
    bool held = mod.KeyEdge(true, true, true, false);
    bool released = mod.KeyEdge(false, true, true, false);
    bool again = mod.KeyEdge(true, true, true, false);
    mod.KeyEdge(false, true, true, false);
    bool background = mod.KeyEdge(true, false, true, false);
    mod.KeyEdge(false, true, true, false);
    bool noStash = mod.KeyEdge(true, true, false, false);
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
    StashMoveView bagSub = v; bagSub.bagTab = -4; expect(bagSub, "unsupported bag tab -4 for stash tab 1");
    StashMoveView socketTab = v; socketTab.stashTab = -2; expect(socketTab, "unsupported stash tab -2");
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
    // A stackable whose stack on the shown tab could not be read (a shared
    // page's entries answer on no map by name) is a skip, never "no stack";
    // a non-stackable does not need the read.
    StashMoveView unreadStack = grid;
    for (StashMoveCell& c : unreadStack.cells) { c.destinationHasStack = false; c.destinationStackRead = false; }
    StashMovePlan u = mod.Plan(unreadStack);
    ok = ok && u.items.size() == 3 && u.items[0].route == StashMoveRoute::None && u.items[1].route == StashMoveRoute::None
        && u.items[0].refusal == "its stack on the shown tab could not be read" && u.items[2].route == StashMoveRoute::Cell;
    // The Materials tab takes class 14 and nothing else: onto its stack when
    // its identity is there, else into a cell (newMaterialRoute). Another
    // class is planned as a skip that calls nothing.
    StashMoveView mats = MixedView(-4);
    mats.cells[0].destinationHasStack = true;
    StashMovePlan m = mod.Plan(mats);
    for (const StashMoveItem& i : m.items) {
        bool mine = i.cell.itemClass == 14;
        if (mine != (i.route == StashMoveRoute::Stack)) { ok = false; detail += " mats " + i.cell.key; }
        if (!mine && (i.route != StashMoveRoute::None || i.refusal != "not taken by the Materials tab")) { ok = false; detail += " matsref " + i.refusal; }
    }
    // The Socketable tab, with a measured merge path (not this build's
    // state, which refuses the tab), takes class 15 onto its stack.
    StashMoveView sock = MixedView(-2);
    sock.cells[6].destinationHasStack = true;
    StashMovePlan s = StashMoveAllMod::PlanWith(sock, Flipped(true, true, true, true), true);
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
    notDispatched.sourceHasKey = 1; notDispatched.destinationHasKey = 0; notDispatched.shownTabChanged = 0;
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
    ok = ok && !mod.KeyEdge(true, true, true, false);
    // The state line tells "turned itself off after a loss" apart from "off",
    // and names the loss; turning on again answers with the same reason.
    const std::string reason = "item 0-0-10-18: the game answered success=true but the bag cell still holds it";
    ok = ok && mod.OffReason() == reason
        && mod.StateLine() == "stashmoveall: state=off-for-this-session reason=" + reason
        && mod.OffForSessionLine() == "stashmoveall: off for this session - " + reason
                                      + "; turn it on again after restarting the game";
    // Negative control: switched off by hand, the state line is plain off.
    StashMoveAllMod byHand;
    byHand.SetEnabled(true);
    byHand.SetEnabled(false);
    ok = ok && byHand.StateLine() == "stashmoveall: state=off key=F4" && byHand.OffReason().empty();
    Check("target/unconfirmed_item_stops_the_run_and_turns_the_mod_off", ok,
          StashMoveAllMod::SummaryLine(t) + " " + Joined(t.lines));
}

// ---- target: never overflow (D4) ------------------------------------------

// One run the way the adapter drives it: for each planned item, the shown
// tab's room re-read first; no room is a skip with nothing called; otherwise
// the stand-in routine is called with the shown tab and the outcome decided
// from the re-reads of the shown tab and the bag.
static int RunAgainst(StashMoveAllMod& mod, const StashMovePlan& p, FakeStash& s,
                      std::vector<std::string>& bag, StashMoveTally& t)
{
    int calls = 0;
    for (const StashMoveItem& item : p.items) {
        StashMoveResult skip;
        if (!StashMoveAllMod::MayCall(item, FakeRoom(s, p.stashTab), skip)) {
            if (!mod.Record(t, skip)) break;
            continue;
        }
        ++calls;
        int landed = FakeQuickMove(s, p.stashTab, item.cell.key);
        StashMoveReport r;
        r.answered = true;
        r.accepted = landed >= 0;
        r.answer = landed >= 0 ? "success=true" : "success=false";
        if (landed >= 0) bag.erase(std::find(bag.begin(), bag.end(), item.cell.key));
        r.sourceHasKey = std::find(bag.begin(), bag.end(), item.cell.key) != bag.end() ? 1 : 0;
        const std::vector<std::string>& shown = s.tabs[p.stashTab];
        r.destinationHasKey = std::find(shown.begin(), shown.end(), item.cell.key) != shown.end() ? 1 : 0;
        r.destinationX = (int)shown.size() - 1;
        r.destinationY = 0;
        // Only the shown tab is re-read (no other has a container readable by
        // name); the stand-in never changes which tab is shown.
        r.shownTabChanged = 0;
        if (!mod.Record(t, StashMoveAllMod::Decide(item, r))) break;
    }
    return calls;
}

static void TargetFullShownTabKeepsItemInBag()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    // The shown tab (0) is full; tabs 1 and 2 have room.
    FakeStash s;
    s.tabs = { {"s1", "s2"}, {"s3"}, {} };
    s.capacity = { 2, 4, 4 };
    const std::vector<std::vector<std::string>> before = s.tabs;
    StashMoveView v;
    v.stashListed = true; v.bagTab = 0; v.stashTab = 0;
    v.cells = { Cell(0, 0, "k1", 18), Cell(1, 0, "k2", 18) };
    std::vector<std::string> bag = { "k1", "k2" };
    StashMovePlan p = mod.Plan(v);
    StashMoveTally t = mod.Begin(p);
    int calls = RunAgainst(mod, p, s, bag, t);
    bool ok = calls == 0 && t.moved == 0 && t.skipped == 2 && !t.stopped && mod.IsEnabled();
    ok = ok && s.tabs == before && bag == std::vector<std::string>({"k1", "k2"});
    ok = ok && Has(t.lines, "stashmoveall: item k1 -> skipped: no room on the shown tab")
        && Has(t.lines, "stashmoveall: item k2 -> skipped: no room on the shown tab");
    ok = ok && StashMoveAllMod::SummaryLine(t) == "stashmoveall: moved 0 of 2 from bag tab 0 to stash tab 0; skipped 2";
    std::string detail = "calls=" + std::to_string(calls) + " " + Joined(t.lines);

    // One free cell on the shown tab: the first item takes it, the second
    // stays in the bag, and the other tabs are still unchanged.
    StashMoveAllMod mod2;
    mod2.SetEnabled(true);
    FakeStash s2;
    s2.tabs = { {"s1"}, {"s3"}, {} };
    s2.capacity = { 2, 4, 4 };
    std::vector<std::string> bag2 = { "k1", "k2" };
    StashMoveTally t2 = mod2.Begin(p);
    int calls2 = RunAgainst(mod2, p, s2, bag2, t2);
    ok = ok && calls2 == 1 && t2.moved == 1 && t2.skipped == 1 && !t2.stopped;
    ok = ok && s2.tabs[0] == std::vector<std::string>({"s1", "k1"}) && s2.tabs[1] == before[1] && s2.tabs[2] == before[2];
    ok = ok && bag2 == std::vector<std::string>({"k2"});
    detail += " partial calls=" + std::to_string(calls2) + " " + Joined(t2.lines);

    // A room read that could not be made calls nothing either.
    StashMoveItem one = p.items[0];
    StashMoveResult unread;
    ok = ok && !StashMoveAllMod::MayCall(one, -1, unread) && unread.outcome == StashMoveOutcome::Skipped
        && unread.answer == "the shown tab's room could not be read";
    // Negative control: with room, the call goes ahead.
    StashMoveResult none;
    ok = ok && StashMoveAllMod::MayCall(one, 1, none);
    Check("target/full_shown_tab_keeps_item_in_bag_and_other_tabs_unchanged", ok, detail);
}

static void TargetShownTabChangedOrUnreadIsUnconfirmed()
{
    // After each call the adapter re-reads UI_Stash_obj.stashTabSelected: the
    // tab on show must still be the planned one. Moved off it (1), or not
    // readable (-1), the outcome cannot be confirmed on the shown tab's own
    // array, whatever the game answered: the run stops and the mod turns off.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMoveItem item;
    item.cell = Cell(0, 0, "k1", 18);
    item.route = StashMoveRoute::Cell;
    StashMoveReport changed = PlacedCell(0, 0);
    changed.shownTabChanged = 1;
    StashMoveResult res = StashMoveAllMod::Decide(item, changed);
    bool ok = res.outcome == StashMoveOutcome::Unconfirmed
        && res.answer == "the game answered success=true but the shown stash tab changed during the move";
    StashMoveReport unread = PlacedCell(0, 0);
    unread.shownTabChanged = -1;
    StashMoveResult u = StashMoveAllMod::Decide(item, unread);
    ok = ok && u.outcome == StashMoveOutcome::Unconfirmed
        && u.answer == "the game answered success=true but the shown stash tab could not be re-read";
    // A refusal is not a skip when the tab on show moved under it.
    StashMoveReport refusedMoved = Refused("success=false");
    refusedMoved.shownTabChanged = 1;
    ok = ok && StashMoveAllMod::Decide(item, refusedMoved).outcome == StashMoveOutcome::Unconfirmed;
    // A report whose shown-tab read was never made is not "unchanged".
    StashMoveReport never = PlacedCell(0, 0);
    never.shownTabChanged = StashMoveReport().shownTabChanged;
    ok = ok && never.shownTabChanged == -1 && StashMoveAllMod::Decide(item, never).outcome == StashMoveOutcome::Unconfirmed;
    StashMoveTally t;
    ok = ok && !mod.Record(t, res) && t.stopped && !mod.IsEnabled() && mod.OffThisSession();
    // Negative control: the tab unchanged and the key at the answer's cell is moved.
    ok = ok && StashMoveAllMod::Decide(item, PlacedCell(0, 0)).outcome == StashMoveOutcome::Moved;
    Check("target/shown_tab_changed_or_unread_is_unconfirmed", ok, res.answer + " " + u.answer + " " + Joined(t.lines));
}

// ---- the routes Live 1e decided ------------------------------------------

// The measured routes with one rule turned the other way: the negative
// control each route scenario keeps beside the state A'8 recorded.
static StashMoveRoutes Flipped(bool socketNew, bool socketMerge, bool newMaterial, bool wholeStackMerge)
{
    StashMoveRoutes r;
    r.socketNew = socketNew;
    r.socketMerge = socketMerge;
    r.newMaterial = newMaterial;
    r.wholeStackMerge = wholeStackMerge;
    return r;
}

static StashMoveView MaterialsView(int bagTab)
{
    StashMoveView v;
    v.stashListed = true;
    v.bagTab = bagTab;
    v.stashTab = -4;
    v.cells = {
        Cell(0, 0, "0-0-71-14", 14, true, 15, true),    // its identity is on the tab
        Cell(1, 0, "0-0-72-14", 14, true, 934, false),  // a new identity
        Cell(2, 0, "0-0-73-14", 14, true, 1, true),     // one unit onto its stack
    };
    return v;
}

static void BaselineGridTabFromBagSubTabIsRefused()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    bool ok = true;
    std::string detail;
    for (int stashTab : {0, 1, 19}) {
        for (int bagTab : {-4, -2}) {
            StashMoveView v = MixedView(stashTab);
            v.bagTab = bagTab;
            StashMovePlan p = mod.Plan(v);
            // The Materials view is a source, for the Materials tab only; the
            // Socket view is none at all while no socketable path is measured.
            const std::string want = bagTab == -4
                ? "unsupported bag tab -4 for stash tab " + std::to_string(stashTab)
                : std::string("unsupported bag tab -2");
            if (!p.refused || p.reason != want || !p.items.empty()) { ok = false; detail += " want " + want + " got " + p.reason; }
        }
    }
    // Another sub-tab (Key, Tarot, Relic, say) is no source anywhere.
    StashMoveView key = MaterialsView(-3);
    StashMovePlan k = mod.Plan(key);
    ok = ok && k.refused && k.reason == "unsupported bag tab -3";
    // Negative control: a bag page (0..4) is a source for a stash page.
    StashMoveView page = MixedView(3);
    page.bagTab = 4;
    ok = ok && !mod.Plan(page).refused;
    Check("baseline/grid_tab_destination_from_a_bag_sub_tab_is_refused", ok, detail + " key=" + k.reason);
}

static void TargetBagMaterialsViewFeedsTheMaterialsTab()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMovePlan p = mod.Plan(MaterialsView(-4));
    bool ok = !p.refused && p.items.size() == 3 && p.bagTab == -4 && p.stashTab == -4;
    // A bag page feeds the Materials tab too.
    StashMovePlan page = mod.Plan(MaterialsView(0));
    ok = ok && !page.refused && page.items.size() == 3;
    Check("target/bag_materials_view_feeds_the_materials_tab", ok, p.reason + " " + Keys(p));
}

static void TargetNewMaterialIdentityIsPlacedInACell()
{
    // newMaterialRoute: byname (Live 1e byname-material-new): a class-14 item
    // whose identity has no stack on the Materials tab is placed in a cell of
    // that tab, whole, through the tab placement.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMovePlan p = mod.Plan(MaterialsView(-4));
    bool ok = StashMoveAllMod::kMeasuredRoutes.newMaterial && p.items.size() == 3
        && p.items[1].cell.key == "0-0-72-14" && p.items[1].route == StashMoveRoute::Cell;
    StashMoveResult placed = StashMoveAllMod::Decide(p.items[1], PlacedCell(3, 16));
    ok = ok && placed.outcome == StashMoveOutcome::Moved && StashMoveAllMod::ItemLine(placed) == "stashmoveall: item 0-0-72-14 -> cell 3,16";
    // Negative control: with the route not measured it is a skip that calls
    // nothing, and the item stays in the bag.
    StashMovePlan off = StashMoveAllMod::PlanWith(MaterialsView(-4), Flipped(false, false, false, true), true);
    ok = ok && off.items.size() == 3 && off.items[1].route == StashMoveRoute::None
        && off.items[1].refusal == "a new kind stays in the bag";
    StashMoveResult skip;
    ok = ok && !StashMoveAllMod::MayCall(off.items[1], 1, skip) && skip.answer == "a new kind stays in the bag";
    Check("target/new_material_identity_is_placed_in_a_cell", ok, Keys(p) + " " + off.items[1].refusal);
}

static void TargetWholeStackMergesByItsCount()
{
    // wholeStackMerge: byname (Live 1e byname-merge-whole): an item of more
    // than one unit whose identity is on the tab merges by its whole count,
    // and it is moved only when the stack rose by exactly that count.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMovePlan p = mod.Plan(MaterialsView(-4));
    bool ok = StashMoveAllMod::kMeasuredRoutes.wholeStackMerge && p.items.size() == 3
        && p.items[0].route == StashMoveRoute::Stack && p.items[0].cell.count == 15
        && p.items[2].route == StashMoveRoute::Stack && p.items[2].cell.count == 1;
    ok = ok && StashMoveAllMod::Decide(p.items[0], Stacked(1, 16)).outcome == StashMoveOutcome::Moved;
    ok = ok && StashMoveAllMod::Decide(p.items[0], Stacked(1, 2)).outcome == StashMoveOutcome::Unconfirmed;
    // Negative control: with the whole-stack merge not measured, more than one
    // unit is a skip and one unit still merges by the one-unit shape.
    StashMovePlan off = StashMoveAllMod::PlanWith(MaterialsView(-4), Flipped(false, false, true, false), true);
    ok = ok && off.items.size() == 3 && off.items[0].route == StashMoveRoute::None
        && off.items[0].refusal == "whole-stack merge not measured" && off.items[2].route == StashMoveRoute::Stack;
    Check("target/whole_stack_merges_by_its_count", ok, Keys(p) + " " + off.items[0].refusal);
}

static void TargetSocketableTabAndSocketViewAreRefused()
{
    // socketRoute: neither path byname (Live 1e measured the Socketable tab by
    // hand only): the Socketable tab is refused as a destination and the
    // bag's Socket view as a source, each with nothing called.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    bool ok = !StashMoveAllMod::kMeasuredRoutes.socketNew && !StashMoveAllMod::kMeasuredRoutes.socketMerge;
    StashMovePlan dest = mod.Plan(MixedView(-2));
    ok = ok && dest.refused && dest.reason == "unsupported stash tab -2" && dest.items.empty();
    StashMoveView fromSocket = MixedView(-2);
    fromSocket.bagTab = -2;
    StashMovePlan src = mod.Plan(fromSocket);
    ok = ok && src.refused && src.reason == "unsupported stash tab -2";
    StashMoveView socketToMats = MaterialsView(-2);
    StashMovePlan mats = mod.Plan(socketToMats);
    ok = ok && mats.refused && mats.reason == "unsupported bag tab -2";
    ok = ok && StashMoveAllMod::RefusalLine("stashmoveall", dest.reason) == "stashmoveall: refused - unsupported stash tab -2; nothing was called";
    // Negative control: with a measured new path, the Socketable tab takes
    // class 15 from the bag's Socket view (a new identity into a cell, one
    // with a stack there a skip while its merge is not measured), and still
    // nothing else.
    StashMoveRoutes on = Flipped(true, false, true, true);
    StashMoveView sock;
    sock.stashListed = true; sock.bagTab = -2; sock.stashTab = -2;
    sock.cells = { Cell(0, 0, "0-0-31-15", 15, true, 1, false), Cell(1, 0, "0-0-118-15", 15, true, 1, true), Cell(2, 0, "0-0-5-7", 7) };
    StashMovePlan withRoute = StashMoveAllMod::PlanWith(sock, on, true);
    ok = ok && !withRoute.refused && withRoute.items.size() == 3
        && withRoute.items[0].route == StashMoveRoute::Cell
        && withRoute.items[1].route == StashMoveRoute::None && withRoute.items[1].refusal == "a socketable merge is not measured"
        && withRoute.items[2].route == StashMoveRoute::None && withRoute.items[2].refusal == "not taken by the Socketable tab";
    // With the merge path measured too, the stacked one merges.
    StashMovePlan both = StashMoveAllMod::PlanWith(sock, Flipped(true, true, true, true), true);
    ok = ok && both.items.size() == 3 && both.items[1].route == StashMoveRoute::Stack;
    // The Socket view is still no source for the Materials tab.
    ok = ok && StashMoveAllMod::PlanWith(socketToMats, on, true).reason == "unsupported bag tab -2 for stash tab -4";
    Check("target/socketable_tab_and_socket_view_are_refused", ok,
          dest.reason + " | " + src.reason + " | " + mats.reason + " | " + withRoute.reason);
}

static void TargetShownTabRoom()
{
    // The room check reads the shown tab's own cells: a free block of the
    // item's footprint (its cells in the bag), or nothing.
    StashMoveGrid g;
    g.rows = 3;
    g.cols = 3;
    g.filled = { 1, 0, 0,
                 1, 0, 1,
                 1, 1, 1 };
    bool ok = StashMoveAllMod::Room(g, 1, 1) == 1 && StashMoveAllMod::Room(g, 2, 1) == 1
        && StashMoveAllMod::Room(g, 1, 2) == 1 && StashMoveAllMod::Room(g, 2, 2) == 0
        && StashMoveAllMod::Room(g, 1, 3) == 0;
    StashMoveGrid full = g;
    full.filled.assign(9, 1);
    ok = ok && StashMoveAllMod::Room(full, 1, 1) == 0;
    // A grid that could not be read is never "has room" and never "full".
    StashMoveGrid unread;
    ok = ok && StashMoveAllMod::Room(unread, 1, 1) == -1;
    StashMoveGrid ragged = g;
    ragged.filled.pop_back();
    ok = ok && StashMoveAllMod::Room(ragged, 1, 1) == -1;
    // The footprint is the item's own cells in the bag: the 2x2 item of the
    // mixed view is 2 wide and 2 high, the others 1 by 1.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMovePlan p = mod.Plan(MixedView(1));
    ok = ok && p.items.size() == 4 && p.items[1].width == 2 && p.items[1].height == 2
        && p.items[0].width == 1 && p.items[0].height == 1;
    Check("target/room_is_a_free_block_of_the_items_footprint_on_the_shown_tab", ok, Keys(p));
}

static void TargetLines()
{
    StashMoveAllMod mod;
    bool ok = mod.SwitchLine(true) == "stashmoveall: on"
        && mod.SwitchLine(false) == "stashmoveall: off - the stash and bag are unchanged";
    mod.SetEnabled(true);
    ok = ok && mod.StateLine() == "stashmoveall: state=on key=F4";
    StashMoveView mats = MixedView(-4);
    mats.cells[0].destinationHasStack = true;
    StashMovePlan p = mod.Plan(mats);
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

// ---- the route at the point of use (round-2 review, C0) --------------------

// The report the round-2 adapter handed the core for the second item of an
// identity the shown tab lacked when the run was planned: it went down the
// planned cell route, StashAddToStack found the stack the first item had just
// made and merged one unit, nothing was placed and nothing cleared the bag cell.
static StashMoveReport MergedWherePlannedACell()
{
    StashMoveReport r;
    r.answered = true;
    r.accepted = true;
    r.answer = "StashAddToStack answered true (merged, not placed)";
    r.sourceHasKey = 1;
    r.destinationHasKey = 0;
    r.shownTabChanged = 0;
    return r;
}

// Two bag items of one stackable identity, and none of it on the shown tab
// when the run is planned: both are planned as cells. The first is placed and
// makes the stack; the second, re-read at the point of use, finds that stack
// and merges into it by its whole count, confirmed on the sum.
static bool SecondItemOfOneIdentity(int stashTab, int itemClass, std::string& detail)
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMoveView v;
    v.stashListed = true;
    v.bagTab = stashTab == StashMoveAllMod::kMaterialsTab ? StashMoveAllMod::kBagMaterialsView : 0;
    v.stashTab = stashTab;
    v.cells = { Cell(0, 0, "0-0-81-" + std::to_string(itemClass), itemClass, true, 5, false),
                Cell(1, 0, "0-0-82-" + std::to_string(itemClass), itemClass, true, 3, false) };
    StashMovePlan p = mod.Plan(v);
    bool ok = !p.refused && p.items.size() == 2 && p.items[0].route == StashMoveRoute::Cell
        && p.items[1].route == StashMoveRoute::Cell;
    StashMoveTally t = mod.Begin(p);
    // The first: its identity is still not on the tab (the sum re-reads 0).
    const StashMoveItem first = StashMoveAllMod::RouteAtUse(p.items[0], stashTab, 0);
    ok = ok && first.route == StashMoveRoute::Cell;
    StashMoveResult one;
    ok = ok && StashMoveAllMod::MayCall(first, 1, one);
    ok = ok && mod.Record(t, StashMoveAllMod::Decide(first, PlacedCell(0, 0)));
    // The second: the first's 5 units are on the tab now.
    const StashMoveItem second = StashMoveAllMod::RouteAtUse(p.items[1], stashTab, 5);
    ok = ok && second.route == StashMoveRoute::Stack && second.cell.count == 3;
    const StashMoveResult merged = StashMoveAllMod::Decide(second, Stacked(5, 8));
    ok = ok && merged.outcome == StashMoveOutcome::Moved && merged.route == StashMoveRoute::Stack
        && StashMoveAllMod::ItemLine(merged) == "stashmoveall: item " + second.cell.key + " -> stack";
    ok = ok && mod.Record(t, merged) && t.moved == 2 && !t.stopped && mod.IsEnabled();
    // Negative control: the planned route, decided before the run, cannot
    // confirm the merge the game did in its place - the round-2 duplicate.
    ok = ok && StashMoveAllMod::Decide(p.items[1], MergedWherePlannedACell()).outcome == StashMoveOutcome::Unconfirmed;
    // The whole-count rule still holds at the point of use: not measured,
    // more than one unit is a skip and one unit merges.
    const StashMoveRoutes noWhole = Flipped(false, false, true, false);
    const StashMoveItem many = StashMoveAllMod::RouteAtUse(p.items[1], stashTab, 5, noWhole);
    StashMoveItem unit = p.items[1];
    unit.cell.count = 1;
    ok = ok && many.route == StashMoveRoute::None && many.refusal == "whole-stack merge not measured"
        && StashMoveAllMod::RouteAtUse(unit, stashTab, 5, noWhole).route == StashMoveRoute::Stack;
    detail = Keys(p) + " second=" + std::to_string((int)second.route) + " " + Joined(t.lines);
    return ok;
}

static void TargetSecondItemMergesAtUseOnMaterials()
{
    std::string detail;
    bool ok = SecondItemOfOneIdentity(StashMoveAllMod::kMaterialsTab, StashMoveAllMod::kMaterialClass, detail);
    // The Materials tab's own rule at the point of use: a sum of 0 is a new
    // identity, placed only while newMaterialRoute is measured.
    StashMoveItem it;
    it.cell = Cell(0, 0, "0-0-83-14", 14, true, 4, false);
    it.route = StashMoveRoute::Cell;
    ok = ok && StashMoveAllMod::RouteAtUse(it, StashMoveAllMod::kMaterialsTab, 0, Flipped(false, false, false, true)).refusal
        == "a new kind stays in the bag";
    Check("target/second_item_of_one_identity_merges_at_the_point_of_use_on_materials", ok, detail);
}

static void TargetSecondItemMergesAtUseOnAPage()
{
    std::string detail;
    // A stackable key (class 12) on the personal page.
    bool ok = SecondItemOfOneIdentity(0, 12, detail);
    Check("target/second_item_of_one_identity_merges_at_the_point_of_use_on_a_page", ok, detail);
}

static void TargetUnreadableStackSumAtUseSkips()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMoveItem it;
    it.cell = Cell(0, 0, "0-0-84-12", 12, true, 2, false);
    it.route = StashMoveRoute::Cell;
    // The sum could not be re-read: a skip that calls nothing, whatever the
    // plan said, and the run goes on.
    const StashMoveItem atUse = StashMoveAllMod::RouteAtUse(it, 3, -1);
    bool ok = atUse.route == StashMoveRoute::None && atUse.refusal == "its stack on the shown tab could not be read";
    StashMoveResult skip;
    ok = ok && !StashMoveAllMod::MayCall(atUse, 1, skip) && skip.outcome == StashMoveOutcome::Skipped
        && skip.answer == "its stack on the shown tab could not be read";
    StashMoveItem stacked = it;
    stacked.route = StashMoveRoute::Stack;
    ok = ok && StashMoveAllMod::RouteAtUse(stacked, 3, -1).route == StashMoveRoute::None;
    // A count that did not read is never merged: the merge passes it.
    StashMoveItem noCount = it;
    noCount.cell.count = -1;
    ok = ok && StashMoveAllMod::RouteAtUse(noCount, 3, 4).route == StashMoveRoute::None;
    StashMoveTally t;
    ok = ok && mod.Record(t, skip) && t.skipped == 1 && !t.stopped && mod.IsEnabled();
    // Negative controls: a sum of 0 is a cell; a non-stackable needs no sum;
    // a planned skip stays a skip with its own reason.
    ok = ok && StashMoveAllMod::RouteAtUse(it, 3, 0).route == StashMoveRoute::Cell;
    StashMoveItem ring;
    ring.cell = Cell(1, 0, "0-0-85-7", 7);
    ring.route = StashMoveRoute::Cell;
    ok = ok && StashMoveAllMod::RouteAtUse(ring, 3, -1).route == StashMoveRoute::Cell;
    StashMoveItem refused;
    refused.cell = Cell(2, 0, "0-0-86-7", 7);
    refused.refusal = "not taken by the Materials tab";
    const StashMoveItem still = StashMoveAllMod::RouteAtUse(refused, StashMoveAllMod::kMaterialsTab, 3);
    ok = ok && still.route == StashMoveRoute::None && still.refusal == "not taken by the Materials tab";
    Check("target/unreadable_stack_sum_at_the_point_of_use_skips", ok, atUse.refusal + " " + Joined(t.lines));
}

static void TargetHeldModifierIsNoKeyEdge()
{
    // Alt+F4 closes the game window; with the stash open and the switch on it
    // must not start a run in the same moment. Any held modifier (Alt, Ctrl,
    // Shift) makes the press no edge, and releasing the modifier while the key
    // is still down does not turn the old press into one.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    mod.KeyEdge(false, true, true, false);
    bool withAlt = mod.KeyEdge(true, true, true, true);
    bool altReleased = mod.KeyEdge(true, true, true, false);
    mod.KeyEdge(false, true, true, false);
    // Negative control: the same press with no modifier is one run.
    bool plain = mod.KeyEdge(true, true, true, false);
    mod.KeyEdge(false, true, true, false);
    bool heldThrough = mod.KeyEdge(true, true, true, true);
    Check("target/a_held_modifier_is_no_key_edge", !withAlt && !altReleased && plain && !heldThrough,
          "withAlt=" + std::to_string(withAlt) + " altReleased=" + std::to_string(altReleased)
          + " plain=" + std::to_string(plain) + " heldThrough=" + std::to_string(heldThrough));
}

static void TargetOwnerStepThatDidNotTakeIsUnconfirmed()
{
    // A shared page and a new Materials identity end with the owner step 0 to
    // 9 after the bag cell is cleared; its measured signature is the key then
    // answering nothing on map 0. Not dispatched, or the key still answering
    // there, or the lookup not made, is unconfirmed - never moved.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMoveItem it;
    it.cell = Cell(0, 0, "0-0-87-18", 18);
    it.route = StashMoveRoute::Cell;
    auto owned = [](int dispatched, int onMap0) {
        StashMoveReport r = PlacedCell(2, 1);
        r.validateAnswer = "true";
        r.ownerStep = 1;
        r.ownerDispatched = dispatched;
        r.ownerAnswer = dispatched == 1 ? "undefined" : "not dispatched (ChangeItemOwner threw)";
        r.keyOnMap0 = onMap0;
        return r;
    };
    const StashMoveResult notRun = StashMoveAllMod::Decide(it, owned(0, 1));
    const StashMoveResult stayed = StashMoveAllMod::Decide(it, owned(1, 1));
    const StashMoveResult unread = StashMoveAllMod::Decide(it, owned(1, -1));
    bool ok = notRun.outcome == StashMoveOutcome::Unconfirmed
        && notRun.answer == "the game answered success=true and placed it, but the owner step was not dispatched: "
                            "not dispatched (ChangeItemOwner threw)"
        && stayed.outcome == StashMoveOutcome::Unconfirmed
        && stayed.answer == "the game answered success=true and placed it, but the key still answers on map 0 after "
                            "the owner step (ChangeItemOwner answered undefined)"
        && unread.outcome == StashMoveOutcome::Unconfirmed;
    // The answers enter the result: a moved item names what ValidateItem and
    // the owner step answered, so "ran and did nothing" is not success.
    const StashMoveResult took = StashMoveAllMod::Decide(it, owned(1, 0));
    ok = ok && took.outcome == StashMoveOutcome::Moved
        && took.answer == "success=true; ValidateItem answered true; ChangeItemOwner answered undefined";
    // Negative control: the personal page runs no owner step, so none is asked of it.
    ok = ok && StashMoveAllMod::Decide(it, PlacedCell(2, 1)).outcome == StashMoveOutcome::Moved;
    StashMoveTally t;
    ok = ok && !mod.Record(t, stayed) && t.stopped && mod.OffThisSession()
        && mod.StateLine().rfind("stashmoveall: state=off-for-this-session reason=item 0-0-87-18: ", 0) == 0;
    Check("target/owner_step_that_did_not_take_is_unconfirmed", ok,
          notRun.answer + " | " + stayed.answer + " | " + took.answer + " | " + mod.StateLine());
}

// ---- the in-game button (the core's side) ---------------------------------

static void BaselineButtonOffCreatesNothing()
{
    // Off, nothing is created whatever the game shows, a node left from an
    // earlier "on" is removed, and a press is not kept for later.
    StashMoveAllMod mod;
    bool ok = mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Keep
        && mod.ButtonStep(false, false, false, false) == StashMoveButtonStep::Keep
        && mod.ButtonStep(true, true, true, true) == StashMoveButtonStep::Remove;
    mod.NoteButtonPress();
    ok = ok && !mod.TakeButtonPress(true, true, false);
    // Negative control: on, the same scene creates it.
    mod.SetEnabled(true);
    ok = ok && mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Create
        && !mod.TakeButtonPress(true, true, false);   // the press made while off was dropped
    Check("baseline/button_off_creates_nothing", ok, "");
}

static void TargetButtonExistsOnlyWithTheStashAndSortListed()
{
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    // (stash listed, Sort listed, Sort visible, node exists) -> the step.
    bool ok = mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Create
        && mod.ButtonStep(true, true, true, true) == StashMoveButtonStep::Keep
        && mod.ButtonStep(false, true, true, true) == StashMoveButtonStep::Remove
        && mod.ButtonStep(false, false, false, false) == StashMoveButtonStep::Keep
        && mod.ButtonStep(true, false, false, true) == StashMoveButtonStep::Remove
        && mod.ButtonStep(true, false, false, false) == StashMoveButtonStep::Keep
        && mod.ButtonStep(true, true, false, true) == StashMoveButtonStep::Remove
        && mod.ButtonStep(true, true, false, false) == StashMoveButtonStep::Keep;
    // A loss turns the mod off for the session, and the node goes with it.
    mod.TurnOffForSession("item k: test");
    ok = ok && mod.ButtonStep(true, true, true, true) == StashMoveButtonStep::Remove
        && mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Keep;
    Check("target/button_exists_only_with_the_stash_and_sort_listed", ok, "");
}

static void TargetButtonPressRunsOnceUnderTheKeyGuard()
{
    // The adapter records a press (from the activation's detour or the frame
    // poll) and the frame tick takes it: one run per press, and only under
    // the key's own guard - the game in front, the stash listed, no modifier
    // held. A press the guard refuses is dropped, never kept for later.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    mod.NoteButtonPress();
    bool first = mod.TakeButtonPress(true, true, false);
    bool again = mod.TakeButtonPress(true, true, false);
    mod.NoteButtonPress();
    mod.NoteButtonPress();
    bool twice = mod.TakeButtonPress(true, true, false);
    bool twiceAgain = mod.TakeButtonPress(true, true, false);
    mod.NoteButtonPress();
    bool modifier = mod.TakeButtonPress(true, true, true);
    bool afterModifier = mod.TakeButtonPress(true, true, false);
    mod.NoteButtonPress();
    bool background = mod.TakeButtonPress(false, true, false);
    mod.NoteButtonPress();
    bool noStash = mod.TakeButtonPress(true, false, false);
    bool ok = first && !again && twice && !twiceAgain && !modifier && !afterModifier && !background && !noStash;
    Check("target/button_press_runs_once_under_the_key_guard", ok,
          "first=" + std::to_string(first) + " again=" + std::to_string(again) + " twice=" + std::to_string(twice)
          + " twiceAgain=" + std::to_string(twiceAgain) + " modifier=" + std::to_string(modifier)
          + " afterModifier=" + std::to_string(afterModifier) + " background=" + std::to_string(background)
          + " noStash=" + std::to_string(noStash));
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
    TargetFullShownTabKeepsItemInBag();
    TargetShownTabChangedOrUnreadIsUnconfirmed();
    BaselineGridTabFromBagSubTabIsRefused();
    TargetBagMaterialsViewFeedsTheMaterialsTab();
    TargetNewMaterialIdentityIsPlacedInACell();
    TargetWholeStackMergesByItsCount();
    TargetSocketableTabAndSocketViewAreRefused();
    TargetShownTabRoom();
    TargetLines();
    TargetSecondItemMergesAtUseOnMaterials();
    TargetSecondItemMergesAtUseOnAPage();
    TargetUnreadableStackSumAtUseSkips();
    TargetHeldModifierIsNoKeyEdge();
    TargetOwnerStepThatDidNotTakeIsUnconfirmed();
    BaselineButtonOffCreatesNothing();
    TargetButtonExistsOnlyWithTheStashAndSortListed();
    TargetButtonPressRunsOnceUnderTheKeyGuard();
    std::cout << (g_Failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return g_Failures ? 1 : 0;
}
