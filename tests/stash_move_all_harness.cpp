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
// merges by its count (both measured by name). Live 1f and 1g decided the
// Socketable tab: a socketable whose identity has a node there merges by name
// (socketMergeRoute), a new kind is a planned skip, and the bag's Socket view
// is its only source. Each rule is pinned for the state recorded, with the
// other state as its negative control. The bag's Materials view is a source
// for the Materials tab only; a stash page from a bag sub-tab is refused
// (baseline).
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
//
// Phase C (C3), after Live 1f and 1g: the button takes the poll route (a left
// press inside the node's bbox; no activation, no detour), a node that cannot
// be made is reported once and never turns the mod off, and the Socketable
// tab merges an identity with a node on it (socketMergeRoute: byname), fed
// from the bag's Socket view only, a new kind staying in the bag. Written
// before the header had them; the first error line was `error C2039:
// 'PressInNode': is not a member of 'ForgePact::StashMoveAllMod'`, 2026-09-28.
//
// Phase C round 1 (the review's instrument-blindness finding): the button's
// press path counts where each press went and the state line prints it, so
// a click that moved nothing names poll-blind, a bbox miss, a poll that
// threw or a guard drop. Written before the header had it; the first error
// line was `error C2039: 'NoteButtonHeld': is not a member of
// 'ForgePact::StashMoveAllMod'`, 2026-09-28.
//
// Phase D (before Live 2): the Socketable tab's merge was measured with one
// unit only, so it reads its own flag, socketWholeStackMerge (off), and a
// socketable of more than one unit stays in the bag; and the state line after
// a loss keeps the button's fields, the free-text reason last. Written before
// the header had either; the first error line was `error C2039:
// 'socketWholeStackMerge': is not a member of 'ForgePact::StashMoveRoutes'`,
// 2026-09-28.
//
// ForgePact #131 (the owner's report of 2026-09-30: the button sat with its
// bottom-right corner in the middle of where it belongs, and items stayed in
// the bag with room on the tab). The game's merge as a model (a stack takes
// the whole count only while it stays at or below the cap, 999, or 999999
// with the sixth argument's flag 8: the static reading of StashAddToStack);
// the route and room rule per stack, not per sum (a full stack of the kind on
// Materials or a page starts a new stack in a free cell, a stack with room
// takes the merge, the Socketable tab's one stack takes a socketable of any
// count and is never doubled); a true answer on the cell route decided as a
// merge by the sum; and the button's origin from Sort's box and the node's own
// extents (UI_Button_Small_obj's origin is its bbox centre, the Sort node's
// its top-left: Live 1f and 1g). Written before the header had them; the
// first error line was `error C2039: 'StashMoveStacks': is not a member of
// 'ForgePact'` (on its using-declaration), 2026-09-30.
//
// #131 round 1 (the review's instrument-blindness finding: the button's place
// was checked on a box read in the frame the node was made, which is not
// known to be its settled box). The check moved to later ensure steps, on a
// box that reads the same twice with the node visible, and the state line
// carries what it read. Written before the header had it; the first error
// line was `error C2039: 'StashMoveButtonCheck': is not a member of
// 'ForgePact'`, 2026-09-30.
//
// #131, owner scope of 2026-09-30 ("Button should be the size and look of
// sort tab button"): Live 1 placed the node right but at 206x48 beside Sort's
// 192x66. The node now wears Sort's look, copied from the Sort node by the
// adapter, so the first node is made with Sort's own extents about Sort's
// origin; its size is judged against Sort's on the settled read, and a size
// or a look that is not Sort's is kept and said once each, never a remake
// and never the mod off. Compiled against the header before it had them, the
// first error line was `error C2039: 'StashMoveButtonLook': is not a member
// of 'ForgePact'`, 2026-09-30.
//
// #131, owner of 2026-09-30 ("Use its coordinates"): the node's box is the
// Mercenary button's, which Live 5 measured beside Sort with the bag open on
// its own and found not listed while the stash is open, so the target is
// Sort's box moved and sized by the measured fractions (merc-route:
// relation); the old Sort rule stays only as the fallback, and the state line
// names which (button_ref=). Written before the header had it; the first
// error line was `error C2039: 'SortRuleBox': is not a member of
// 'ForgePact::StashMoveAllMod'`, 2026-09-30.
//
// #131, the review of fix2's round 2: the adapter's look copy returned from
// inside its loop on the first member of a kind it did not accept (a
// `textFont` read as a string, possibly), so the label offsets after it were
// never written. The per-member decisions (LookStep, LookCompare) and the
// verdict (StashMoveLookTally) moved into the core, over every kind the
// runtime returns for these members, and the copy is Live 5's 13 members as
// read. The scenarios drive them through a stand-in of the adapter's loop.
// Compiled against the header before it had them, the first error line was
// `error C2039: 'StashMoveLookKind': is not a member of 'ForgePact'`,
// 2026-09-30.
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// PRODUCTION_STASHMOVEALL

using ForgePact::StashMoveStacks;
using ForgePact::StashMoveBox;
using ForgePact::StashMoveExtents;
using ForgePact::StashMoveAllMod;
using ForgePact::StashMoveButtonStep;
using ForgePact::StashMoveButtonLook;
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

// The shown tab's stacks of one identity, each stack's count in the array's
// order, as the adapter read them.
static StashMoveStacks Stacks(const std::vector<long long>& counts)
{
    StashMoveStacks s;
    s.read = true;
    for (long long n : counts) s.counts.push_back(n);
    return s;
}

// The round-2 scenarios' one-number form of a re-read: -1 unread, 0 no stack
// of the identity, else one stack of that count.
static StashMoveStacks Sum(long long n)
{
    if (n < 0) return StashMoveStacks();
    return n == 0 ? Stacks({}) : Stacks({n});
}

// A stack with room for any count these scenarios move (the Materials cap is
// 999): what "a stack of its identity is there" meant before #131.
static const std::vector<long long> kRoomyStack = {100};

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
    c.destinationStacks = Stacks(destinationHasStack ? kRoomyStack : std::vector<long long>());
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
static StashMoveReport MergedWherePlannedACell();

// The place check's fields of the state line before any node was made (the
// look and size: owner scope, 2026-09-30; the look's members that read the
// same last: the review of fix2's round 2).
static const std::string kIdlePlace =
    " button_place=none button_box=none button_extents=none button_makes=0 button_step=0"
    " button_look=none button_size=none button_ref=none button_look_same=none";
// The button's fields of the state line before any node or press.
static const std::string kIdleButton =
    " button=none presses=0 in_node=0 outside=0 unread=0 errors=0 taken=0 dropped=0 last_drop=none" + kIdlePlace;

// ---- baseline: off is vanilla ----------------------------------------------

static void BaselineOffByDefault()
{
    StashMoveAllMod mod;
    Check("baseline/off_by_default", !mod.IsEnabled() && !mod.OffThisSession()
          && mod.StateLine() == "stashmoveall: state=off key=F4" + kIdleButton,
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
    // The Socketable tab takes only the bag's Socket view (socketMergeRoute).
    StashMoveView socketTab = v; socketTab.stashTab = -2; expect(socketTab, "unsupported bag tab 0 for stash tab -2");
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
    for (StashMoveCell& c : unreadStack.cells) c.destinationStacks = StashMoveStacks();
    StashMovePlan u = mod.Plan(unreadStack);
    ok = ok && u.items.size() == 3 && u.items[0].route == StashMoveRoute::None && u.items[1].route == StashMoveRoute::None
        && u.items[0].refusal == "its stack on the shown tab could not be read" && u.items[2].route == StashMoveRoute::Cell;
    // The Materials tab takes class 14 and nothing else: onto its stack when
    // its identity is there, else into a cell (newMaterialRoute). Another
    // class is planned as a skip that calls nothing.
    StashMoveView mats = MixedView(-4);
    mats.cells[0].destinationStacks = Stacks(kRoomyStack);
    StashMovePlan m = mod.Plan(mats);
    for (const StashMoveItem& i : m.items) {
        bool mine = i.cell.itemClass == 14;
        if (mine != (i.route == StashMoveRoute::Stack)) { ok = false; detail += " mats " + i.cell.key; }
        if (!mine && (i.route != StashMoveRoute::None || i.refusal != "not taken by the Materials tab")) { ok = false; detail += " matsref " + i.refusal; }
    }
    // The Socketable tab (socketMergeRoute: byname), fed from the bag's Socket
    // view, takes class 15 onto the stack of its identity; one unit, the count
    // Live 1f measured (more than one: its own scenario below).
    StashMoveView sock = MixedView(-2);
    sock.bagTab = -2;
    sock.cells[6].destinationStacks = Stacks(kRoomyStack);
    sock.cells[6].count = 1;
    StashMovePlan s = mod.Plan(sock);
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
        && mod.StateLine() == "stashmoveall: state=off-for-this-session" + kIdleButton + " reason=" + reason
        && mod.OffForSessionLine() == "stashmoveall: off for this session - " + reason
                                      + "; turn it on again after restarting the game";
    // Negative control: switched off by hand, the state line is plain off.
    StashMoveAllMod byHand;
    byHand.SetEnabled(true);
    byHand.SetEnabled(false);
    ok = ok && byHand.StateLine() == "stashmoveall: state=off key=F4" + kIdleButton &&byHand.OffReason().empty();
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
            // The Materials view is a source for the Materials tab only, and
            // the Socket view for the Socketable tab only.
            const std::string want = "unsupported bag tab " + std::to_string(bagTab) + " for stash tab "
                + std::to_string(stashTab);
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

// The bag's Socket view as the adapter would read it with the Socketable tab
// on show: an orb whose identity has a node on the tab (Live 1f's base id
// 118), three of a gem whose identity has one too (base id 38, which merged
// and so is stackable), a socketable of a kind the tab lacks, and a ring.
static StashMoveView SocketView()
{
    StashMoveView v;
    v.stashListed = true;
    v.bagTab = -2;
    v.stashTab = -2;
    v.cells = { Cell(0, 0, "0-0-118-15", 15, true, 1, true), Cell(1, 0, "0-0-38-15", 15, true, 3, true),
                Cell(2, 0, "0-0-31-15", 15, true, 1, false), Cell(3, 0, "0-0-5-7", 7) };
    return v;
}

static void BaselineSocketableTabTakesOnlyTheBagSocketView()
{
    // socketMergeRoute: byname was measured from the bag's Socket view only
    // (Live 1f, Live 1g), so that view is the Socketable tab's one source: a
    // bag page, or the Materials view, feeding it is refused with nothing
    // called, and the Socket view feeds no other stash tab.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    bool ok = true;
    std::string detail;
    for (int bagTab : {0, 4, -4}) {
        StashMoveView v = SocketView();
        v.bagTab = bagTab;
        const StashMovePlan p = mod.Plan(v);
        const std::string want = "unsupported bag tab " + std::to_string(bagTab) + " for stash tab -2";
        if (!p.refused || p.reason != want || !p.items.empty()) { ok = false; detail += " want " + want + " got " + p.reason; }
    }
    StashMoveView toMats = SocketView();
    toMats.stashTab = -4;
    ok = ok && mod.Plan(toMats).reason == "unsupported bag tab -2 for stash tab -4";
    StashMoveView toPage = SocketView();
    toPage.stashTab = 1;
    ok = ok && mod.Plan(toPage).reason == "unsupported bag tab -2 for stash tab 1";
    ok = ok && StashMoveAllMod::RefusalLine("stashmoveall", "unsupported bag tab 0 for stash tab -2")
        == "stashmoveall: refused - unsupported bag tab 0 for stash tab -2; nothing was called";
    // Negative control: the Socket view with the Socketable tab on show is a run.
    ok = ok && !mod.Plan(SocketView()).refused;
    Check("baseline/socketable_tab_takes_only_the_bag_socket_view", ok, detail);
}

static void TargetSocketableMergesAnIdentityWithANode()
{
    // socketMergeRoute: byname (Live 1f byname-socket-merge, Live 1g): a
    // socketable whose identity has a node on the tab merges by its whole
    // count, confirmed only on that identity's count rising by exactly it; a
    // ring is not taken there. A count above 1 merges while
    // socketWholeStackMerge is on (on since #131; its own scenario below
    // pins both states); the gem's merge is checked with it on.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    const StashMovePlan p = mod.Plan(SocketView());
    bool ok = StashMoveAllMod::kMeasuredRoutes.socketMerge && !p.refused && p.items.size() == 4
        && p.items[0].route == StashMoveRoute::Stack && p.items[0].cell.count == 1
        && p.items[3].route == StashMoveRoute::None && p.items[3].refusal == "not taken by the Socketable tab";
    StashMoveRoutes whole = StashMoveAllMod::kMeasuredRoutes;
    whole.socketWholeStackMerge = true;
    const StashMovePlan pw = StashMoveAllMod::PlanWith(SocketView(), whole, true);
    ok = ok && pw.items.size() == 4 && pw.items[1].route == StashMoveRoute::Stack && pw.items[1].cell.count == 3;
    const StashMoveResult orb = StashMoveAllMod::Decide(p.items[0], Stacked(81, 82));
    const StashMoveResult gem = StashMoveAllMod::Decide(pw.items[1], Stacked(2, 5));
    ok = ok && orb.outcome == StashMoveOutcome::Moved && StashMoveAllMod::ItemLine(orb) == "stashmoveall: item 0-0-118-15 -> stack"
        && gem.outcome == StashMoveOutcome::Moved;
    // One unit short is a loss, never a move.
    ok = ok && StashMoveAllMod::Decide(pw.items[1], Stacked(2, 3)).outcome == StashMoveOutcome::Unconfirmed;
    // At the point of use the sum decides, as on the Materials tab: a node
    // still there is merged into, an unread sum is a skip that calls nothing.
    ok = ok && StashMoveAllMod::RouteAtUse(p.items[0], -2, Sum(81)).route == StashMoveRoute::Stack
        && StashMoveAllMod::RouteAtUse(p.items[0], -2, Sum(-1)).refusal == "its stack on the shown tab could not be read";
    // Negative control: with the merge not measured (socketMergeRoute
    // not-observed) the tab is refused as a destination, as before Live 1f.
    const StashMovePlan off = StashMoveAllMod::PlanWith(SocketView(), Flipped(false, false, true, true), true);
    ok = ok && off.refused && off.reason == "unsupported stash tab -2" && off.items.empty();
    Check("target/socketable_merges_an_identity_with_a_node_by_its_whole_count", ok,
          Keys(p) + " " + StashMoveAllMod::ItemLine(orb) + " off=" + off.reason);
}

static void TargetSocketableNewKindStaysInTheBag()
{
    // socketRoute new: not-observed (no accepted identity absent from the tab
    // was obtainable without a person): a socketable whose identity has no
    // node on the tab is a planned skip that calls nothing, and the run goes on.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    const StashMovePlan p = mod.Plan(SocketView());
    bool ok = !StashMoveAllMod::kMeasuredRoutes.socketNew && p.items.size() == 4
        && p.items[2].route == StashMoveRoute::None && p.items[2].refusal == "a new kind stays in the bag";
    StashMoveResult skip;
    ok = ok && !StashMoveAllMod::MayCall(p.items[2], 1, skip) && skip.outcome == StashMoveOutcome::Skipped
        && StashMoveAllMod::ItemLine(skip) == "stashmoveall: item 0-0-31-15 -> skipped: a new kind stays in the bag";
    StashMoveTally t = mod.Begin(p);
    ok = ok && mod.Record(t, skip) && !t.stopped && mod.IsEnabled();
    // At the point of use too: a sum of 0 is a new kind, still a skip.
    StashMoveItem planned = p.items[0];
    ok = ok && StashMoveAllMod::RouteAtUse(planned, -2, Sum(0)).refusal == "a new kind stays in the bag";
    // Negative control: were the new-identity placement measured, it would
    // go into a cell.
    const StashMovePlan withNew = StashMoveAllMod::PlanWith(SocketView(), Flipped(true, true, true, true), true);
    ok = ok && withNew.items.size() == 4 && withNew.items[2].route == StashMoveRoute::Cell;
    Check("target/socketable_new_kind_stays_in_the_bag", ok, Keys(p) + " " + p.items[2].refusal);
}

static void TargetSocketableWholeStackMergesIntoItsOneStack()
{
    // #131: most bag socketables are stacks, and the flag that kept a stack of
    // more than one in the bag (Live 1f measured the Socketable tab's merge
    // with one unit only) is on, confirmed by Live procedure 3's socket-whole.
    // The tab holds one stack per kind (owner, 2026-09-30), and its merge
    // passes the sixth argument 8, so the cap is 999999: [81] + 3 merges by
    // the whole count, moved only on that node's count rising by exactly 3.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    const StashMovePlan p = mod.Plan(SocketView());
    bool ok = StashMoveAllMod::kMeasuredRoutes.socketWholeStackMerge && !StashMoveAllMod::kMeasuredRoutes.socketNew
        && !p.refused && p.items.size() == 4
        && p.items[1].cell.key == "0-0-38-15" && p.items[1].route == StashMoveRoute::Stack && p.items[1].cell.count == 3;
    const StashMoveItem atUse = StashMoveAllMod::RouteAtUse(p.items[1], -2, Stacks({81}));
    ok = ok && atUse.route == StashMoveRoute::Stack && atUse.cell.count == 3;
    StashMoveResult none;
    ok = ok && StashMoveAllMod::StackRoom(Stacks({81}), 3, StashMoveAllMod::CapFor(-2)) == 1
        && StashMoveAllMod::MayCall(atUse, 1, none);
    const StashMoveResult moved = StashMoveAllMod::Decide(atUse, Stacked(81, 84));
    ok = ok && moved.outcome == StashMoveOutcome::Moved && StashMoveAllMod::ItemLine(moved) == "stashmoveall: item 0-0-38-15 -> stack";
    // One unit short is a loss, never a move.
    ok = ok && StashMoveAllMod::Decide(atUse, Stacked(81, 82)).outcome == StashMoveOutcome::Unconfirmed;
    // A single socketable still merges (the ordinary case Live 1f measured).
    ok = ok && StashMoveAllMod::RouteAtUse(p.items[0], -2, Stacks({81})).route == StashMoveRoute::Stack;
    // Negative control: with the flag off (the state before #131), a stack
    // of more than one is a planned skip and one unit still merges.
    const char* why = "a socketable merge of more than one unit is not measured";
    StashMoveRoutes off = StashMoveAllMod::kMeasuredRoutes;
    off.socketWholeStackMerge = false;
    const StashMovePlan po = StashMoveAllMod::PlanWith(SocketView(), off, true);
    ok = ok && po.items.size() == 4 && po.items[1].route == StashMoveRoute::None && po.items[1].refusal == why
        && po.items[0].route == StashMoveRoute::Stack
        && StashMoveAllMod::RouteAtUse(p.items[1], -2, Stacks({81}), off).refusal == why;
    Check("target/socketable_whole_stack_merges_into_its_one_stack", ok,
          Keys(p) + " " + (p.items.size() > 1 ? std::to_string((int)p.items[1].route) + p.items[1].refusal : std::string()));
}

static void TargetFullSocketableStackNeverStartsASecondStack()
{
    // The Socketable tab has one stack per kind: a stack its merge cannot
    // take (999999 is the cap with flag 8) is a skip with its own reason, and
    // no placement is tried, since the tab never holds a second stack of a
    // kind. No stack at all is still a new kind (socketNew: not-observed).
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    StashMoveItem gem;
    gem.cell = Cell(1, 0, "0-0-38-15", 15, true, 1, true);
    gem.route = StashMoveRoute::Stack;
    const StashMoveItem full = StashMoveAllMod::RouteAtUse(gem, -2, Stacks({999999}));
    bool ok = full.route == StashMoveRoute::None && full.refusal == "its stack on the shown tab is full";
    StashMoveResult skip;
    ok = ok && !StashMoveAllMod::MayCall(full, 1, skip)
        && StashMoveAllMod::ItemLine(skip) == "stashmoveall: item 0-0-38-15 -> skipped: its stack on the shown tab is full";
    StashMoveTally t;
    ok = ok && mod.Record(t, skip) && !t.stopped && mod.IsEnabled();
    ok = ok && StashMoveAllMod::RouteAtUse(gem, -2, Stacks({})).refusal == "a new kind stays in the bag";
    // Even with the new-kind placement turned on, a full stack is no new one.
    StashMoveRoutes withNew = StashMoveAllMod::kMeasuredRoutes;
    withNew.socketNew = true;
    ok = ok && StashMoveAllMod::RouteAtUse(gem, -2, Stacks({999999}), withNew).refusal == "its stack on the shown tab is full";
    // Negative control: one unit below the cap still takes it.
    ok = ok && StashMoveAllMod::RouteAtUse(gem, -2, Stacks({999998})).route == StashMoveRoute::Stack;
    Check("target/full_socketable_stack_never_starts_a_second_stack", ok, full.refusal);
}

// ---- #131: the game's merge rule, and the per-stack route --------------------

static void BaselineGameMergeTakesAStackOnlyWhileTheSumStaysAtTheCap()
{
    // The static reading of StashAddToStack (2026-09-30): the cap is 999, or
    // 999999 when the sixth argument carries flag 8; the first stack of the
    // identity, in array order, whose count plus the moved count is at or
    // below the cap takes the whole count; none does, and the answer is
    // false. A list that could not be read is unknown, never "fits" or "full".
    using M = StashMoveAllMod;
    const int64_t cap = M::StackCap(0);
    bool ok = cap == 999 && M::StackCap(8) == 999999 && M::StackCap(9) == 999999 && M::StackCap(2) == 999
        && M::CapFor(-4) == 999 && M::CapFor(0) == 999 && M::CapFor(7) == 999 && M::CapFor(-2) == 999999;
    ok = ok && M::StackThatFits(Stacks({998}), 1, cap) == 0
        && M::StackThatFits(Stacks({999}), 1, cap) == M::kNoStackFits
        && M::StackThatFits(Stacks({999, 400}), 500, cap) == 1
        && M::StackThatFits(Stacks({949}), 50, cap) == 0
        && M::StackThatFits(Stacks({950}), 50, cap) == M::kNoStackFits
        && M::StackThatFits(Stacks({}), 1, cap) == M::kNoStackFits;
    // The first that fits, not the fullest or the emptiest.
    ok = ok && M::StackThatFits(Stacks({10, 20}), 5, cap) == 0;
    // Unknown: the list unread, a stack in it unread, or the count unread.
    ok = ok && M::StackThatFits(StashMoveStacks(), 1, cap) == M::kStacksUnknown
        && M::StackThatFits(Stacks({5, -1}), 1, cap) == M::kStacksUnknown
        && M::StackThatFits(Stacks({5}), 0, cap) == M::kStacksUnknown
        && M::StackThatFits(Stacks({5}), -1, cap) == M::kStacksUnknown;
    // The same answers as the room a stack route has, and the sum it is
    // confirmed by.
    ok = ok && M::StackRoom(Stacks({998}), 1, cap) == 1 && M::StackRoom(Stacks({999}), 1, cap) == 0
        && M::StackRoom(StashMoveStacks(), 1, cap) == -1
        && M::StackSum(Stacks({999, 400})) == 1399 && M::StackSum(Stacks({})) == 0
        && M::StackSum(StashMoveStacks()) == -1 && M::StackSum(Stacks({3, -1})) == -1;
    // Negative control: the sum-only rule this replaces would have merged
    // [999] + 1, which the game refuses.
    ok = ok && M::StackSum(Stacks({999})) > 0 && M::StackThatFits(Stacks({999}), 1, cap) < 0;
    Check("baseline/game_merge_takes_a_stack_only_while_the_sum_stays_at_the_cap", ok, "");
}

// The Materials tab (sixth argument 0, cap 999), from its view: one bag
// material of `count` whose identity has `stacks` on the tab.
static StashMoveItem MaterialAtUse(long long count, const StashMoveStacks& stacks, const std::string& key = "0-0-72-14")
{
    StashMoveItem it;
    it.cell = Cell(0, 0, key, 14, true, count, true);
    it.route = StashMoveRoute::Stack;
    return StashMoveAllMod::RouteAtUse(it, StashMoveAllMod::kMaterialsTab, stacks);
}

static void TargetFullMaterialsStackOverflowsIntoAFreeCell()
{
    // The owner's report: a Materials stack at 999 left the next unit in the
    // bag although the tab had free cells. [999] + 1 with room is a new stack
    // in a cell (the tab placement, as a new identity takes); the ordinary
    // cases beside it: [100] + 50 merges, a new identity with room is placed.
    const int64_t cap = StashMoveAllMod::CapFor(StashMoveAllMod::kMaterialsTab);
    const StashMoveItem full = MaterialAtUse(1, Stacks({999}));
    StashMoveResult none;
    bool ok = full.route == StashMoveRoute::Cell && StashMoveAllMod::MayCall(full, 1, none);
    const StashMoveResult placed = StashMoveAllMod::Decide(full, PlacedCell(4, 2));
    ok = ok && placed.outcome == StashMoveOutcome::Moved && StashMoveAllMod::ItemLine(placed) == "stashmoveall: item 0-0-72-14 -> cell 4,2";
    const StashMoveItem ordinary = MaterialAtUse(50, Stacks({100}));
    ok = ok && ordinary.route == StashMoveRoute::Stack && StashMoveAllMod::StackRoom(Stacks({100}), 50, cap) == 1
        && StashMoveAllMod::Decide(ordinary, Stacked(100, 150)).outcome == StashMoveOutcome::Moved;
    ok = ok && MaterialAtUse(50, Stacks({})).route == StashMoveRoute::Cell;
    // Negative control: before #131 any stack of the kind meant a merge, the
    // game answered false, and a false with both sides unchanged is a skip.
    StashMoveItem sumOnly = full;
    sumOnly.route = StashMoveRoute::Stack;
    StashMoveReport refused = Refused("StashAddToStack answered false");
    refused.stackBefore = 999;
    refused.stackAfter = 999;
    ok = ok && StashMoveAllMod::Decide(sumOnly, refused).outcome == StashMoveOutcome::Skipped;
    Check("target/full_materials_stack_overflows_into_a_free_cell", ok,
          std::to_string((int)full.route) + " " + full.refusal);
}

static void TargetMaterialsMergeSkipsTheFullStackForOneWithRoom()
{
    // The tab holds several stacks of a kind (owner, 2026-09-30): [999, 400]
    // + 500 merges (the game takes the second stack), confirmed on the kind's
    // sum rising by 500.
    const StashMoveItem it = MaterialAtUse(500, Stacks({999, 400}));
    bool ok = it.route == StashMoveRoute::Stack
        && StashMoveAllMod::StackThatFits(Stacks({999, 400}), 500, 999) == 1
        && StashMoveAllMod::StackRoom(Stacks({999, 400}), 500, 999) == 1;
    ok = ok && StashMoveAllMod::Decide(it, Stacked(1399, 1899)).outcome == StashMoveOutcome::Moved;
    // Negative control: [999, 600] + 500 fits neither, so it is a new cell.
    ok = ok && MaterialAtUse(500, Stacks({999, 600})).route == StashMoveRoute::Cell;
    Check("target/materials_merge_skips_the_full_stack_for_one_with_room", ok, it.refusal);
}

static void TargetMergeAtExactlyTheCapIsAMerge()
{
    // The cap is inclusive: [949] + 50 reaches 999 exactly and merges;
    // [950] + 50 would pass it and starts a new stack instead.
    bool ok = MaterialAtUse(50, Stacks({949})).route == StashMoveRoute::Stack
        && MaterialAtUse(50, Stacks({950})).route == StashMoveRoute::Cell
        && MaterialAtUse(1, Stacks({998})).route == StashMoveRoute::Stack;
    Check("target/merge_at_exactly_the_cap_is_a_merge", ok, "");
}

static void TargetNoStackFitsAndNoFreeCellStaysInTheBag()
{
    // Never overflow (D4) holds for a new stack too: [999, 600] + 500 with no
    // free block on the shown tab calls nothing and stays in the bag; the
    // room read failing calls nothing either; with room, it is placed.
    const StashMoveItem it = MaterialAtUse(500, Stacks({999, 600}));
    StashMoveResult skip, unread, none;
    bool ok = it.route == StashMoveRoute::Cell
        && !StashMoveAllMod::MayCall(it, 0, skip) && skip.outcome == StashMoveOutcome::Skipped
        && StashMoveAllMod::ItemLine(skip) == "stashmoveall: item 0-0-72-14 -> skipped: no room on the shown tab"
        && !StashMoveAllMod::MayCall(it, -1, unread) && unread.answer == "the shown tab's room could not be read"
        && StashMoveAllMod::MayCall(it, 1, none);
    // A stack list that could not be read is a skip, never a new stack.
    const StashMoveItem u = MaterialAtUse(500, StashMoveStacks());
    ok = ok && u.route == StashMoveRoute::None && u.refusal == "its stack on the shown tab could not be read";
    // A new stack on the Materials tab is the new-identity placement: not
    // measured, it stays in the bag.
    StashMoveItem planned;
    planned.cell = Cell(0, 0, "0-0-72-14", 14, true, 1, true);
    planned.route = StashMoveRoute::Stack;
    ok = ok && StashMoveAllMod::RouteAtUse(planned, StashMoveAllMod::kMaterialsTab, Stacks({999}),
                                           Flipped(false, true, false, true)).refusal == "a new kind stays in the bag";
    Check("target/no_stack_fits_and_no_free_cell_stays_in_the_bag", ok, skip.answer + " | " + u.refusal);
}

static void TargetFullKeyStackOnAPageOverflowsIntoAFreeCell()
{
    // A stash page (the personal one, the sixth argument 0, cap 999): a key
    // stack at 999 is no merge for one more key; with room it goes into a
    // free cell of the page. Negative control: [998] merges.
    StashMoveItem key;
    key.cell = Cell(0, 0, "0-0-5-12", 12, true, 1, true);
    key.route = StashMoveRoute::Stack;
    const StashMoveItem full = StashMoveAllMod::RouteAtUse(key, 0, Stacks({999}));
    StashMoveResult none;
    bool ok = StashMoveAllMod::CapFor(0) == 999 && full.route == StashMoveRoute::Cell && StashMoveAllMod::MayCall(full, 1, none)
        && StashMoveAllMod::Decide(full, PlacedCell(3, 0)).outcome == StashMoveOutcome::Moved;
    ok = ok && StashMoveAllMod::RouteAtUse(key, 0, Stacks({998})).route == StashMoveRoute::Stack;
    // The plan says the same before the run.
    StashMoveView v;
    v.stashListed = true; v.bagTab = 0; v.stashTab = 0;
    v.cells = { Cell(0, 0, "0-0-5-12", 12, true, 1) };
    v.cells[0].destinationStacks = Stacks({999});
    const StashMovePlan p = StashMoveAllMod::PlanWith(v, StashMoveAllMod::kMeasuredRoutes, true);
    ok = ok && p.items.size() == 1 && p.items[0].route == StashMoveRoute::Cell;
    Check("target/full_key_stack_on_a_page_overflows_into_a_free_cell", ok, full.refusal);
}

static void TargetUnexpectedMergeOnTheCellRouteIsConfirmedAsAMerge()
{
    // The cell route's StashAddToStack now carries the item's whole count
    // (the value the measured merge passes). A true answer there is the game
    // merging where the model read no stack with room: decided as a merge,
    // moved only when the kind's sum rose by exactly the count, never a loss
    // by construction and never a unit taken from a larger stack.
    StashMoveItem it = MaterialAtUse(5, Stacks({999}));
    bool ok = it.route == StashMoveRoute::Cell;
    const StashMoveItem merge = StashMoveAllMod::AsMerge(it);
    ok = ok && merge.route == StashMoveRoute::Stack && merge.cell.key == it.cell.key && merge.cell.count == 5;
    const StashMoveResult moved = StashMoveAllMod::Decide(merge, Stacked(999, 1004));
    ok = ok && moved.outcome == StashMoveOutcome::Moved && StashMoveAllMod::ItemLine(moved) == "stashmoveall: item 0-0-72-14 -> stack";
    // Anything else is unconfirmed: one unit taken, or the bag cell kept.
    ok = ok && StashMoveAllMod::Decide(merge, Stacked(999, 1000)).outcome == StashMoveOutcome::Unconfirmed
        && StashMoveAllMod::Decide(merge, Stacked(999, 1004, 1)).outcome == StashMoveOutcome::Unconfirmed;
    // Negative control: decided on the cell route, the same answer cannot be
    // confirmed (nothing was placed) - the round-2 duplicate's shape.
    ok = ok && StashMoveAllMod::Decide(it, MergedWherePlannedACell()).outcome == StashMoveOutcome::Unconfirmed;
    Check("target/unexpected_merge_on_the_cell_route_is_confirmed_as_a_merge", ok, "");
}

// ---- #131: the button's origin ----------------------------------------------

// Live 1f and 1g, the same numbers both times (2560x1440 GUI, save slot 14):
// the Sort node's x, y and bbox, and the Move all node the old formula made.
static StashMoveBox Box(double l, double t, double r, double b)
{
    StashMoveBox box;
    box.left = l; box.top = t; box.right = r; box.bottom = b;
    return box;
}
static const StashMoveBox kSortBox = Box(2303.5, 1198.9, 2485.9, 1261.6);
static const double kSortX = 2303.5, kSortY = 1198.9;
static const StashMoveBox kOldNodeBox = Box(2016.2, 1176.1, 2211.9, 1221.7);
static const double kOldNodeX = 2113.1, kOldNodeY = 1198.9;
static const double kGap = 8.0;

static bool Near(double a, double b, double tol) { return a - b <= tol && b - a <= tol; }

static void BaselineButtonSmallOriginIsItsCentreAndSortOriginItsTopLeft()
{
    // What UiCreateNode's x, y are: the node's origin, which for
    // UI_Button_Small_obj (sprite Menu_Button_Chat_spr) lies at its bbox
    // centre, while the Sort node's lies at its bbox top-left.
    bool ok = Near(kOldNodeX, (kOldNodeBox.left + kOldNodeBox.right) / 2, 1.0)
        && Near(kOldNodeY, (kOldNodeBox.top + kOldNodeBox.bottom) / 2, 1.0)
        && Near(kSortX, kSortBox.left, 0.05) && Near(kSortY, kSortBox.top, 0.05);
    StashMoveExtents e;
    ok = ok && StashMoveAllMod::ExtentsOf(kOldNodeX, kOldNodeY, kOldNodeBox, e)
        && Near(e.left, 96.9, 0.05) && Near(e.up, 22.8, 0.05) && Near(e.right, 98.8, 0.05) && Near(e.down, 22.8, 0.05);
    // The old formula: Sort's x less its width less the gap, Sort's y, as if
    // the new node's origin were its top-left - which gives the origin Live
    // 1f and 1g measured.
    const double oldX = kSortX - (kSortBox.right - kSortBox.left) - kGap;
    ok = ok && Near(oldX, kOldNodeX, 0.05) && Near(kSortY, kOldNodeY, 0.05);
    // Negative control: a box that did not read gives no extents.
    StashMoveExtents none;
    ok = ok && !StashMoveAllMod::ExtentsOf(kOldNodeX, kOldNodeY, StashMoveBox(), none)
        && !StashMoveAllMod::ExtentsOf(std::nan(""), kOldNodeY, kOldNodeBox, none);
    Check("baseline/button_small_origin_is_its_centre_and_sort_origin_its_top_left", ok, "");
}

static void TargetButtonRightEdgeSitsTheGapLeftOfSortCentredOnIt()
{
    StashMoveExtents e;
    StashMoveAllMod::ExtentsOf(kOldNodeX, kOldNodeY, kOldNodeBox, e);
    double x = 0, y = 0;
    bool ok = StashMoveAllMod::ButtonOrigin(kSortBox, e, kGap, x, y) && Near(x, 2196.7, 0.05) && Near(y, 1230.25, 0.05);
    // The box it gives: right edge the gap left of Sort, the vertical centre
    // Sort's; on target, and clear of Sort.
    const StashMoveBox target = Box(x - e.left, y - e.up, x + e.right, y + e.down);
    ok = ok && Near(target.left, 2099.8, 0.05) && Near(target.top, 1207.45, 0.05)
        && Near(target.right, 2295.5, 0.05) && Near(target.bottom, 1253.05, 0.05)
        && target.right < kSortBox.left && StashMoveAllMod::ButtonOnTarget(kSortBox, target, kGap);
    // Within 1 GUI unit is on target; 1.5 off is not.
    ok = ok && StashMoveAllMod::ButtonOnTarget(kSortBox, Box(target.left + 0.9, target.top, target.right + 0.9, target.bottom), kGap)
        && !StashMoveAllMod::ButtonOnTarget(kSortBox, Box(target.left, target.top + 1.5, target.right, target.bottom + 1.5), kGap);
    // Extents measured on the node, not built in: a node half the size (a
    // GUI scale change) is still placed right.
    StashMoveExtents half;
    half.left = e.left / 2; half.up = e.up / 2; half.right = e.right / 2; half.down = e.down / 2;
    double hx = 0, hy = 0;
    ok = ok && StashMoveAllMod::ButtonOrigin(kSortBox, half, kGap, hx, hy)
        && StashMoveAllMod::ButtonOnTarget(kSortBox, Box(hx - half.left, hy - half.up, hx + half.right, hy + half.down), kGap);
    // The first creation of a session, before any extents are measured, sits
    // at a provisional origin: a box of Sort's own size about its centre.
    double px = 0, py = 0;
    ok = ok && StashMoveAllMod::ButtonOrigin(kSortBox, StashMoveAllMod::ProvisionalExtents(kSortBox), kGap, px, py)
        && Near(px, 2303.5 - 8 - 91.2, 0.05) && Near(py, 1230.25, 0.05);
    // The line for a node still off target after the second creation: said
    // once, the offsets to a tenth, and the mod stays on.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    // (dy is -31.35 on paper, so its last digit is left to the rounding.)
    const std::string off = mod.ButtonOffTarget(kSortBox, kOldNodeBox, kGap);
    const std::string head = "stashmoveall: button - placed -83.6,-31.", tail = " off beside Sort; F4 still works";
    ok = ok && off.rfind(head, 0) == 0 && off.size() == head.size() + 1 + tail.size()
        && off.compare(off.size() - tail.size(), tail.size(), tail) == 0
        && mod.ButtonOffTarget(kSortBox, kOldNodeBox, kGap).empty() && mod.IsEnabled();
    // Negative controls: a Sort box that did not read gives no origin; a node
    // box that did not read is never on target.
    double nx = 0, ny = 0;
    ok = ok && !StashMoveAllMod::ButtonOrigin(StashMoveBox(), e, kGap, nx, ny)
        && !StashMoveAllMod::ButtonOnTarget(kSortBox, StashMoveBox(), kGap);
    Check("target/button_right_edge_sits_the_gap_left_of_sort_centred_on_it", ok,
          std::to_string(x) + "," + std::to_string(y) + " " + off);
}

static void TargetOldButtonOriginPutItsCornerInsideTheTargetBox()
{
    // The owner's report, reproduced from the measured numbers: the old
    // origin centred the node on the point meant for its top-left corner, so
    // its bottom-right corner (2211.9, 1221.7) lies inside the box it should
    // occupy. The old box is off target; the new one is on it.
    StashMoveExtents e;
    StashMoveAllMod::ExtentsOf(kOldNodeX, kOldNodeY, kOldNodeBox, e);
    double x = 0, y = 0;
    StashMoveAllMod::ButtonOrigin(kSortBox, e, kGap, x, y);
    const StashMoveBox target = Box(x - e.left, y - e.up, x + e.right, y + e.down);
    const double cx = kOldNodeX + e.right, cy = kOldNodeY + e.down;
    bool ok = Near(cx, 2211.9, 0.05) && Near(cy, 1221.7, 0.05)
        && StashMoveAllMod::PressInNode(cx, cy, target.left, target.top, target.right, target.bottom)
        && !StashMoveAllMod::ButtonOnTarget(kSortBox, kOldNodeBox, kGap)
        && StashMoveAllMod::ButtonOnTarget(kSortBox, target, kGap);
    // Negative control: the old box's own top-left is outside the target box.
    ok = ok && !StashMoveAllMod::PressInNode(kOldNodeBox.left, kOldNodeBox.top, target.left, target.top, target.right, target.bottom);
    Check("target/old_button_origin_put_its_corner_inside_the_target_box", ok, "");
}

static bool Has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static void TargetButtonIsCheckedOnItsSettledBoxNotTheCreationFrame()
{
    // Review of #131 round 0 (instrument blindness): a box read in the frame
    // UiCreateNode returned is not known to be the node's settled box (Live
    // 1f: visible=0 in the reply, 1 a frame later), so the place is checked
    // on later ensure steps only, once the node reads visible and its box
    // reads the same on two steps in a row. Here the box changes between the
    // creation and the first settled read: an early read that happens to sit
    // on target is never taken as the answer.
    using Check_ = ForgePact::StashMoveButtonCheck;
    StashMoveExtents e;
    StashMoveAllMod::ExtentsOf(kOldNodeX, kOldNodeY, kOldNodeBox, e);
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    bool ok = Has(mod.StateLine(), " button_place=none button_box=none button_extents=none button_makes=0 button_step=0");
    // The first make of a session: at the provisional origin.
    double px = 0, py = 0;
    ok = ok && StashMoveAllMod::ButtonOrigin(kSortBox, mod.ButtonExtents(kSortBox), kGap, px, py)
        && Near(px, 2303.5 - 8 - 91.2, 0.05) && Near(py, 1230.25, 0.05);
    mod.NoteButtonMade(false);
    ok = ok && Has(mod.StateLine(), " button_place=pending") && Has(mod.StateLine(), " button_makes=1");
    double tx = 0, ty = 0;
    StashMoveAllMod::ButtonOrigin(kSortBox, e, kGap, tx, ty);
    const StashMoveBox onTarget = Box(tx - e.left, ty - e.up, tx + e.right, ty + e.down);
    const StashMoveBox settled = Box(px - e.left, py - e.up, px + e.right, py + e.down);   // 7.6 right of its place
    double x = 0, y = 0;
    std::string line;
    // Not visible yet; then visible once with the early box; then the box
    // changes: none of these is a settled read, so nothing is decided.
    ok = ok && mod.ButtonCheck(false, kSortBox, px, py, onTarget, kGap, x, y, line) == Check_::Keep && line.empty()
        && mod.ButtonCheck(true, kSortBox, px, py, onTarget, kGap, x, y, line) == Check_::Keep && line.empty()
        && mod.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Keep && line.empty()
        && Has(mod.StateLine(), " button_place=pending");
    // The same box twice: settled, off target, so it is made again at the
    // origin its own measured extents give.
    ok = ok && mod.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Remake && line.empty()
        && Near(x, 2196.7, 0.05) && Near(y, 1230.25, 0.05)
        && Has(mod.StateLine(), " button_place=remake") && Has(mod.StateLine(), " button_extents=96.9,22.8,98.8,22.8")
        && Has(mod.StateLine(), " button_step=4");
    mod.NoteButtonMade(true);
    // The new node's check starts from nothing: the last node's extents are
    // not shown as this one's (the review of #131 round 1).
    ok = ok && Has(mod.StateLine(), " button_place=pending button_box=none button_extents=none button_makes=2");
    const StashMoveBox good = Box(x - e.left, y - e.up, x + e.right, y + e.down);
    ok = ok && mod.ButtonCheck(false, kSortBox, x, y, good, kGap, x, y, line) == Check_::Keep && line.empty()
        && mod.ButtonCheck(true, kSortBox, x, y, good, kGap, x, y, line) == Check_::Keep && line.empty();
    // Settled on target: said once, positively, with the box it read.
    ok = ok && mod.ButtonCheck(true, kSortBox, x, y, good, kGap, x, y, line) == Check_::Keep
        && line.rfind("stashmoveall: button - placed beside Sort, box 2099.8,", 0) == 0
        && Has(mod.StateLine(), " button_place=on button_box=2099.8,") && Has(mod.StateLine(), " button_makes=2 button_step=3");
    const std::string placed = line;
    // Checked: later steps read nothing more and say nothing.
    ok = ok && mod.ButtonCheck(true, kSortBox, x, y, kOldNodeBox, kGap, x, y, line) == Check_::Keep && line.empty()
        && Has(mod.StateLine(), " button_place=on");
    // The next stash open makes it straight at the measured extents, and
    // checks that node again; the placed line is not said twice.
    double nx = 0, ny = 0;
    ok = ok && StashMoveAllMod::ButtonOrigin(kSortBox, mod.ButtonExtents(kSortBox), kGap, nx, ny)
        && Near(nx, 2196.7, 0.05) && Near(ny, 1230.25, 0.05);
    mod.NoteButtonMade(false);
    ok = ok && Has(mod.StateLine(), " button_place=pending button_box=none button_extents=none")
        && mod.ButtonCheck(true, kSortBox, nx, ny, good, kGap, x, y, line) == Check_::Keep
        && mod.ButtonCheck(true, kSortBox, nx, ny, good, kGap, x, y, line) == Check_::Keep && line.empty()
        && Has(mod.StateLine(), " button_place=on button_box=2099.8,");

    // Negative control: a box that never settles is never judged nor made
    // again; after kButtonSettleSteps it is said unchecked, once.
    StashMoveAllMod drift;
    drift.SetEnabled(true);
    drift.NoteButtonMade(false);
    bool remade = false;
    int said = 0;
    std::string unsettled;
    for (int i = 0; i < StashMoveAllMod::kButtonSettleSteps + 3; ++i) {
        const StashMoveBox moving = Box(settled.left + i, settled.top, settled.right + i, settled.bottom);
        remade = remade || drift.ButtonCheck(true, kSortBox, px, py, moving, kGap, x, y, line) == Check_::Remake;
        if (!line.empty()) { ++said; unsettled = line; }
    }
    ok = ok && !remade && said == 1 && Has(unsettled, "had not settled") && Has(unsettled, "; F4 still works")
        && Has(drift.StateLine(), " button_place=unsettled") && drift.IsEnabled();
    // A later node of that session which settles off target still says so:
    // each cause is said once on its own, so an early unsettled line does not
    // hide a real misplacement (the review of #131 round 1).
    drift.NoteButtonMade(false);
    ok = ok && drift.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Keep
        && drift.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Remake;
    drift.NoteButtonMade(true);
    ok = ok && drift.ButtonCheck(true, kSortBox, x, y, kOldNodeBox, kGap, x, y, line) == Check_::Keep && line.empty()
        && drift.ButtonCheck(true, kSortBox, x, y, kOldNodeBox, kGap, x, y, line) == Check_::Keep
        && Has(line, " off beside Sort; F4 still works") && Has(drift.StateLine(), " button_place=off");
    // Negative control: no third make. Still off after the remake, the node
    // is kept and said once, and the mod stays on.
    StashMoveAllMod off;
    off.SetEnabled(true);
    off.NoteButtonMade(false);
    ok = ok && off.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Keep
        && off.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Remake;
    off.NoteButtonMade(true);
    ok = ok && off.ButtonCheck(true, kSortBox, x, y, kOldNodeBox, kGap, x, y, line) == Check_::Keep && line.empty()
        && off.ButtonCheck(true, kSortBox, x, y, kOldNodeBox, kGap, x, y, line) == Check_::Keep
        && line.rfind("stashmoveall: button - placed -83.6,", 0) == 0 && Has(line, " off beside Sort; F4 still works")
        && Has(off.StateLine(), " button_place=off") && off.IsEnabled()
        && off.ButtonCheck(true, kSortBox, x, y, kOldNodeBox, kGap, x, y, line) == Check_::Keep && line.empty();
    // Negative control: a remake UiRemoveNode could not carry out (the node
    // still held, no second make) is not asked for again: said off instead.
    StashMoveAllMod kept;
    kept.SetEnabled(true);
    kept.NoteButtonMade(false);
    kept.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line);
    ok = ok && kept.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Remake
        && kept.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Keep
        && Has(line, " off beside Sort; F4 still works") && Has(kept.StateLine(), " button_place=off");
    // Negative control: no node made, nothing to check.
    StashMoveAllMod none;
    none.SetEnabled(true);
    ok = ok && none.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Keep && line.empty()
        && none.ButtonCheck(true, kSortBox, px, py, settled, kGap, x, y, line) == Check_::Keep && line.empty();
    Check("target/button_is_checked_on_its_settled_box_not_the_creation_frame", ok, placed + " | " + unsettled);
}

// ---- #131, owner scope 2026-09-30: the button takes Sort's look and size ---

// Live 1 of this workorder (the capture's button-placed row, 2560x1440 GUI):
// Sort's x, y (its top-left) and bbox, 192x66, and the node the #131 origin
// made, wearing its own sprite, 206x48.
static const StashMoveBox kLive1Sort = Box(2290.0, 1262.0, 2482.0, 1328.0);
static const double kLive1SortX = 2290.0, kLive1SortY = 1262.0;
static const StashMoveBox kLive1Node = Box(2076.0, 1271.0, 2282.0, 1319.0);
static const double kLive1NodeX = 2178.0, kLive1NodeY = 1295.0;

static void BaselineLive1NodeOfAnotherSizeSatBesideSort()
{
    // What Live 1 measured: the place was right (right edge 8 left of Sort,
    // the centres level) and the size was not Sort's: 14 wider, 18 lower.
    bool ok = StashMoveAllMod::ButtonOnTarget(kLive1Sort, kLive1Node, kGap)
        && Near((kLive1Node.right - kLive1Node.left) - (kLive1Sort.right - kLive1Sort.left), 14.0, 0.05)
        && Near((kLive1Sort.bottom - kLive1Sort.top) - (kLive1Node.bottom - kLive1Node.top), 18.0, 0.05)
        && !StashMoveAllMod::ButtonSortSized(kLive1Sort, kLive1Node);
    // Sort's origin is its top-left, the node's within one unit of its centre.
    ok = ok && Near(kLive1SortX, kLive1Sort.left, 0.05) && Near(kLive1SortY, kLive1Sort.top, 0.05)
        && Near(kLive1NodeX, (kLive1Node.left + kLive1Node.right) / 2, 1.0)
        && Near(kLive1NodeY, (kLive1Node.top + kLive1Node.bottom) / 2, 1.0);
    // Positive control: Sort's own box is Sort-sized.
    ok = ok && StashMoveAllMod::ButtonSortSized(kLive1Sort, kLive1Sort);
    Check("baseline/live1_node_of_another_size_sat_beside_sort", ok, "");
}

static void TargetFirstNodeMadeWithSortsOwnExtentsLandsOnTarget()
{
    using Check_ = ForgePact::StashMoveButtonCheck;
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    // Before any node is measured, the extents are Sort's own about its
    // origin (its x, y and bbox, read by name): 0, 0, 192, 66, so the origin
    // is 2090, 1262 - Sort's left less 8 less its width, Sort's top.
    const StashMoveExtents e = mod.ButtonExtents(kLive1Sort, kLive1SortX, kLive1SortY);
    double x = 0, y = 0;
    bool ok = Near(e.left, 0.0, 0.05) && Near(e.up, 0.0, 0.05) && Near(e.right, 192.0, 0.05) && Near(e.down, 66.0, 0.05)
        && StashMoveAllMod::ButtonOrigin(kLive1Sort, e, kGap, x, y) && Near(x, 2090.0, 0.05) && Near(y, 1262.0, 0.05);
    // Made there wearing Sort's look, it settles at 2090,1262,2282,1328: on
    // target and Sort-sized with one make, said once on one line.
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(StashMoveButtonLook::Sort);
    ok = ok && Has(mod.StateLine(), " button_look=sort button_size=none");
    const StashMoveBox wearing = Box(2090.0, 1262.0, 2282.0, 1328.0);
    double rx = 0, ry = 0;
    std::string line;
    ok = ok && mod.ButtonCheck(true, kLive1Sort, x, y, wearing, kGap, rx, ry, line) == Check_::Keep && line.empty()
        && mod.ButtonCheck(true, kLive1Sort, x, y, wearing, kGap, rx, ry, line) == Check_::Keep
        && line == "stashmoveall: button - placed beside Sort, box 2090.0,1262.0,2282.0,1328.0"
        && Has(mod.StateLine(), " button_place=on button_box=2090.0,1262.0,2282.0,1328.0 button_extents=0.0,0.0,192.0,66.0"
                                " button_makes=1 button_step=2 button_look=sort button_size=192.0x66.0")
        && mod.IsEnabled();
    // After a measurement the node's own extents are used, whatever Sort's
    // x, y read: a node of another size still lands right.
    StashMoveAllMod other;
    other.SetEnabled(true);
    other.NoteButtonMade(false);
    other.NoteButtonLook(StashMoveButtonLook::Differs);
    other.ButtonCheck(true, kLive1Sort, kLive1NodeX, kLive1NodeY, kLive1Node, kGap, rx, ry, line);
    other.ButtonCheck(true, kLive1Sort, kLive1NodeX, kLive1NodeY, kLive1Node, kGap, rx, ry, line);
    const StashMoveExtents m = other.ButtonExtents(kLive1Sort, kLive1SortX, kLive1SortY);
    double mx = 0, my = 0;
    ok = ok && Near(m.left, 102.0, 0.05) && Near(m.up, 24.0, 0.05) && Near(m.right, 104.0, 0.05) && Near(m.down, 24.0, 0.05)
        && StashMoveAllMod::ButtonOrigin(kLive1Sort, m, kGap, mx, my) && Near(mx, 2178.0, 0.05) && Near(my, 1295.0, 0.05);
    // Negative control: Sort's x, y unread - the provisional box of Sort's
    // size about its centre stays the fallback.
    StashMoveAllMod fresh;
    const StashMoveExtents p = fresh.ButtonExtents(kLive1Sort, std::nan(""), kLive1SortY);
    ok = ok && Near(p.left, 96.0, 0.05) && Near(p.right, 96.0, 0.05) && Near(p.up, 33.0, 0.05) && Near(p.down, 33.0, 0.05);
    Check("target/first_node_made_with_sorts_own_extents_lands_on_target", ok, line);
}

static void TargetButtonSizeWithinOneOfSortsIsSortSized()
{
    // Within kButtonTolerance on width and height is Sort-sized.
    bool ok = StashMoveAllMod::ButtonSortSized(kLive1Sort, Box(2089.6, 1262.2, 2282.0, 1327.8))   // 192.4x65.6
        && !StashMoveAllMod::ButtonSortSized(kLive1Sort, kLive1Node)                              // 206x48
        && !StashMoveAllMod::ButtonSortSized(kLive1Sort, Box(2088.5, 1262.0, 2282.0, 1328.0))     // 193.5 wide
        && !StashMoveAllMod::ButtonSortSized(kLive1Sort, Box(2090.0, 1262.0, 2282.0, 1329.5));    // 67.5 high
    // Negative controls: a box that did not read never is, the node's or Sort's.
    ok = ok && !StashMoveAllMod::ButtonSortSized(kLive1Sort, StashMoveBox())
        && !StashMoveAllMod::ButtonSortSized(StashMoveBox(), kLive1Sort)
        && !StashMoveAllMod::ButtonSortSized(kLive1Sort, Box(2090.0, std::nan(""), 2282.0, 1328.0));
    Check("target/button_size_within_one_of_sorts_is_sort_sized", ok, "");
}

static void TargetButtonOfAnotherSizeIsKeptAndSaidOnce()
{
    using Check_ = ForgePact::StashMoveButtonCheck;
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(StashMoveButtonLook::Sort);
    double x = 0, y = 0;
    std::string line;
    // Settles at Live 1's box: on target, so kept - never a remake for its
    // size - and the size said once, on a line of its own after the placed one.
    const std::string size = "stashmoveall: button - its size 206.0x48.0 is not the Sort button's 192.0x66.0, "
                             "so it is kept as it is; F4 still works";
    bool ok = mod.ButtonCheck(true, kLive1Sort, kLive1NodeX, kLive1NodeY, kLive1Node, kGap, x, y, line) == Check_::Keep
        && mod.ButtonCheck(true, kLive1Sort, kLive1NodeX, kLive1NodeY, kLive1Node, kGap, x, y, line) == Check_::Keep
        && line == "stashmoveall: button - placed beside Sort, box 2076.0,1271.0,2282.0,1319.0\n" + size
        && Has(mod.StateLine(), " button_place=on") && Has(mod.StateLine(), " button_makes=1")
        && Has(mod.StateLine(), " button_look=sort button_size=206.0x48.0") && mod.IsEnabled();
    const std::string first = line;
    // A second node of that size (the next stash open) is silent.
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(StashMoveButtonLook::Sort);
    ok = ok && Has(mod.StateLine(), " button_look=sort button_size=none")
        && mod.ButtonCheck(true, kLive1Sort, kLive1NodeX, kLive1NodeY, kLive1Node, kGap, x, y, line) == Check_::Keep
        && mod.ButtonCheck(true, kLive1Sort, kLive1NodeX, kLive1NodeY, kLive1Node, kGap, x, y, line) == Check_::Keep
        && line.empty() && Has(mod.StateLine(), " button_size=206.0x48.0") && mod.IsEnabled();
    // Negative control: a Sort-sized node on target says no size line.
    StashMoveAllMod sized;
    sized.SetEnabled(true);
    sized.NoteButtonMade(false);
    sized.NoteButtonLook(StashMoveButtonLook::Sort);
    const StashMoveBox wearing = Box(2090.0, 1262.0, 2282.0, 1328.0);
    sized.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line);
    sized.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line);
    ok = ok && !Has(line, "its size") && !Has(line, "\n");
    Check("target/button_of_another_size_is_kept_and_said_once", ok, first);
}

static void TargetButtonLookNotTakenIsKeptAndSaidOnce()
{
    using Check_ = ForgePact::StashMoveButtonCheck;
    const StashMoveBox wearing = Box(2090.0, 1262.0, 2282.0, 1328.0);
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    mod.NoteButtonMade(false);
    // The look read back as Sort's when it was written, then not on the
    // later ensure steps (the UI layer putting its own back, say): the look
    // judged is the one on the settled read.
    mod.NoteButtonLook(StashMoveButtonLook::Sort);
    double x = 0, y = 0;
    std::string line;
    bool ok = mod.ButtonLookWanted();
    mod.NoteButtonLook(StashMoveButtonLook::Differs);
    ok = ok && mod.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line) == Check_::Keep;
    mod.NoteButtonLook(StashMoveButtonLook::Differs);
    const std::string look = "stashmoveall: button - it did not take the Sort button's look, so it is kept with its "
                             "own; F4 still works";
    ok = ok && mod.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line) == Check_::Keep
        && line == "stashmoveall: button - placed beside Sort, box 2090.0,1262.0,2282.0,1328.0\n" + look
        && Has(mod.StateLine(), " button_place=on") && Has(mod.StateLine(), " button_look=differs button_size=192.0x66.0")
        && mod.IsEnabled() && !mod.ButtonLookWanted();
    const std::string first = line;
    // Decided: a later read changes nothing.
    mod.NoteButtonLook(StashMoveButtonLook::Sort);
    ok = ok && Has(mod.StateLine(), " button_look=differs");
    // A second node whose look did not take is silent.
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(StashMoveButtonLook::Differs);
    mod.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line);
    ok = ok && mod.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line) == Check_::Keep
        && line.empty() && Has(mod.StateLine(), " button_look=differs") && mod.IsEnabled();
    // A look that could not be read is said once on its own line, apart
    // from the not-taken one, which does not hide it.
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(StashMoveButtonLook::Unread);
    mod.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line);
    ok = ok && mod.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line) == Check_::Keep
        && line == "stashmoveall: button - its look beside Sort could not be read, so it is unchecked; F4 still works"
        && Has(mod.StateLine(), " button_look=unread") && mod.IsEnabled();
    // Negative control: a look that took says no look line.
    StashMoveAllMod took;
    took.SetEnabled(true);
    took.NoteButtonMade(false);
    took.NoteButtonLook(StashMoveButtonLook::Sort);
    took.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line);
    took.ButtonCheck(true, kLive1Sort, 2090.0, 1262.0, wearing, kGap, x, y, line);
    ok = ok && !Has(line, "look") && Has(took.StateLine(), " button_look=sort");
    Check("target/button_look_not_taken_is_kept_and_said_once", ok, first);
}

// ---- #131, owner 2026-09-30: the Mercenary button's box ---------------------

// Live 5 (docs/stash-move-research.md § Live 5 results, 2560x1440 GUI): with
// the bag open on its own, the backpack's Sort (InventorySort, x, y its
// top-left) and the game's own Mercenary button beside it, the same size,
// its right edge 4 left of Sort's; and the node the Sort rule made with the
// stash open, its right edge 8 left of Sort's.
static const StashMoveBox kLive5Sort = Box(2290.0, 1262.0, 2482.0, 1328.0);
static const double kLive5SortX = 2290.0, kLive5SortY = 1262.0;
static const StashMoveBox kLive5Merc = Box(2094.0, 1262.0, 2286.0, 1328.0);
static const StashMoveBox kLive5Node = Box(2090.0, 1262.0, 2282.0, 1328.0);

static bool SameSides(const StashMoveBox& a, const StashMoveBox& b, double tol)
{
    return Near(a.left, b.left, tol) && Near(a.top, b.top, tol) && Near(a.right, b.right, tol) && Near(a.bottom, b.bottom, tol);
}

static void BaselineSortGapBoxIsNotTheMercenaryBox()
{
    // The rule the node was made to until now (Sort-sized, right edge 8 left
    // of Sort, centred on it) gives the box Live 5's node read, and the old
    // check calls it on target; the Mercenary box is 4 further right, more
    // than kButtonTolerance, so the owner's place is not that box.
    const StashMoveBox rule = StashMoveAllMod::SortRuleBox(kLive5Sort, kGap);
    bool ok = SameSides(rule, kLive5Node, 0.05) && StashMoveAllMod::ButtonOnTarget(kLive5Sort, kLive5Node, kGap)
        && StashMoveAllMod::OnTarget(rule, kLive5Node);
    double dx = 0, dy = 0;
    ok = ok && StashMoveAllMod::TargetOffset(kLive5Merc, kLive5Node, dx, dy) && Near(dx, -4.0, 0.05) && Near(dy, 0.0, 0.05)
        && !StashMoveAllMod::OnTarget(kLive5Merc, kLive5Node) && !StashMoveAllMod::OnTarget(kLive5Merc, rule);
    // Both are Sort's size: the difference is the place, not the size.
    ok = ok && StashMoveAllMod::TargetSized(kLive5Sort, kLive5Merc) && StashMoveAllMod::TargetSized(kLive5Merc, kLive5Node);
    // Positive control: the Mercenary box is on its own target.
    ok = ok && StashMoveAllMod::OnTarget(kLive5Merc, kLive5Merc);
    Check("baseline/sort_gap_box_is_not_the_mercenary_box", ok, StashMoveAllMod::BoxText(rule));
}

static void TargetButtonSettlesOnTheMercenaryBox()
{
    using Check_ = ForgePact::StashMoveButtonCheck;
    using Ref = ForgePact::StashMoveButtonRef;
    // merc-route: relation. The Mercenary button is not listed while the
    // stash is open (Live 5), so the target is Sort's box moved and sized by
    // the fractions Live 5 measured: the Mercenary box, from Sort's alone.
    StashMoveBox t;
    bool ok = StashMoveAllMod::ButtonTarget(Ref::Relation, kLive5Sort, StashMoveBox(), kGap, t) == Ref::Relation
        && SameSides(t, kLive5Merc, 1e-6);
    // The fractions follow the GUI scale: Sort at Live 1f/1g's 182.4 wide
    // gives a box of its width whose right edge is 3.8 left of it.
    StashMoveBox small;
    ok = ok && StashMoveAllMod::ButtonTarget(Ref::Relation, kSortBox, StashMoveBox(), kGap, small) == Ref::Relation
        && Near(kSortBox.left - small.right, 3.8, 0.05) && StashMoveAllMod::TargetSized(kSortBox, small)
        && Near(small.top, kSortBox.top, 1e-6);
    // merc-route: live, had it been measured: the box read wins.
    StashMoveBox live;
    ok = ok && StashMoveAllMod::ButtonTarget(Ref::Mercenary, kLive5Sort, kLive5Merc, kGap, live) == Ref::Mercenary
        && SameSides(live, kLive5Merc, 1e-6);
    // Made from Sort's own extents (the node wears Sort's sprite), the
    // origin is the Mercenary box's top-left.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    ok = ok && Has(mod.StateLine(), " button_ref=none");
    const StashMoveExtents e = mod.ButtonExtentsFor(kLive5Sort, t, kLive5SortX, kLive5SortY);
    double x = 0, y = 0;
    ok = ok && Near(e.left, 0.0, 0.05) && Near(e.up, 0.0, 0.05) && Near(e.right, 192.0, 0.05) && Near(e.down, 66.0, 0.05)
        && StashMoveAllMod::TargetOrigin(t, e, x, y) && Near(x, 2094.0, 0.05) && Near(y, 1262.0, 0.05);
    // It settles there: on target with one make, said once, the route on
    // the state line.
    ok = ok && mod.NoteButtonRef(Ref::Relation).empty();
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(StashMoveButtonLook::Sort);
    double rx = 0, ry = 0;
    std::string line;
    ok = ok && mod.ButtonCheck(true, kLive5Sort, t, Ref::Relation, x, y, kLive5Merc, rx, ry, line) == Check_::Keep && line.empty()
        && mod.ButtonCheck(true, kLive5Sort, t, Ref::Relation, x, y, kLive5Merc, rx, ry, line) == Check_::Keep
        && line == "stashmoveall: button - placed in the Mercenary button's place, box 2094.0,1262.0,2286.0,1328.0"
        && Has(mod.StateLine(), " button_place=on button_box=2094.0,1262.0,2286.0,1328.0 button_extents=0.0,0.0,192.0,66.0"
                                " button_makes=1 button_step=2 button_look=sort button_size=192.0x66.0 button_ref=relation")
        && mod.IsEnabled();
    const std::string placed = line;
    // Negative control: a node that settles on the old rule's box is off
    // this target and made again once, at the Mercenary box's top-left.
    StashMoveAllMod old;
    old.SetEnabled(true);
    old.NoteButtonMade(false);
    old.NoteButtonLook(StashMoveButtonLook::Sort);
    ok = ok && old.ButtonCheck(true, kLive5Sort, t, Ref::Relation, 2090.0, 1262.0, kLive5Node, rx, ry, line) == Check_::Keep
        && old.ButtonCheck(true, kLive5Sort, t, Ref::Relation, 2090.0, 1262.0, kLive5Node, rx, ry, line) == Check_::Remake
        && line.empty() && Near(rx, 2094.0, 0.05) && Near(ry, 1262.0, 0.05) && Has(old.StateLine(), " button_place=remake");
    old.NoteButtonMade(true);
    ok = ok && old.ButtonCheck(true, kLive5Sort, t, Ref::Relation, rx, ry, kLive5Merc, x, y, line) == Check_::Keep
        && old.ButtonCheck(true, kLive5Sort, t, Ref::Relation, rx, ry, kLive5Merc, x, y, line) == Check_::Keep
        && line.rfind("stashmoveall: button - placed in the Mercenary button's place, box 2094.0,", 0) == 0
        && Has(old.StateLine(), " button_place=on") && Has(old.StateLine(), " button_makes=2");
    // Still off after the remake: kept and said once against this target.
    StashMoveAllMod off;
    off.SetEnabled(true);
    off.NoteButtonMade(false);
    off.ButtonCheck(true, kLive5Sort, t, Ref::Relation, 2090.0, 1262.0, kLive5Node, rx, ry, line);
    off.ButtonCheck(true, kLive5Sort, t, Ref::Relation, 2090.0, 1262.0, kLive5Node, rx, ry, line);
    off.NoteButtonMade(true);
    off.ButtonCheck(true, kLive5Sort, t, Ref::Relation, rx, ry, kLive5Node, x, y, line);
    ok = ok && off.ButtonCheck(true, kLive5Sort, t, Ref::Relation, rx, ry, kLive5Node, x, y, line) == Check_::Keep
        && line == "stashmoveall: button - placed -4.0,0.0 off the Mercenary button's place; F4 still works"
        && Has(off.StateLine(), " button_place=off") && off.IsEnabled();
    // A target not Sort's size (the owner default: the Mercenary box wins):
    // the scale is Sort's times target over Sort per axis, the extents to
    // make it with follow, and a Sort-sized node is not its size, said once.
    const StashMoveBox wide = Box(2000.0, 1262.0, 2288.0, 1328.0);   // 288 wide, 66 high
    double sx = 0, sy = 0;
    ok = ok && StashMoveAllMod::ButtonScale(kLive5Sort, wide, sx, sy) && Near(sx, 1.5, 1e-9) && Near(sy, 1.0, 1e-9);
    StashMoveAllMod scaled;
    const StashMoveExtents w = scaled.ButtonExtentsFor(kLive5Sort, wide, kLive5SortX, kLive5SortY);
    ok = ok && Near(w.left, 0.0, 0.05) && Near(w.right, 288.0, 0.05) && Near(w.down, 66.0, 0.05);
    scaled.SetEnabled(true);
    scaled.NoteButtonMade(false);
    scaled.NoteButtonLook(StashMoveButtonLook::Sort);
    const StashMoveBox sortSized = Box(2096.0, 1262.0, 2288.0, 1328.0);
    scaled.ButtonCheck(true, kLive5Sort, wide, Ref::Relation, 2096.0, 1262.0, sortSized, rx, ry, line);
    ok = ok && scaled.ButtonCheck(true, kLive5Sort, wide, Ref::Relation, 2096.0, 1262.0, sortSized, rx, ry, line) == Check_::Keep
        && Has(line, "stashmoveall: button - its size 192.0x66.0 is not the Mercenary button's 288.0x66.0, so it is kept "
                     "as it is; F4 still works");
    // Negative controls: no scale from a box that did not read or has no
    // size; a Sort box that did not read gives no target.
    StashMoveBox none;
    ok = ok && !StashMoveAllMod::ButtonScale(kLive5Sort, StashMoveBox(), sx, sy)
        && !StashMoveAllMod::ButtonScale(Box(2290.0, 1262.0, 2290.0, 1328.0), wide, sx, sy)
        && StashMoveAllMod::ButtonTarget(Ref::Relation, StashMoveBox(), kLive5Merc, kGap, none) == Ref::None
        && !StashMoveAllMod::BoxReads(none);
    Check("target/button_settles_on_the_mercenary_box", ok, placed);
}

static void TargetUnreadTargetFallsBackToTheSortRule()
{
    using Check_ = ForgePact::StashMoveButtonCheck;
    using Ref = ForgePact::StashMoveButtonRef;
    // merc-route: live with no Mercenary box read, and the relation from a
    // Sort box of no width: each falls back to the old rule's box.
    StashMoveBox t;
    bool ok = StashMoveAllMod::ButtonTarget(Ref::Mercenary, kLive5Sort, StashMoveBox(), kGap, t) == Ref::Sort
        && SameSides(t, kLive5Node, 0.05);
    StashMoveBox flat;
    const StashMoveBox noWidth = Box(2290.0, 1262.0, 2290.0, 1328.0);
    ok = ok && StashMoveAllMod::ButtonTarget(Ref::Relation, noWidth, StashMoveBox(), kGap, flat) == Ref::Sort
        && Near(flat.right, 2282.0, 0.05);
    // Said once a session, the mod on, the state line naming the rule.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    const std::string said = mod.NoteButtonRef(Ref::Sort);
    ok = ok && said == "stashmoveall: button - the Mercenary button's place could not be worked out, so it sits beside "
                       "Sort by the old rule; F4 still works"
        && mod.NoteButtonRef(Ref::Sort).empty() && Has(mod.StateLine(), " button_ref=sort") && mod.IsEnabled();
    // It is then checked against that rule's box, and said placed beside Sort.
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(StashMoveButtonLook::Sort);
    double x = 0, y = 0;
    std::string line;
    mod.ButtonCheck(true, kLive5Sort, t, Ref::Sort, 2090.0, 1262.0, kLive5Node, x, y, line);
    ok = ok && mod.ButtonCheck(true, kLive5Sort, t, Ref::Sort, 2090.0, 1262.0, kLive5Node, x, y, line) == Check_::Keep
        && line == "stashmoveall: button - placed beside Sort, box 2090.0,1262.0,2282.0,1328.0"
        && Has(mod.StateLine(), " button_place=on") && Has(mod.StateLine(), " button_ref=sort");
    // Negative controls: a route that gave its box says nothing; a Sort box
    // that did not read gives no target and says nothing (making the node is
    // refused on its own line).
    StashMoveAllMod quiet;
    StashMoveBox none;
    ok = ok && quiet.NoteButtonRef(Ref::Relation).empty() && quiet.NoteButtonRef(Ref::Mercenary).empty()
        && StashMoveAllMod::ButtonTarget(Ref::Mercenary, StashMoveBox(), StashMoveBox(), kGap, none) == Ref::None
        && quiet.NoteButtonRef(Ref::None).empty() && Has(quiet.StateLine(), " button_ref=none");
    Check("target/unread_target_falls_back_to_the_sort_rule", ok, said);
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
    ok = ok && mod.StateLine() == "stashmoveall: state=on key=F4" + kIdleButton;
    StashMoveView mats = MixedView(-4);
    mats.cells[0].destinationStacks = Stacks(kRoomyStack);
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
    const StashMoveItem first = StashMoveAllMod::RouteAtUse(p.items[0], stashTab, Sum(0));
    ok = ok && first.route == StashMoveRoute::Cell;
    StashMoveResult one;
    ok = ok && StashMoveAllMod::MayCall(first, 1, one);
    ok = ok && mod.Record(t, StashMoveAllMod::Decide(first, PlacedCell(0, 0)));
    // The second: the first's 5 units are on the tab now.
    const StashMoveItem second = StashMoveAllMod::RouteAtUse(p.items[1], stashTab, Sum(5));
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
    const StashMoveItem many = StashMoveAllMod::RouteAtUse(p.items[1], stashTab, Sum(5), noWhole);
    StashMoveItem unit = p.items[1];
    unit.cell.count = 1;
    ok = ok && many.route == StashMoveRoute::None && many.refusal == "whole-stack merge not measured"
        && StashMoveAllMod::RouteAtUse(unit, stashTab, Sum(5), noWhole).route == StashMoveRoute::Stack;
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
    ok = ok && StashMoveAllMod::RouteAtUse(it, StashMoveAllMod::kMaterialsTab, Sum(0), Flipped(false, false, false, true)).refusal
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
    const StashMoveItem atUse = StashMoveAllMod::RouteAtUse(it, 3, Sum(-1));
    bool ok = atUse.route == StashMoveRoute::None && atUse.refusal == "its stack on the shown tab could not be read";
    StashMoveResult skip;
    ok = ok && !StashMoveAllMod::MayCall(atUse, 1, skip) && skip.outcome == StashMoveOutcome::Skipped
        && skip.answer == "its stack on the shown tab could not be read";
    StashMoveItem stacked = it;
    stacked.route = StashMoveRoute::Stack;
    ok = ok && StashMoveAllMod::RouteAtUse(stacked, 3, Sum(-1)).route == StashMoveRoute::None;
    // A count that did not read is never merged: the merge passes it. Since
    // #131 the cell route passes it too, so it is never placed either.
    StashMoveItem noCount = it;
    noCount.cell.count = -1;
    ok = ok && StashMoveAllMod::RouteAtUse(noCount, 3, Sum(4)).route == StashMoveRoute::None
        && StashMoveAllMod::RouteAtUse(noCount, 3, Sum(0)).route == StashMoveRoute::None;
    // One stack of the list that could not be read makes the whole list unread.
    ok = ok && StashMoveAllMod::RouteAtUse(it, 3, Stacks({4, -1})).refusal == "its stack on the shown tab could not be read";
    StashMoveTally t;
    ok = ok && mod.Record(t, skip) && t.skipped == 1 && !t.stopped && mod.IsEnabled();
    // Negative controls: a sum of 0 is a cell; a non-stackable needs no sum;
    // a planned skip stays a skip with its own reason.
    ok = ok && StashMoveAllMod::RouteAtUse(it, 3, Sum(0)).route == StashMoveRoute::Cell;
    StashMoveItem ring;
    ring.cell = Cell(1, 0, "0-0-85-7", 7);
    ring.route = StashMoveRoute::Cell;
    ok = ok && StashMoveAllMod::RouteAtUse(ring, 3, Sum(-1)).route == StashMoveRoute::Cell;
    StashMoveItem refused;
    refused.cell = Cell(2, 0, "0-0-86-7", 7);
    refused.refusal = "not taken by the Materials tab";
    const StashMoveItem still = StashMoveAllMod::RouteAtUse(refused, StashMoveAllMod::kMaterialsTab, Sum(3));
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
        && mod.StateLine().rfind("stashmoveall: state=off-for-this-session" + kIdleButton + " reason=item 0-0-87-18: ", 0) == 0;
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

static void TargetButtonPressIsALeftPressInsideTheNodeBbox()
{
    // buttonRoute: poll (Live 1g): the node's activation is left undefined,
    // and a left press whose GUI point lies inside the node's bbox, read at
    // that frame, is the button press. The bbox is inclusive, as the game's
    // own sides are; a side that did not read (NaN) or a box turned inside
    // out is never a press.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    // Live 1g's node sat left of Sort; these are the shape of its box.
    const double l = 520, t = 600, r = 600, b = 632;
    bool ok = StashMoveAllMod::PressInNode(560, 616, l, t, r, b)
        && StashMoveAllMod::PressInNode(l, t, l, t, r, b) && StashMoveAllMod::PressInNode(r, b, l, t, r, b);
    // Negative controls: just outside each side, Sort's box beside it, and
    // the panel background (Live 1g node-press-negative).
    ok = ok && !StashMoveAllMod::PressInNode(l - 1, 616, l, t, r, b) && !StashMoveAllMod::PressInNode(r + 1, 616, l, t, r, b)
        && !StashMoveAllMod::PressInNode(560, t - 1, l, t, r, b) && !StashMoveAllMod::PressInNode(560, b + 1, l, t, r, b)
        && !StashMoveAllMod::PressInNode(640, 616, l, t, r, b) && !StashMoveAllMod::PressInNode(300, 200, l, t, r, b);
    ok = ok && !StashMoveAllMod::PressInNode(560, 616, nan, t, r, b) && !StashMoveAllMod::PressInNode(nan, 616, l, t, r, b)
        && !StashMoveAllMod::PressInNode(560, 616, r, t, l, b);
    Check("target/button_press_is_a_left_press_inside_the_node_bbox", ok, "");
}

static void TargetButtonRefusalIsReportedOnceAndKeepsTheModOn()
{
    // A node the adapter could not create (UiCreateNode refused it, the Sort
    // row did not read) is reported once, is not tried again while the stash
    // stays open, and never turns the mod off: F4 keeps working.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    bool ok = mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Create;
    const std::string first = mod.ButtonRefused("UiCreateNode answered undefined");
    const std::string again = mod.ButtonRefused("UiCreateNode answered undefined");
    ok = ok && first == "stashmoveall: button - UiCreateNode answered undefined; F4 still works" && again.empty();
    ok = ok && mod.IsEnabled() && !mod.OffThisSession() && mod.StateLine() == "stashmoveall: state=on key=F4" + kIdleButton;
    ok = ok && mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Keep;
    // F4 still starts a run.
    mod.KeyEdge(false, true, true, false);
    ok = ok && mod.KeyEdge(true, true, true, false);
    // The stash closed and opened again: one more try, and one more line if
    // it is refused again.
    ok = ok && mod.ButtonStep(false, false, false, false) == StashMoveButtonStep::Keep
        && mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Create
        && !mod.ButtonRefused("the Sort row's x, y or bbox did not read").empty();
    // So does the switch turned off and on again.
    mod.SetEnabled(false);
    mod.SetEnabled(true);
    ok = ok && mod.ButtonStep(true, true, true, false) == StashMoveButtonStep::Create;
    // Negative control: a node that exists is kept, refusal or not.
    ok = ok && mod.ButtonStep(true, true, true, true) == StashMoveButtonStep::Keep;
    Check("target/button_refusal_is_reported_once_and_keeps_the_mod_on", ok, first + " | " + again);
}

static void TargetButtonCountersNameWhereAPressWent()
{
    // Round-0 review of Phase C (instrument blindness): a click on the button
    // that moved nothing must say why on the state line, since the player
    // build carries no probe. poll-blind is presses=0 while the node is held;
    // a bbox miss is presses above in_node (outside, or a point or box that
    // did not read); a poll that threw is errors; a guard drop is dropped,
    // with the last reason. The state word stays first, for the panel.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double l = 520, t = 600, r = 600, b = 632;
    StashMoveAllMod mod;
    const std::string off0 = "stashmoveall: state=off key=F4 button=none presses=0 in_node=0 outside=0 unread=0 errors=0"
                             " taken=0 dropped=0 last_drop=none" + kIdlePlace;
    bool ok = mod.StateLine() == off0;
    mod.SetEnabled(true);
    mod.NoteButtonHeld(true);
    const std::string blind = "stashmoveall: state=on key=F4 button=held presses=0 in_node=0 outside=0 unread=0 errors=0"
                              " taken=0 dropped=0 last_drop=none" + kIdlePlace;
    ok = ok && mod.StateLine() == blind;
    // The misses: outside the box, a box that did not read, a box inside
    // out, a mouse point that did not read; and a poll that threw.
    mod.NoteButtonMiss(StashMoveAllMod::PressReads(640, 616, l, t, r, b));
    mod.NoteButtonMiss(StashMoveAllMod::PressReads(560, 616, nan, t, r, b));
    mod.NoteButtonMiss(StashMoveAllMod::PressReads(560, 616, r, t, l, b));
    mod.NoteButtonMiss(StashMoveAllMod::PressReads(nan, 616, l, t, r, b));
    mod.NoteButtonPollError();
    // Presses inside: dropped by each guard in turn, then one taken; a take
    // with no press recorded counts nothing.
    mod.NoteButtonPress();
    const bool fg = mod.TakeButtonPress(false, false, false);
    mod.NoteButtonPress();
    const bool stash = mod.TakeButtonPress(true, false, false);
    mod.NoteButtonPress();
    const bool held = mod.TakeButtonPress(true, true, true);
    mod.NoteButtonPress();
    const bool taken = mod.TakeButtonPress(true, true, false);
    const bool nothing = mod.TakeButtonPress(true, true, false);
    ok = ok && !fg && !stash && !held && taken && !nothing;
    const std::string after = "stashmoveall: state=on key=F4 button=held presses=8 in_node=4 outside=1 unread=3 errors=1"
                              " taken=1 dropped=3 last_drop=modifier" + kIdlePlace;
    ok = ok && mod.StateLine() == after;
    // A press recorded, then the switch off before the tick took it: dropped
    // as off. The node gone: button=none, the counts kept for the session.
    mod.NoteButtonPress();
    mod.SetEnabled(false);
    const bool whileOff = mod.TakeButtonPress(true, true, false);
    mod.NoteButtonHeld(false);
    const std::string offAfter = "stashmoveall: state=off key=F4 button=none presses=9 in_node=5 outside=1 unread=3 errors=1"
                                 " taken=1 dropped=4 last_drop=off" + kIdlePlace;
    ok = ok && !whileOff && mod.StateLine() == offAfter;
    // Negative control: PressReads is true for a readable point and box
    // whether the point is inside or not.
    ok = ok && StashMoveAllMod::PressReads(560, 616, l, t, r, b) && StashMoveAllMod::PressReads(640, 616, l, t, r, b);
    Check("target/button_counters_name_where_a_press_went", ok, mod.StateLine());
}

static void TargetOffForThisSessionStateLineKeepsTheButtonFields()
{
    // After a loss the state line still carries the button's fields, so a
    // click that ended in a loss can be read from the one line the operator
    // and a bug report see. The reason goes last: it is free text.
    StashMoveAllMod mod;
    mod.SetEnabled(true);
    mod.NoteButtonHeld(true);
    mod.NoteButtonPress();
    const bool taken = mod.TakeButtonPress(true, true, false);
    StashMovePlan p = mod.Plan(MixedView(1));
    StashMoveTally t = mod.Begin(p);
    bool ok = mod.Record(t, StashMoveAllMod::Decide(p.items[0], PlacedCell(0, 0)));
    StashMoveReport half = PlacedCell(1, 0);
    half.sourceHasKey = 1;
    const StashMoveResult loss = StashMoveAllMod::Decide(p.items[1], half);
    ok = ok && taken && loss.outcome == StashMoveOutcome::Unconfirmed && !mod.Record(t, loss) && mod.OffThisSession();
    mod.NoteButtonHeld(false);
    const std::string want = "stashmoveall: state=off-for-this-session button=none presses=1 in_node=1 outside=0"
                             " unread=0 errors=0 taken=1 dropped=0 last_drop=none" + kIdlePlace + " reason=" + mod.OffReason();
    ok = ok && !mod.OffReason().empty() && mod.StateLine() == want;
    // The state word stays first, for the panel.
    ok = ok && mod.StateLine().rfind("stashmoveall: state=off-for-this-session ", 0) == 0;
    // Negative control: switched off by hand, the line is plain off with the
    // key and the same fields, and no reason.
    StashMoveAllMod byHand;
    byHand.SetEnabled(true);
    byHand.SetEnabled(false);
    ok = ok && byHand.StateLine() == "stashmoveall: state=off key=F4" + kIdleButton
        && byHand.StateLine().find(" reason=") == std::string::npos;
    Check("target/off_for_this_session_state_line_keeps_the_button_fields", ok, mod.StateLine());
}

// ---- #131, fix2's round 2: the look copy never stops on a member's kind -----

using ForgePact::StashMoveLookKind;
using ForgePact::StashMoveLookPut;
using ForgePact::StashMoveLookSame;
using ForgePact::StashMoveLookStep;
using ForgePact::StashMoveLookTally;
using ForgePact::StashMoveLookValue;
using ForgePact::StashMoveLookWrite;

// A member's value as the adapter hands it to the core, one per kind the
// runtime returns for these members (a number, a bool, a string, an asset
// reference, undefined).
static StashMoveLookValue LookNumber(double v)
{
    StashMoveLookValue r;
    r.kind = StashMoveLookKind::Number;
    r.number = v;
    return r;
}

static StashMoveLookValue LookBool(bool b)
{
    StashMoveLookValue r;
    r.kind = StashMoveLookKind::Bool;
    r.number = b ? 1.0 : 0.0;
    return r;
}

static StashMoveLookValue LookString(const std::string& s)
{
    StashMoveLookValue r;
    r.kind = StashMoveLookKind::String;
    r.text = s;
    return r;
}

static StashMoveLookValue LookAsset(double index, const std::string& printed)
{
    StashMoveLookValue r;
    r.kind = StashMoveLookKind::Asset;
    r.number = index;
    r.text = printed;
    return r;
}

static StashMoveLookValue LookUndefined()
{
    StashMoveLookValue r;
    r.kind = StashMoveLookKind::Undefined;
    return r;
}

struct LookMember {
    std::string        name;
    StashMoveLookWrite how;
    StashMoveLookValue sort;   // what it reads off InventorySort
};

// The adapter's 16 members (kSmaLookVars/kSmaLookWrites) with what Live 5
// read off InventorySort (docs/stash-move-research.md § Decision
// buttonLabel): the 13 as read, the two scales scaled. The sprite's index is
// a fixture (Live 4 read the name, not the index); `textFont` is handed in,
// since whether it reads as a string or a font reference is not established.
static std::vector<LookMember> Live5Look(const StashMoveLookValue& textFont)
{
    const StashMoveLookWrite as = StashMoveLookWrite::AsRead;
    return {
        {"sprite_index", as, LookAsset(1502, "ref sprite Inventory_Tab_Button_Solid_spr")},
        {"image_xscale", StashMoveLookWrite::ScaleX, LookNumber(1.0)},
        {"image_yscale", StashMoveLookWrite::ScaleY, LookNumber(1.0)},
        {"textFont", as, textFont},
        {"dropShadow", as, LookBool(false)},
        {"createX", as, LookNumber(2290)},
        {"drawXOffset", as, LookNumber(48)},
        {"drawYOffset", as, LookNumber(9)},
        {"navBboxX", as, LookNumber(2290)},
        {"navBboxY", as, LookNumber(1262)},
        {"navBboxWidth", as, LookNumber(192)},
        {"navBboxHeight", as, LookNumber(66)},
        {"naviDown", as, LookBool(false)},
        {"naviDownPrev", as, LookBool(false)},
        {"naviRight", as, LookBool(false)},
        {"naviRightPrev", as, LookBool(false)},
    };
}

// The mod's node as the copy leaves it: each member as it reads, and the
// members written, in order. Before the copy it holds the node's own (Live
// 5: `textFont` __newfont6, `drawXOffset` 0, ...); a member the copy does not
// write keeps its own.
struct FakeLookNode {
    std::map<std::string, StashMoveLookValue> members;
    std::vector<std::string>                  written;
};

// A stand-in for the adapter's loop (SmaButtonLook in ModuleMain.cpp, which
// cannot be compiled here: it calls the runtime), the same shape: every
// member is read off Sort, the core's LookStep says what to write, the write
// is made, the member is read back (or as `back` says, the game putting its
// own back, say) and the core's LookCompare goes into the tally. The verdict
// is the tally's, after the whole list.
static StashMoveLookTally CopyLook(const std::vector<LookMember>& list, FakeLookNode& node, double sx, double sy,
                                   const std::map<std::string, StashMoveLookValue>& back = {})
{
    StashMoveLookTally t;
    for (const LookMember& m : list) {
        const StashMoveLookStep step = StashMoveAllMod::LookStep(m.sort, m.how, sx, sy);
        if (step.put == StashMoveLookPut::AsRead) node.members[m.name] = m.sort;
        else if (step.put == StashMoveLookPut::Number) node.members[m.name] = LookNumber(step.want.number);
        if (step.put != StashMoveLookPut::Nothing) node.written.push_back(m.name);
        const auto b = back.find(m.name);
        const auto own = node.members.find(m.name);
        const StashMoveLookValue got = b != back.end() ? b->second
            : own != node.members.end() ? own->second : LookUndefined();
        t.Note(m.name, StashMoveAllMod::LookCompare(step.want, got));
    }
    return t;
}

// The node before the copy: Live 5's own values beside Sort's.
static FakeLookNode Live5Node()
{
    FakeLookNode n;
    n.members = {
        {"sprite_index", LookAsset(1502, "ref sprite Inventory_Tab_Button_Solid_spr")},
        {"image_xscale", LookNumber(1.0)}, {"image_yscale", LookNumber(1.0)},
        {"textFont", LookString("__newfont6")}, {"dropShadow", LookBool(true)}, {"createX", LookNumber(2090)},
        {"drawXOffset", LookNumber(0)}, {"drawYOffset", LookNumber(-7)}, {"navBboxX", LookNumber(1988)},
        {"navBboxY", LookNumber(1238)}, {"navBboxWidth", LookNumber(206)}, {"navBboxHeight", LookNumber(48)},
        {"naviDown", LookBool(true)}, {"naviDownPrev", LookBool(true)}, {"naviRight", LookBool(true)},
        {"naviRightPrev", LookBool(true)},
    };
    return n;
}

static std::string Written(const FakeLookNode& n)
{
    std::string s;
    for (const std::string& w : n.written) s += (s.empty() ? "" : ",") + w;
    return s;
}

static const std::string kAllSixteen = "sprite_index,image_xscale,image_yscale,textFont,dropShadow,createX,drawXOffset,"
                                       "drawYOffset,navBboxX,navBboxY,navBboxWidth,navBboxHeight,naviDown,naviDownPrev,"
                                       "naviRight,naviRightPrev";

// Settle a node on the Mercenary box with `look` noted, and return the check's lines.
static std::string SettleWithLook(StashMoveAllMod& mod, const StashMoveLookTally& look)
{
    using Ref = ForgePact::StashMoveButtonRef;
    StashMoveBox t;
    StashMoveAllMod::ButtonTarget(Ref::Relation, kLive5Sort, StashMoveBox(), kGap, t);
    mod.SetEnabled(true);
    mod.NoteButtonMade(false);
    mod.NoteButtonLook(look);
    double x = 0, y = 0;
    std::string line;
    mod.ButtonCheck(true, kLive5Sort, t, Ref::Relation, 2094.0, 1262.0, kLive5Merc, x, y, line);
    mod.ButtonCheck(true, kLive5Sort, t, Ref::Relation, 2094.0, 1262.0, kLive5Merc, x, y, line);
    return line;
}

static void BaselineLookAllNumericMembersCopiedAndReadSort()
{
    // The kinds fix2's copy accepted (numbers, bools, asset references):
    // `textFont` as a font reference. Every member is written, 16 of 16
    // read back the same, and the verdict is sort, as it was.
    FakeLookNode node = Live5Node();
    const StashMoveLookTally t = CopyLook(Live5Look(LookAsset(7, "ref font __newfont2")), node, 1.0, 1.0);
    bool ok = Written(node) == kAllSixteen && t.listed == 16 && t.equal == 16 && t.first.empty()
        && t.Verdict() == StashMoveButtonLook::Sort
        && node.members["drawXOffset"].number == 48 && node.members["navBboxX"].number == 2290
        && node.members["createX"].number == 2290 && node.members["dropShadow"].number == 0;
    // The scales alone are scaled: to a target 1.5 wide, image_xscale is
    // written 1.5; everything else as read, navBboxWidth included.
    FakeLookNode wide = Live5Node();
    const StashMoveLookTally w = CopyLook(Live5Look(LookAsset(7, "ref font __newfont2")), wide, 1.5, 1.0);
    ok = ok && w.Verdict() == StashMoveButtonLook::Sort && Near(wide.members["image_xscale"].number, 1.5, 1e-12)
        && Near(wide.members["image_yscale"].number, 1.0, 1e-12) && wide.members["navBboxWidth"].number == 192;
    // The state line says it, and no look line is said.
    StashMoveAllMod mod;
    const std::string line = SettleWithLook(mod, t);
    ok = ok && Has(mod.StateLine(), " button_look=sort") && Has(mod.StateLine(), " button_look_same=16/16")
        && !Has(line, "look");
    // Negative control: before any node the field reads none.
    StashMoveAllMod idle;
    ok = ok && Has(idle.StateLine(), " button_look_same=none");
    Check("baseline/look_all_numeric_members_copied_and_read_sort", ok, Written(node) + " | " + mod.StateLine());
}

static void TargetLookStringMemberIsCopiedAndComparedAsText()
{
    // `textFont` reads as a string (the probe printed a bare __newfont2).
    // fix2's copy stopped there, before the label offsets; now it is written
    // as read, every member after it is written too, and it compares by text.
    FakeLookNode node = Live5Node();
    const StashMoveLookTally t = CopyLook(Live5Look(LookString("__newfont2")), node, 1.0, 1.0);
    bool ok = Written(node) == kAllSixteen && node.members["textFont"].kind == StashMoveLookKind::String
        && node.members["textFont"].text == "__newfont2" && node.members["drawYOffset"].number == 9
        && node.members["naviRightPrev"].number == 0 && t.equal == 16 && t.Verdict() == StashMoveButtonLook::Sort;
    const StashMoveLookStep step = StashMoveAllMod::LookStep(LookString("__newfont2"), StashMoveLookWrite::AsRead, 1, 1);
    ok = ok && step.put == StashMoveLookPut::AsRead
        && StashMoveAllMod::LookCompare(step.want, LookString("__newfont2")) == StashMoveLookSame::Same;
    // A scaled member that reads as anything but a number is not written
    // and compares unread; the members after it are still written.
    std::vector<LookMember> list = Live5Look(LookString("__newfont2"));
    list[1].sort = LookString("1");
    FakeLookNode odd = Live5Node();
    const StashMoveLookTally o = CopyLook(list, odd, 1.0, 1.0);
    ok = ok && StashMoveAllMod::LookStep(LookString("1"), StashMoveLookWrite::ScaleX, 1, 1).put == StashMoveLookPut::Nothing
        && Written(odd) == "sprite_index,image_yscale,textFont,dropShadow,createX,drawXOffset,drawYOffset,navBboxX,"
                           "navBboxY,navBboxWidth,navBboxHeight,naviDown,naviDownPrev,naviRight,naviRightPrev"
        && o.equal == 15 && o.first == "image_xscale" && o.Verdict() == StashMoveButtonLook::Unread;
    Check("target/look_string_member_is_copied_and_compared_as_text", ok, Written(node));
}

static void TargetLookAssetMemberComparesByItsIndex()
{
    // An asset reference compares by its index: read back as the same
    // reference, or as a plain number of that index, it is the same; the
    // name it prints plays no part.
    const StashMoveLookValue sprite = LookAsset(1502, "ref sprite Inventory_Tab_Button_Solid_spr");
    const StashMoveLookStep step = StashMoveAllMod::LookStep(sprite, StashMoveLookWrite::AsRead, 1, 1);
    bool ok = step.put == StashMoveLookPut::AsRead
        && StashMoveAllMod::LookCompare(step.want, sprite) == StashMoveLookSame::Same
        && StashMoveAllMod::LookCompare(step.want, LookNumber(1502)) == StashMoveLookSame::Same
        && StashMoveAllMod::LookCompare(step.want, LookAsset(1502, "ref sprite 1502")) == StashMoveLookSame::Same;
    // Negative controls: another index differs, even with the same name
    // printed; an index that did not read is unread; a string differs.
    ok = ok && StashMoveAllMod::LookCompare(step.want, LookAsset(1503, sprite.text)) == StashMoveLookSame::Differs
        && StashMoveAllMod::LookCompare(step.want, LookAsset(std::nan(""), sprite.text)) == StashMoveLookSame::Unread
        && StashMoveAllMod::LookCompare(step.want, LookString(sprite.text)) == StashMoveLookSame::Differs;
    // In the copy: the sprite read back as its index is still sort.
    FakeLookNode node = Live5Node();
    const StashMoveLookTally t = CopyLook(Live5Look(LookAsset(7, "ref font __newfont2")), node, 1.0, 1.0,
                                          {{"sprite_index", LookNumber(1502)}, {"textFont", LookNumber(7)}});
    ok = ok && t.equal == 16 && t.Verdict() == StashMoveButtonLook::Sort;
    Check("target/look_asset_member_compares_by_its_index", ok, "");
}

static void TargetLookUnreadMemberIsNamedAfterTheWholeCopy()
{
    // `drawXOffset` reads undefined off Sort (a member missing on some
    // build): it is not written, every other member is, and the verdict is
    // unread only once the whole list has been through, naming it.
    std::vector<LookMember> list = Live5Look(LookString("__newfont2"));
    list[6].sort = LookUndefined();
    FakeLookNode node = Live5Node();
    const StashMoveLookTally t = CopyLook(list, node, 1.0, 1.0);
    bool ok = Written(node) == "sprite_index,image_xscale,image_yscale,textFont,dropShadow,createX,drawYOffset,navBboxX,"
                               "navBboxY,navBboxWidth,navBboxHeight,naviDown,naviDownPrev,naviRight,naviRightPrev"
        && node.members["drawXOffset"].number == 0   // the node's own, untouched
        && t.listed == 16 && t.equal == 15 && t.first == "drawXOffset" && t.firstWas == StashMoveLookSame::Unread
        && t.Verdict() == StashMoveButtonLook::Unread;
    // A member whose read or write threw is noted unread by the adapter's
    // catch, and the members after it still count.
    StashMoveLookTally thrown;
    thrown.Note("sprite_index", StashMoveLookSame::Same);
    thrown.Note("textFont", StashMoveLookSame::Unread);
    thrown.Note("drawXOffset", StashMoveLookSame::Same);
    ok = ok && thrown.listed == 3 && thrown.equal == 2 && thrown.first == "textFont"
        && thrown.Verdict() == StashMoveButtonLook::Unread;
    // Said once, naming the member, and the state line counts the rest.
    StashMoveAllMod mod;
    const std::string line = SettleWithLook(mod, t);
    ok = ok && Has(line, "stashmoveall: button - its look beside Sort could not be read (drawXOffset did not read; "
                         "15/16 members the same), so it is unchecked; F4 still works")
        && Has(mod.StateLine(), " button_look=unread") && Has(mod.StateLine(), " button_look_same=15/16") && mod.IsEnabled();
    // Negative control: nothing listed is never sort.
    ok = ok && StashMoveLookTally().Verdict() == StashMoveButtonLook::Unread;
    Check("target/look_unread_member_is_named_after_the_whole_copy", ok, Written(node) + " | " + line);
}

static void TargetLookDifferingStringReadsDiffers()
{
    // Negative control for the wider kinds: a string that reads back other
    // than it was written (the node's own __newfont6, the game putting it
    // back, say) differs, never sort - accepting strings is not accepting
    // anything.
    FakeLookNode node = Live5Node();
    const StashMoveLookTally t = CopyLook(Live5Look(LookString("__newfont2")), node, 1.0, 1.0,
                                          {{"textFont", LookString("__newfont6")}});
    bool ok = Written(node) == kAllSixteen && t.equal == 15 && t.first == "textFont"
        && t.firstWas == StashMoveLookSame::Differs && t.Verdict() == StashMoveButtonLook::Differs;
    // A string against a number, and a differing string beside an unread
    // member: still differs.
    ok = ok && StashMoveAllMod::LookCompare(LookString("48"), LookNumber(48)) == StashMoveLookSame::Differs
        && StashMoveAllMod::LookCompare(LookString("__newfont2"), LookString("__newfont2 ")) == StashMoveLookSame::Differs;
    StashMoveLookTally both;
    both.Note("textFont", StashMoveLookSame::Differs);
    both.Note("drawXOffset", StashMoveLookSame::Unread);
    ok = ok && both.Verdict() == StashMoveButtonLook::Differs;
    StashMoveAllMod mod;
    const std::string line = SettleWithLook(mod, t);
    ok = ok && Has(line, "stashmoveall: button - it did not take the Sort button's look (textFont differs; 15/16 members "
                         "the same), so it is kept with its own; F4 still works")
        && Has(mod.StateLine(), " button_look=differs") && Has(mod.StateLine(), " button_look_same=15/16");
    Check("target/look_differing_string_reads_differs", ok, line);
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
    BaselineSocketableTabTakesOnlyTheBagSocketView();
    TargetSocketableMergesAnIdentityWithANode();
    TargetSocketableNewKindStaysInTheBag();
    TargetSocketableWholeStackMergesIntoItsOneStack();
    TargetFullSocketableStackNeverStartsASecondStack();
    BaselineGameMergeTakesAStackOnlyWhileTheSumStaysAtTheCap();
    TargetFullMaterialsStackOverflowsIntoAFreeCell();
    TargetMaterialsMergeSkipsTheFullStackForOneWithRoom();
    TargetMergeAtExactlyTheCapIsAMerge();
    TargetNoStackFitsAndNoFreeCellStaysInTheBag();
    TargetFullKeyStackOnAPageOverflowsIntoAFreeCell();
    TargetUnexpectedMergeOnTheCellRouteIsConfirmedAsAMerge();
    BaselineButtonSmallOriginIsItsCentreAndSortOriginItsTopLeft();
    TargetButtonRightEdgeSitsTheGapLeftOfSortCentredOnIt();
    TargetOldButtonOriginPutItsCornerInsideTheTargetBox();
    TargetButtonIsCheckedOnItsSettledBoxNotTheCreationFrame();
    BaselineLive1NodeOfAnotherSizeSatBesideSort();
    TargetFirstNodeMadeWithSortsOwnExtentsLandsOnTarget();
    TargetButtonSizeWithinOneOfSortsIsSortSized();
    TargetButtonOfAnotherSizeIsKeptAndSaidOnce();
    TargetButtonLookNotTakenIsKeptAndSaidOnce();
    BaselineSortGapBoxIsNotTheMercenaryBox();
    TargetButtonSettlesOnTheMercenaryBox();
    TargetUnreadTargetFallsBackToTheSortRule();
    BaselineLookAllNumericMembersCopiedAndReadSort();
    TargetLookStringMemberIsCopiedAndComparedAsText();
    TargetLookAssetMemberComparesByItsIndex();
    TargetLookUnreadMemberIsNamedAfterTheWholeCopy();
    TargetLookDifferingStringReadsDiffers();
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
    TargetButtonPressIsALeftPressInsideTheNodeBbox();
    TargetButtonRefusalIsReportedOnceAndKeepsTheModOn();
    TargetButtonCountersNameWhereAPressWent();
    TargetOffForThisSessionStateLineKeepsTheButtonFields();
    std::cout << (g_Failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return g_Failures ? 1 : 0;
}
