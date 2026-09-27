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
//   the foreground window and the stash window is listed (KeyEdge);
// - the plan over the shown bag tab's occupied cells: each item once, in
//   row-major order (row, then column) by its first cell, so a multi-cell
//   item is planned by its top-left cell (Plan);
// - the route per item for the shown stash tab: a grid tab (0, 1..19) takes
//   anything through the tab routine, or through the stack routine when a
//   stack of the item's identity is already there; the Materials tab (-4)
//   takes only class 14 and the Socketable tab (-2) only class 15, both
//   through the stack routine; any other class there is a skip that calls
//   nothing; the Unique tab (-5) and any number not listed here refuse the
//   run (TabOf, Plan);
// - the outcome of each item from the adapter's re-reads (Decide): moved only
//   when the source cell no longer holds the key and either the destination
//   holds it (a cell) or the stack rose by exactly the item's count (a stack);
//   skipped when the game answered no and both sides read unchanged;
//   unconfirmed otherwise, a read that could not be made included;
// - that a skip continues and an unconfirmed item stops the run and turns the
//   mod off for the session (Record);
// - the lines: one per item, one per run, the refusal and loss lines, and the
//   switch and state lines, each starting with its verb.
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

// One occupied cell of the bag tab on show, as the adapter read it.
struct StashMoveCell {
    int         x = -1;
    int         y = -1;
    std::string key;                       // the item's fingerprint, its map 0 key
    int         itemClass = -1;            // the item's class (itemType)
    bool        stackable = false;
    int64_t     count = 1;                 // the stack count; 1 for a single item
    bool        destinationHasStack = false;  // the shown stash tab holds a stack of this identity
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
    StashMoveCell  cell;
    StashMoveRoute route = StashMoveRoute::None;
    std::string    refusal;   // why a None route is skipped; empty otherwise
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
    // The bag's page tabs (Main, then four Extra); its sub-tabs are not a
    // source in this version.
    static constexpr int kBagPageTabs = 5;
    static constexpr int kLastGridTab = 19;

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
    // foreground window and the stash window is listed. Off, nothing; the
    // key's state is still remembered, so turning the switch on while the key
    // is held starts nothing until it is released and pressed again.
    bool KeyEdge(bool down, bool foreground, bool stashListed) {
        if (!IsEnabled()) { m_KeyWasDown = down; return false; }
        bool press = down && !m_KeyWasDown;
        m_KeyWasDown = down;
        return press && foreground && stashListed;
    }

    static StashMoveTab TabOf(int tab) {
        if (tab >= 0 && tab <= kLastGridTab) return StashMoveTab::Grid;
        if (tab == -4) return StashMoveTab::Materials;
        if (tab == -2) return StashMoveTab::Socketable;
        if (tab == -5) return StashMoveTab::Unique;
        return StashMoveTab::Unsupported;
    }

    // The items one run moves. Off, or anything the run cannot stand on,
    // refuses the whole run with its reason and plans nothing.
    StashMovePlan Plan(const StashMoveView& view) const {
        StashMovePlan plan;
        plan.bagTab = view.bagTab;
        plan.stashTab = view.stashTab;
        auto refuse = [&](const std::string& reason) {
            plan.refused = true;
            plan.reason = reason;
            return plan;
        };
        if (!IsEnabled()) return refuse(OffThisSession() ? "off for this session" : "off");
        if (!view.stashListed) return refuse("no stash window");
        if (view.stashTab == kUnreadTab || view.bagTab == kUnreadTab) return refuse("no shown tab");
        StashMoveTab tab = TabOf(view.stashTab);
        if (tab == StashMoveTab::Unsupported || tab == StashMoveTab::Unique)
            return refuse("unsupported stash tab " + std::to_string(view.stashTab));
        if (view.bagTab < 0 || view.bagTab >= kBagPageTabs)
            return refuse("unsupported bag tab " + std::to_string(view.bagTab));

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
            if (tab == StashMoveTab::Grid) {
                item.route = (c.stackable && c.destinationHasStack) ? StashMoveRoute::Stack : StashMoveRoute::Cell;
            } else {
                bool materials = tab == StashMoveTab::Materials;
                int mine = materials ? kMaterialClass : kSocketClass;
                if (c.itemClass == mine) item.route = StashMoveRoute::Stack;
                else item.refusal = std::string("not taken by the ") + (materials ? "Materials" : "Socketable") + " tab";
            }
            plan.items.push_back(item);
        }
        if (plan.items.empty()) return refuse("nothing to move");
        return plan;
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
            out.x = r.destinationX;
            out.y = r.destinationY;
        }
        out.outcome = StashMoveOutcome::Moved;
        out.answer = said;
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
        TurnOffForSession();
        t.lines.push_back(LossLine("stashmoveall", "item " + r.key + ": " + r.answer));
        return false;
    }

    // Turn the mod off for the rest of the session (a loss).
    void TurnOffForSession() {
        m_OffThisSession.store(true);
        m_Enabled.store(false);
    }

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

    std::string StateLine() const {
        return std::string("stashmoveall: state=") + (IsEnabled() ? "on" : "off") + " key=F4";
    }

private:
    std::atomic<bool> m_Enabled{false};
    std::atomic<bool> m_OffThisSession{false};
    bool              m_KeyWasDown = false;
};

} // namespace ForgePact
