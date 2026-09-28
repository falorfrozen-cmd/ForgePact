#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ForgePact {

// "Move all into the stash" (ForgePact #68). With the stash open and the mod
// on, one key (F4) moves every item on the bag tab on show into the stash tab
// on show, each item by the game's own per-item move called by name, exactly
// as if the player had moved it by hand: a stackable joins a stack of its kind,
// an item the tab has no room for or does not take stays in the bag, and
// nothing is lost, duplicated or left half-moved. Owner's decisions
// (2026-09-27, docs/stash-move-research.md): the game's own move, once per
// item, by name; never a write to a container or a map entry; a refusal the
// game gives skips the item and the run goes on; an outcome the re-read cannot
// confirm stops the run and turns the mod off for the session.
//
// This header decides; the adapter in ModuleMain.cpp touches the game. What it
// decides, each pinned by tests/stash_move_all_harness.cpp:
// - the switch, off by default, and off for the rest of the session after a
//   loss (SetEnabled refuses to turn it back on);
// - the hotkey: one run per press, only while the switch is on, the game is
//   the foreground window, the stash window is listed and no modifier is
//   held, so Alt+F4 closing the game starts nothing (KeyEdge);
// - the in-game Move all button: whether its node should exist (on, the stash
//   window and the bag's Sort button listed, Sort visible) and a press the
//   adapter records, taken once by the frame tick under the key's own guard
//   (ButtonStep, NoteButtonPress, TakeButtonPress);
// - the plan over the shown bag tab's occupied cells: each item once, in
//   row-major order (row, then column) by its first cell, so a multi-cell
//   item is planned by its top-left cell (Plan);
// - the sources: the bag page on show (tabSelected 0..4) for every stash tab
//   the run takes; the bag's Materials view (-4) for the Materials tab only;
//   the bag's Socket view (-2) for the Socketable tab only, and only while a
//   socketable path is measured; a stash page from a bag sub-tab is refused
//   as unmeasured (Plan);
// - the route per item for the shown stash tab: a grid tab (0, 1..19) takes
//   anything through the tab routine, or through the stack routine when a
//   stack of the item's identity is already there; the Materials tab (-4)
//   takes only class 14 and the Socketable tab (-2) only class 15: onto the
//   stack of the item's identity when there is one, else into a cell through
//   the tab placement, each only where kMeasuredRoutes says the research
//   reproduced it by name (a route not reproduced is a planned skip); any
//   other class there is a skip that calls nothing; the Unique tab (-5), the
//   Socketable tab while no socketable path is measured, and any number not
//   listed here refuse the run (TabOf, Plan);
// - the route again at the point of use: a stackable's route is decided once
//   more from the stack sum the adapter re-reads on the shown tab just before
//   its call, whatever the plan said, because an earlier item of the same run
//   may have made that stack (round-2 review: two bag items of one identity
//   the tab lacked duplicated a unit) - a sum above 0 merges, 0 places, a sum
//   that could not be read is a skip (RouteAtUse);
// - never overflow (the owner's 2026-09-28 rule): before each item's call the
//   adapter re-reads the shown stash tab's room for it - a free block of the
//   item's footprint on the shown tab's own cells (Room), or a stack of its
//   identity there - and an item the shown tab has no room for (or whose room
//   could not be read) is a skip that calls nothing, so it stays in the bag
//   and every other stash tab is left as it was (MayCall);
// - the outcome of each item from the adapter's re-reads (Decide): moved only
//   when the stash tab on show is still the planned one, the source cell no
//   longer holds the key, and either the destination holds it (a cell) or the
//   stack rose by exactly the item's count (a stack), and, where the route
//   ends with the owner step, the key answering nothing on map 0 after it;
//   skipped when the game answered no and every side read unchanged;
//   unconfirmed otherwise, a read that could not be made, a shown tab that
//   changed and an owner step that did not take included. Only the
//   shown tab is re-read: the other stash tabs have no container readable by
//   name (RUNTIME_DATA_MODELS § 17), so no-spill rests on the route - each
//   routine is handed only the shown tab's array - and on the room check
//   (owner, 2026-09-28, "Accept");
// - that a skip continues and an unconfirmed item stops the run and turns the
//   mod off for the session, keeping the reason (Record);
// - the lines: one per item, one per run, the refusal and loss lines, and the
//   switch and state lines, each starting with its verb; after a loss the
//   state line reads off-for-this-session with the reason, never plain off.
//
// It is game-independent on purpose: it names no runtime interface, builtin,
// log call or runtime value type, so tests/stash_move_all_harness.cpp compiles
// it whole with no runtime stub. The item classes are plain numbers here
// (hs-game-sdk's ItemType: 14 materials, 15 socketables); the adapter passes
// the class it read.

// What kind of stash tab a tab number names.
enum class StashMoveTab : int { Unsupported = 0, Grid, Materials, Socketable, Unique };

// How one item goes: not at all (a skip planned before any call), onto a
// stack through the game's stack routine, or into a cell through its tab
// routine.
enum class StashMoveRoute : int { None = 0, Stack, Cell };

enum class StashMoveOutcome : int { Moved = 1, Skipped, Unconfirmed };

// What the frame tick does with the in-game button's node this frame.
enum class StashMoveButtonStep : int { Keep = 0, Create, Remove };

// One occupied cell of the bag tab on show, as the adapter read it.
struct StashMoveCell {
    int         x = -1;
    int         y = -1;
    std::string key;                       // the item's fingerprint, its map 0 key
    int         itemClass = -1;            // the item's class (itemType)
    bool        stackable = false;
    int64_t     count = 1;                 // the stack count; 1 for a single item
    bool        destinationHasStack = false;  // the shown stash tab holds a stack of this identity
    // False when the shown tab holds an item whose identity could not be
    // read (a shared page's entries answer on no map by name), so "no stack
    // of this identity" cannot be told from "one the read missed".
    bool        destinationStackRead = true;
};

// What the adapter read before a run: whether the stash window is listed, and
// the two tabs on show (kUnreadTab when a tab could not be read).
struct StashMoveView {
    bool                       stashListed = false;
    int                        bagTab = -1000;
    int                        stashTab = -1000;
    std::vector<StashMoveCell> cells;
};

struct StashMoveItem {
    StashMoveCell  cell;      // its first cell (row-major)
    int            width = 1; // its footprint in the bag: the columns and rows
    int            height = 1;//   its cells cover
    StashMoveRoute route = StashMoveRoute::None;
    std::string    refusal;   // why a None route is skipped; empty otherwise
};

// The routes the research reproduced by name, each from its line in
// docs/stash-move-research.md § Decision (Live 1e, recorded in round A'8).
// A route that is false here is not called: the Socketable tab is refused
// while neither socketable path is measured, a new identity on a special tab
// and a merge of more than one unit are planned skips. Fixed at build time,
// never a setting; PlanWith takes another value only so the tests can pin
// what each rule does when it is the other way.
struct StashMoveRoutes {
    bool socketNew = false;       // socketRoute new: not-observed (by hand only)
    bool socketMerge = false;     // socketRoute merge: not-observed (by hand only)
    bool newMaterial = true;      // newMaterialRoute: byname
    bool wholeStackMerge = true;  // wholeStackMerge: byname (on the Materials tab)
};

// The shown stash tab's own cells as the adapter read them, [row][col] like
// the game's nodeGrid ([y][x]); rows = 0 when they could not be read.
struct StashMoveGrid {
    int               rows = 0;
    int               cols = 0;
    std::vector<char> filled;   // rows * cols, row-major: 1 a cell holds an item
};

struct StashMovePlan {
    bool                       refused = false;
    std::string                reason;   // off, no stash window, no shown tab, unsupported ... tab <n>, nothing to move
    int                        bagTab = -1000;
    int                        stashTab = -1000;
    std::vector<StashMoveItem> items;
};

// How one item went, as the adapter saw it: whether the game's routine was
// dispatched and what it answered, then both sides re-read. A read that was
// not made, or failed, is -1 - never 0 or false: "unknown" must not compare
// equal to "empty".
struct StashMoveReport {
    bool        answered = false;      // the routine was dispatched and returned
    bool        accepted = false;      // its answer says it placed the item
    std::string answer;                // the answer as text, for the lines
    int         sourceHasKey = -1;     // 1 the bag cell still holds the key, 0 it is empty
    int         destinationHasKey = -1;// 1 the stash tab holds the key (cell route)
    int         destinationX = -1;
    int         destinationY = -1;
    int64_t     stackBefore = -1;      // the destination stack's count (stack route)
    int64_t     stackAfter = -1;
    int         shownTabChanged = -1;  // 1 stashTabSelected moved off the planned tab, 0 unchanged
    // The placement's follow-ups (cell route), so "ran and did nothing" is
    // told apart from success: the second ValidateItem's answer, and the owner
    // step 0 to 9 a shared page and a new Materials identity end with.
    std::string validateAnswer;        // ValidateItem's answer (self the stash grid), as text
    int         ownerStep = 0;         // 1 the route ends with the owner step, 0 it runs none
    int         ownerDispatched = -1;  // 1 ChangeItemOwner was dispatched and returned, 0 not, -1 not tried
    std::string ownerAnswer;           // its answer, or why it was not dispatched
    int         keyOnMap0 = -1;        // after it: 1 the key still answers on map 0, 0 nothing there, -1 unread
};

struct StashMoveResult {
    StashMoveOutcome outcome = StashMoveOutcome::Unconfirmed;
    StashMoveRoute   route = StashMoveRoute::None;
    std::string      key;
    int              x = -1;
    int              y = -1;
    std::string      answer;   // the game's answer (Skipped) or what did not agree (Unconfirmed)
};

// One run's running count and its lines, in order.
struct StashMoveTally {
    int                      bagTab = -1000;
    int                      stashTab = -1000;
    int                      planned = 0;
    int                      moved = 0;
    int                      skipped = 0;
    bool                     stopped = false;
    std::vector<std::string> lines;
};

class StashMoveAllMod {
public:
    // A tab the adapter could not read.
    static constexpr int kUnreadTab = -1000;
    static constexpr int kMaterialClass = 14;
    static constexpr int kSocketClass = 15;
    // The bag's page tabs (Main, then four Extra), and the two sub-tabs that
    // can be a source: Materials for the Materials tab, Socket for the
    // Socketable tab.
    static constexpr int kBagPageTabs = 5;
    static constexpr int kBagMaterialsView = -4;
    static constexpr int kBagSocketView = -2;
    static constexpr int kMaterialsTab = -4;
    static constexpr int kSocketableTab = -2;
    static constexpr int kLastGridTab = 19;
    static constexpr StashMoveRoutes kMeasuredRoutes{};

    static StashMoveAllMod& Instance() {
        static StashMoveAllMod s_Instance;
        return s_Instance;
    }

    // Public so the harness can build a fresh instance per scenario; the
    // plugin only ever uses Instance().
    StashMoveAllMod() = default;

    bool IsEnabled() const { return m_Enabled.load(); }
    bool OffThisSession() const { return m_OffThisSession.load(); }

    // Turning on is refused (false) once a loss has turned the mod off for the
    // session; turning off is always allowed.
    bool SetEnabled(bool on) {
        if (on && m_OffThisSession.load()) return false;
        m_Enabled.store(on);
        return true;
    }

    // One call per frame with the key's state. True on the press that should
    // start a run: the key went down while the switch is on, the game is the
    // foreground window, the stash window is listed and no modifier (Alt,
    // Ctrl, Shift) is held - Alt+F4 closes the game, and a run started as it
    // closes would move items the stash's own close never saves. Off,
    // nothing; the key's state is still remembered, so turning the switch on
    // while the key is held starts nothing until it is released and pressed
    // again, and releasing a modifier while the key is held is no edge either.
    bool KeyEdge(bool down, bool foreground, bool stashListed, bool modifier) {
        if (!IsEnabled()) { m_KeyWasDown = down; return false; }
        bool press = down && !m_KeyWasDown;
        m_KeyWasDown = down;
        return press && foreground && stashListed && !modifier;
    }

    // ---- the in-game Move all button -----------------------------------------

    // What the frame tick does with the button's node: it exists exactly while
    // the switch is on, the stash window is listed and the bag's Sort button
    // (the node it sits beside) is listed and visible. Off, nothing is
    // created, and a node left from before is removed.
    StashMoveButtonStep ButtonStep(bool stashListed, bool sortListed, bool sortVisible, bool nodeExists) const {
        const bool wanted = IsEnabled() && stashListed && sortListed && sortVisible;
        if (wanted == nodeExists) return StashMoveButtonStep::Keep;
        return wanted ? StashMoveButtonStep::Create : StashMoveButtonStep::Remove;
    }

    // The adapter saw the button pressed (the activation's detour, or the
    // frame poll). Nothing moves here: the press waits for the frame tick,
    // so nothing moves inside a game script call. Off, it is not kept.
    void NoteButtonPress() {
        if (IsEnabled()) m_ButtonPressed.store(true);
    }

    // The frame tick takes a recorded press once, under the key's own guard:
    // the game in front, the stash listed, no modifier held. Presses recorded
    // before one tick are one run; a press the guard refuses is dropped.
    bool TakeButtonPress(bool foreground, bool stashListed, bool modifier) {
        const bool pressed = m_ButtonPressed.exchange(false);
        return pressed && IsEnabled() && foreground && stashListed && !modifier;
    }

    static StashMoveTab TabOf(int tab) {
        if (tab >= 0 && tab <= kLastGridTab) return StashMoveTab::Grid;
        if (tab == -4) return StashMoveTab::Materials;
        if (tab == -2) return StashMoveTab::Socketable;
        if (tab == -5) return StashMoveTab::Unique;
        return StashMoveTab::Unsupported;
    }

    // The items one run moves, on the routes the research measured. Off, or
    // anything the run cannot stand on, refuses the whole run with its reason
    // and plans nothing.
    StashMovePlan Plan(const StashMoveView& view) const {
        return PlanWith(view, kMeasuredRoutes, IsEnabled(), OffThisSession());
    }

    static StashMovePlan PlanWith(const StashMoveView& view, const StashMoveRoutes& routes, bool enabled,
                                  bool offThisSession = false) {
        StashMovePlan plan;
        plan.bagTab = view.bagTab;
        plan.stashTab = view.stashTab;
        auto refuse = [&](const std::string& reason) {
            plan.refused = true;
            plan.reason = reason;
            return plan;
        };
        if (!enabled) return refuse(offThisSession ? "off for this session" : "off");
        if (!view.stashListed) return refuse("no stash window");
        if (view.stashTab == kUnreadTab || view.bagTab == kUnreadTab) return refuse("no shown tab");
        const StashMoveTab tab = TabOf(view.stashTab);
        const bool socketMeasured = routes.socketNew || routes.socketMerge;
        if (tab == StashMoveTab::Unsupported || tab == StashMoveTab::Unique
            || (tab == StashMoveTab::Socketable && !socketMeasured))
            return refuse("unsupported stash tab " + std::to_string(view.stashTab));
        // The source: a bag page for any destination; a sub-tab only for the
        // special tab its moves were measured into.
        const bool page = view.bagTab >= 0 && view.bagTab < kBagPageTabs;
        const bool subTab = view.bagTab == kBagMaterialsView || (view.bagTab == kBagSocketView && socketMeasured);
        if (!page && !subTab) return refuse("unsupported bag tab " + std::to_string(view.bagTab));
        if (!page) {
            const bool fits = (view.bagTab == kBagMaterialsView && tab == StashMoveTab::Materials)
                || (view.bagTab == kBagSocketView && tab == StashMoveTab::Socketable);
            if (!fits) return refuse("unsupported bag tab " + std::to_string(view.bagTab) + " for stash tab "
                                     + std::to_string(view.stashTab));
        }

        std::vector<StashMoveCell> cells = view.cells;
        std::stable_sort(cells.begin(), cells.end(), [](const StashMoveCell& a, const StashMoveCell& b) {
            return a.y != b.y ? a.y < b.y : a.x < b.x;
        });
        std::vector<std::string> seen;
        for (const StashMoveCell& c : cells) {
            if (c.key.empty()) continue;
            if (std::find(seen.begin(), seen.end(), c.key) != seen.end()) continue;
            seen.push_back(c.key);
            StashMoveItem item;
            item.cell = c;
            Footprint(cells, c.key, item.width, item.height);
            if (tab != StashMoveTab::Grid) {
                const bool materials = tab == StashMoveTab::Materials;
                const int mine = materials ? kMaterialClass : kSocketClass;
                if (c.itemClass != mine) {
                    item.refusal = std::string("not taken by the ") + (materials ? "Materials" : "Socketable") + " tab";
                    plan.items.push_back(item);
                    continue;
                }
            }
            RouteFor(tab, c, c.destinationStackRead, c.destinationHasStack, routes, item);
            plan.items.push_back(item);
        }
        if (plan.items.empty()) return refuse("nothing to move");
        return plan;
    }

    // The route of an item the shown tab takes, from what is known of its
    // identity's stack there: `stackRead` false when it could not be read,
    // else `hasStack`. A stackable whose stack is unknown is a skip, since the
    // game's stack routine would merge into one the read missed; a special
    // tab's merge and new-identity placement only where the research
    // reproduced them by name.
    static void RouteFor(StashMoveTab tab, const StashMoveCell& c, bool stackRead, bool hasStack,
                         const StashMoveRoutes& routes, StashMoveItem& item) {
        item.route = StashMoveRoute::None;
        item.refusal.clear();
        const bool many = c.count > 1;
        const char* unread = "its stack on the shown tab could not be read";
        if (tab == StashMoveTab::Grid) {
            if (c.stackable && !stackRead) {
                item.refusal = unread;
            } else if (c.stackable && hasStack) {
                if (many && !routes.wholeStackMerge) item.refusal = "whole-stack merge not measured";
                else item.route = StashMoveRoute::Stack;
            } else {
                item.route = StashMoveRoute::Cell;
            }
            return;
        }
        const bool materials = tab == StashMoveTab::Materials;
        if (!stackRead) {
            item.refusal = unread;
        } else if (hasStack) {
            // The one-unit merge is measured on the Materials tab
            // (stackMoveRoute); the Socketable tab's only by its path.
            if (!materials && !routes.socketMerge) item.refusal = "a socketable merge is not measured";
            else if (many && !routes.wholeStackMerge) item.refusal = "whole-stack merge not measured";
            else item.route = StashMoveRoute::Stack;
        } else {
            const bool placed = materials ? routes.newMaterial : routes.socketNew;
            if (placed) item.route = StashMoveRoute::Cell;
            else item.refusal = "a new kind stays in the bag";
        }
    }

    // The route decided again at the point of use (round-2 review). The plan
    // read the shown tab's stacks once, before the run; an earlier item of the
    // same run may since have made the stack a later item of its identity
    // joins. So just before a stackable's call the adapter re-reads that
    // identity's sum on the shown tab and this decides with it, whatever the
    // plan said: above 0 the stack routine with the item's whole count, 0 the
    // placement, -1 (unread) a skip that calls nothing. `planned.cell.count`
    // is the count re-read at the same moment; one that did not read (below
    // 1) is never merged, since the merge passes it. A planned skip stays one
    // with its own reason, and a non-stackable keeps its placement: it needs
    // no sum.
    static StashMoveItem RouteAtUse(const StashMoveItem& planned, int stashTab, int64_t stackSumNow,
                                    const StashMoveRoutes& routes = kMeasuredRoutes) {
        if (planned.route == StashMoveRoute::None || !planned.cell.stackable) return planned;
        StashMoveItem item = planned;
        const bool read = stackSumNow >= 0 && (stackSumNow == 0 || planned.cell.count >= 1);
        RouteFor(TabOf(stashTab), planned.cell, read, stackSumNow > 0, routes, item);
        return item;
    }

    // The columns and rows one key's cells cover in the bag; 1 by 1 when it
    // has none.
    static void Footprint(const std::vector<StashMoveCell>& cells, const std::string& key, int& width, int& height) {
        int x0 = 0, x1 = -1, y0 = 0, y1 = -1;
        for (const StashMoveCell& c : cells) {
            if (c.key != key) continue;
            if (x1 < x0) { x0 = x1 = c.x; y0 = y1 = c.y; continue; }
            x0 = (std::min)(x0, c.x); x1 = (std::max)(x1, c.x);
            y0 = (std::min)(y0, c.y); y1 = (std::max)(y1, c.y);
        }
        width = x1 < x0 ? 1 : x1 - x0 + 1;
        height = y1 < y0 ? 1 : y1 - y0 + 1;
    }

    // Whether the shown tab's own cells have a free block `width` columns by
    // `height` rows: 1 yes, 0 no, -1 the cells were not read (never "has
    // room", and never "full" either).
    static int Room(const StashMoveGrid& g, int width, int height) {
        if (g.rows <= 0 || g.cols <= 0 || (int)g.filled.size() != g.rows * g.cols || width < 1 || height < 1) return -1;
        for (int r = 0; r + height <= g.rows; ++r) {
            for (int c = 0; c + width <= g.cols; ++c) {
                bool free = true;
                for (int dr = 0; free && dr < height; ++dr)
                    for (int dc = 0; free && dc < width; ++dc)
                        if (g.filled[(size_t)(r + dr) * g.cols + (c + dc)]) free = false;
                if (free) return 1;
            }
        }
        return 0;
    }

    StashMoveTally Begin(const StashMovePlan& plan) const {
        StashMoveTally t;
        t.bagTab = plan.bagTab;
        t.stashTab = plan.stashTab;
        t.planned = (int)plan.items.size();
        return t;
    }

    // An item the plan skips before any call.
    static StashMoveResult NotAttempted(const StashMoveItem& item) {
        StashMoveResult r;
        r.outcome = StashMoveOutcome::Skipped;
        r.route = StashMoveRoute::None;
        r.key = item.cell.key;
        r.answer = item.refusal.empty() ? "not attempted" : item.refusal;
        return r;
    }

    // Never overflow: whether the game's routine may be called for this item.
    // shownTabHasRoom is the adapter's re-read of the shown stash tab just
    // before the call: 1 it has a free cell for the item's size, or (stack
    // route) a stack of its identity with room; 0 it has neither; -1 the read
    // could not be made. Anything but 1 fills skip with a skip that called
    // nothing, and the item stays in the bag. The array handed to the routine
    // is always the shown tab's, and this check comes first, so an item the
    // shown tab cannot hold never reaches a routine at all (the by-name
    // placement answered no room on a full tab and tried no other, Live 1d
    // byname-full; the game's own quick move did the same, Live 1c hand-full).
    static bool MayCall(const StashMoveItem& item, int shownTabHasRoom, StashMoveResult& skip) {
        if (item.route == StashMoveRoute::None) { skip = NotAttempted(item); return false; }
        if (shownTabHasRoom == 1) return true;
        skip = StashMoveResult();
        skip.outcome = StashMoveOutcome::Skipped;
        skip.route = StashMoveRoute::None;
        skip.key = item.cell.key;
        skip.answer = shownTabHasRoom == 0 ? "no room on the shown tab" : "the shown tab's room could not be read";
        return false;
    }

    // The outcome of one item from the adapter's report.
    static StashMoveResult Decide(const StashMoveItem& item, const StashMoveReport& r) {
        StashMoveResult out;
        out.key = item.cell.key;
        out.route = item.route;
        auto unconfirmed = [&](const std::string& why) {
            out.outcome = StashMoveOutcome::Unconfirmed;
            out.answer = why;
            return out;
        };
        const bool stack = item.route == StashMoveRoute::Stack;
        const std::string said = r.answer.empty() ? std::string("no answer") : r.answer;
        // The re-reads below are of the shown tab's own array: a tab on show
        // that moved off the planned one, or could not be re-read, leaves
        // nothing to confirm them against, whatever the game answered.
        if (r.shownTabChanged == 1)
            return unconfirmed("the game answered " + said + " but the shown stash tab changed during the move");
        if (r.shownTabChanged != 0)
            return unconfirmed("the game answered " + said + " but the shown stash tab could not be re-read");

        if (!r.answered || !r.accepted) {
            // A refusal is a skip only when both sides read unchanged.
            if (r.sourceHasKey != 1) return unconfirmed("the game answered " + said + " but the bag cell was not read holding it");
            if (stack) {
                if (r.stackBefore < 0 || r.stackAfter < 0 || r.stackAfter != r.stackBefore)
                    return unconfirmed("the game answered " + said + " but the stash stack did not read unchanged");
            } else if (r.destinationHasKey != 0) {
                return unconfirmed("the game answered " + said + " but the stash tab was not read without it");
            }
            out.outcome = StashMoveOutcome::Skipped;
            out.answer = said;
            return out;
        }

        if (r.sourceHasKey == 1) return unconfirmed("the game answered " + said + " but the bag cell still holds it");
        if (r.sourceHasKey != 0) return unconfirmed("the game answered " + said + " but the bag cell could not be read");
        if (stack) {
            if (r.stackBefore < 0 || r.stackAfter < 0)
                return unconfirmed("the game answered " + said + " but the stash stack could not be read");
            if (r.stackAfter - r.stackBefore != item.cell.count)
                return unconfirmed("the game answered " + said + " but the stash stack rose by "
                                   + std::to_string(r.stackAfter - r.stackBefore) + ", not "
                                   + std::to_string(item.cell.count));
        } else {
            if (r.destinationHasKey != 1)
                return unconfirmed("the game answered " + said + " but the stash tab was not read holding it");
            // The owner step 0 to 9 a shared page and a new Materials identity
            // end with: measured, the key then answers nothing on map 0. Not
            // dispatched, still answering there, or not looked up, is not a move.
            if (r.ownerStep == 1) {
                const std::string placed = "the game answered " + said + " and placed it, but ";
                if (r.ownerDispatched != 1)
                    return unconfirmed(placed + "the owner step was not dispatched: "
                                       + (r.ownerAnswer.empty() ? std::string("not tried") : r.ownerAnswer));
                if (r.keyOnMap0 == 1)
                    return unconfirmed(placed + "the key still answers on map 0 after the owner step (ChangeItemOwner answered "
                                       + r.ownerAnswer + ")");
                if (r.keyOnMap0 != 0)
                    return unconfirmed(placed + "the key's map 0 lookup after the owner step could not be made");
            }
            out.x = r.destinationX;
            out.y = r.destinationY;
        }
        out.outcome = StashMoveOutcome::Moved;
        out.answer = said;
        if (!r.validateAnswer.empty()) out.answer += "; ValidateItem answered " + r.validateAnswer;
        if (r.ownerStep == 1) out.answer += "; ChangeItemOwner answered " + r.ownerAnswer;
        return out;
    }

    // Count one item into the run and write its line. False when the run
    // stops: an unconfirmed item turns the mod off for the session, and the
    // rest of the plan is not attempted.
    bool Record(StashMoveTally& t, const StashMoveResult& r) {
        if (t.stopped) return false;
        t.lines.push_back(ItemLine(r));
        if (r.outcome == StashMoveOutcome::Moved) { ++t.moved; return true; }
        if (r.outcome == StashMoveOutcome::Skipped) { ++t.skipped; return true; }
        t.stopped = true;
        const std::string reason = "item " + r.key + ": " + r.answer;
        TurnOffForSession(reason);
        t.lines.push_back(LossLine("stashmoveall", reason));
        return false;
    }

    // Turn the mod off for the rest of the session (a loss), keeping why, so
    // the state line and a later `stashmoveall 1` can say it. The first
    // reason is kept.
    void TurnOffForSession(const std::string& reason) {
        if (!m_OffThisSession.load()) m_OffReason = reason;
        m_OffThisSession.store(true);
        m_Enabled.store(false);
    }

    const std::string& OffReason() const { return m_OffReason; }

    // ---- lines ---------------------------------------------------------------

    static std::string Where(const StashMoveResult& r) {
        if (r.outcome == StashMoveOutcome::Moved)
            return r.route == StashMoveRoute::Stack ? std::string("stack")
                                                    : "cell " + std::to_string(r.x) + "," + std::to_string(r.y);
        if (r.outcome == StashMoveOutcome::Skipped) return "skipped: " + r.answer;
        return "unconfirmed: " + r.answer;
    }

    static std::string ItemLine(const StashMoveResult& r) {
        return "stashmoveall: item " + r.key + " -> " + Where(r);
    }

    static std::string SummaryLine(const StashMoveTally& t) {
        return "stashmoveall: moved " + std::to_string(t.moved) + " of " + std::to_string(t.planned)
            + " from bag tab " + std::to_string(t.bagTab) + " to stash tab " + std::to_string(t.stashTab)
            + "; skipped " + std::to_string(t.skipped) + (t.stopped ? "; stopped" : "");
    }

    // `stashmove <fingerprint>`'s one line for its one item.
    static std::string SingleLine(const StashMoveResult& r) {
        if (r.outcome == StashMoveOutcome::Moved) return "stashmove: moved " + r.key + " -> " + Where(r);
        if (r.outcome == StashMoveOutcome::Skipped) return "stashmove: not-taken - " + r.answer + "; the item stays in the bag";
        return LossLine("stashmove", "item " + r.key + ": " + r.answer);
    }

    static std::string RefusalLine(const std::string& verb, const std::string& reason) {
        return verb + ": refused - " + reason + "; nothing was called";
    }

    static std::string LossLine(const std::string& verb, const std::string& reason) {
        return verb + ": off for this session - " + reason + "; turn it on again after restarting the game";
    }

    static std::string SwitchLine(bool on) {
        return on ? std::string("stashmoveall: on") : std::string("stashmoveall: off - the stash and bag are unchanged");
    }

    // The state: on, off, or turned off by a loss - which a bug report must
    // be able to tell apart from "switched off" (round-2 review), and which
    // the panel reads from the last of these lines.
    std::string StateLine() const {
        if (OffThisSession()) return "stashmoveall: state=off-for-this-session reason=" + m_OffReason;
        return std::string("stashmoveall: state=") + (IsEnabled() ? "on" : "off") + " key=F4";
    }

    // `stashmoveall 1` after a loss: still off, and why.
    std::string OffForSessionLine() const { return LossLine("stashmoveall", m_OffReason); }

private:
    std::atomic<bool> m_Enabled{false};
    std::atomic<bool> m_OffThisSession{false};
    std::atomic<bool> m_ButtonPressed{false};
    bool              m_KeyWasDown = false;
    std::string       m_OffReason;
};

} // namespace ForgePact
