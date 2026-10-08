#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
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
//   window and the bag's Sort button listed, Sort visible); a press, which is
//   a left press whose GUI point lies inside the node's bbox (buttonRoute:
//   poll, Live 1g: the node has no activation and nothing is hooked for it),
//   recorded by the adapter and taken once by the frame tick under the key's
//   own guard; and a node the adapter could not make, reported once and never
//   turning the mod off (ButtonStep, PressInNode, NoteButtonPress,
//   TakeButtonPress, ButtonRefused); each press the adapter read, where it
//   went and why a recorded one started no run, counted for the state line
//   (NoteButtonHeld, NoteButtonMiss, NoteButtonPollError, ButtonFields);
// - the plan over the shown bag tab's occupied cells: each item once, in
//   row-major order (row, then column) by its first cell, so a multi-cell
//   item is planned by its top-left cell (Plan);
// - the sources: the bag page on show (tabSelected 0..4) for every stash tab
//   the run takes but the Socketable one; the bag's Materials view (-4) for
//   the Materials tab only; the bag's Socket view (-2) for the Socketable tab,
//   and it is that tab's only source (the view every socketable merge was
//   measured from, Live 1f and 1g); a stash page from a bag sub-tab is
//   refused as unmeasured (Plan);
// - the route per item for the shown stash tab, from each stack of the item's
//   identity there and the game's merge rule as a model (ForgePact #131:
//   StackCap, StackThatFits - a stack takes the whole count only while it
//   stays at or below 999, or 999999 with the sixth argument's flag 8): a grid
//   tab (0, 1..19) takes anything through the tab routine, or through the
//   stack routine when a stack of the item's identity has room for its whole
//   count; the Materials tab (-4) takes only class 14 and the Socketable tab
//   (-2) only class 15: onto a stack of the item's identity with room for it,
//   else into a cell through the tab placement - on the Materials tab a new
//   stack also when every stack of its kind is too full, while the Socketable
//   tab holds one stack per kind and a full one is a skip - each only where
//   kMeasuredRoutes says the research reproduced it by name (a route not
//   reproduced is a planned skip); any other class there is a skip that calls
//   nothing; the Unique tab (-5), the Socketable tab while no socketable path
//   is measured, and any number not listed here refuse the run (TabOf, Plan,
//   RouteFor);
// - the route again at the point of use: a stackable's route is decided once
//   more from the stacks the adapter re-reads on the shown tab just before
//   its call, whatever the plan said, because an earlier item of the same run
//   may have made or filled that stack (round-2 review: two bag items of one
//   identity the tab lacked duplicated a unit) - a stack with room merges,
//   none with room places, stacks that could not be read are a skip
//   (RouteAtUse); a true answer on the cell route is decided as a merge
//   (AsMerge);
// - never overflow (the owner's 2026-09-28 rule): before each item's call the
//   adapter re-reads the shown stash tab's room for it - a free block of the
//   item's footprint on the shown tab's own cells (Room), or a stack of its
//   identity there with room for its whole count (StackRoom) - and an item
//   the shown tab has no room for (or whose room could not be read) is a skip
//   that calls nothing, so it stays in the bag and every other stash tab is
//   left as it was (MayCall);
// - the in-game button's place: the column of the bag's page tab above the
//   slot left of Sort, in Sort's row (the owner, 2026-10-02) - the left and
//   right edges of InventoryTab_4 as the adapter read it by name, with Sort's
//   top and bottom; when no such tab reads, the same column worked out from
//   Sort's box by the tab grid's measured relation, said once; the old rule
//   (Sort-sized, its right edge 8 GUI units left of Sort, its vertical centre
//   Sort's) only as the last fallback, said once (ButtonTarget, GridBox,
//   SortRuleBox, NoteButtonRef); the origin from that box and the node's own
//   extents, measured on it (TargetOrigin, OnTarget), checked on later ensure steps
//   once the node's box has settled, never in the frame it was made, with one
//   remake when it is off (ButtonExtentsFor, NoteButtonMade, ButtonCheck), and
//   what the check read on the state line, the target's route and the tab's
//   box it was taken from included;
// - the button's look and size (owner scope, 2026-09-30): the Sort button's
//   own, copied from the Sort node by the adapter (its label's place and font
//   among them, Live 5), its sprite's scale per axis to the target's size, so
//   the first node is made with Sort's extents about Sort's origin; each
//   member written and read back whatever its kind, one that cannot be costing
//   only its own verdict, never the members after it (LookStep, LookCompare,
//   StashMoveLookTally); judged on the same settled read as the place, a node
//   not the target's size or whose look did not take kept and said once each,
//   naming the first member off, never remade for it (TargetSized,
//   ButtonScale, NoteButtonLook);
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
//   state line reads off-for-this-session, the button's fields kept and the
//   reason last, never plain off.
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

// What the ensure step does with a node it holds, from the place check
// (ButtonCheck): nothing, or take it away and make it again at a new origin.
enum class StashMoveButtonCheck : int { Keep = 0, Remake };

// The node's look as the adapter read it after giving it the Sort button's
// own (owner scope, 2026-09-30): none before a node is made, sort when the
// members that carry its sprite, size and label read back the same as
// Sort's, differs when one read and is not, unread when none differs and one
// could not be read or compared (StashMoveLookTally).
enum class StashMoveButtonLook : int { None = 0, Sort, Differs, Unread };

// Which box the node is made to and checked against (the owner, 2026-10-02:
// the column of the Extra tab above the slot, in Sort's row): none while
// Sort's box has not read; tab, InventoryTab_4's left and right edges as read
// with Sort's top and bottom; grid, the same column worked out from Sort's box
// by the tab grid's measured relation, when no tab reads; sort, the old rule
// standing in when neither could be had.
enum class StashMoveButtonRef : int { None = 0, Tab, Grid, Sort };

// One look member's value as the adapter read it, in plain terms (#131, the
// review of fix2's round 2: a member's kind never stops the copy). Its kind,
// classified the way the research probe classifies a value: a number, a
// bool, a string, an asset reference (a handle whose type names an asset),
// undefined, or any other kind (a reference, struct, array or method);
// unread when the read itself threw. What it compares by: a number or bool
// by `number` (a bool 0 or 1), an asset by its index in `number`, a string
// by `text`.
enum class StashMoveLookKind : int { Unread = 0, Number, Bool, String, Asset, Undefined, Other };

struct StashMoveLookValue {
    StashMoveLookKind kind = StashMoveLookKind::Unread;
    double            number = std::nan("");
    std::string       text;
};

// How a look member is written onto the node: as read; scaled on one axis
// by the target's size over Sort's (the sprite's scale, Live 4, and the
// highlight box's size); or displaced on one axis by the target's offset
// from Sort (a member holding an absolute GUI place, which on Sort is Sort's
// own: createX and the highlight box's corner, navBboxX/Y).
enum class StashMoveLookWrite : int { AsRead = 0, ScaleX, ScaleY, ShiftX, ShiftY };

// What the adapter writes for one member (LookStep): nothing, the value as
// it read it off Sort, or a number.
enum class StashMoveLookPut : int { Nothing = 0, AsRead, Number };

// One member read back off the node against what it should read: the same,
// differs, or unread (not read, or of a kind that cannot be compared).
enum class StashMoveLookSame : int { Same = 0, Differs, Unread };

// The look step for one member: what to write and what the node should read
// back as (unread when nothing is written).
struct StashMoveLookStep {
    StashMoveLookPut   put = StashMoveLookPut::Nothing;
    StashMoveLookValue want;
};

// The look copy over the whole list, member by member (Note), and its
// verdict once every member has been written and read back (Verdict): sort
// when every one compared the same, differs when one differs, unread when
// none differs and one could not be read or compared. It carries the count
// that compared the same and the first member that did not, with how.
struct StashMoveLookTally {
    int               listed = 0;
    int               equal = 0;
    bool              differs = false;
    bool              unread = false;
    std::string       first;                      // the first member not the same; empty when every one was
    StashMoveLookSame firstWas = StashMoveLookSame::Same;
    StashMoveButtonLook look = StashMoveButtonLook::None;   // a verdict handed in whole, with no members

    void Note(const std::string& name, StashMoveLookSame same) {
        ++listed;
        if (same == StashMoveLookSame::Same) { ++equal; return; }
        if (same == StashMoveLookSame::Differs) differs = true;
        else unread = true;
        if (first.empty()) { first = name; firstWas = same; }
    }

    StashMoveButtonLook Verdict() const {
        if (listed == 0) return look == StashMoveButtonLook::None ? StashMoveButtonLook::Unread : look;
        if (differs) return StashMoveButtonLook::Differs;
        if (unread) return StashMoveButtonLook::Unread;
        return StashMoveButtonLook::Sort;
    }
};

// The shown stash tab's stacks of one identity, as the adapter read them:
// each stack's count, in the array's order (on the Socketable tab, the one
// node that holds the identity). `read` is false when the array, or any item
// on it, could not be read - a shared page's entries answer on no map by
// name - so "no stack of this identity" is never told from "one the read
// missed": unknown is not empty (ForgePact #131).
struct StashMoveStacks {
    bool                 read = false;
    std::vector<int64_t> counts;
};

// One occupied cell of the bag tab on show, as the adapter read it.
struct StashMoveCell {
    int             x = -1;
    int             y = -1;
    std::string     key;                   // the item's fingerprint, its map 0 key
    int             itemClass = -1;        // the item's class (itemType)
    bool            stackable = false;
    int64_t         count = 1;             // the stack count; 1 for a single item; below 1 unread
    StashMoveStacks destinationStacks;     // the shown stash tab's stacks of this identity
};

// A node's bbox in GUI units, as the adapter read it (NaN a side that did
// not read), and a node's extents: the distance from its origin (the x, y
// UiCreateNode is given) to each side of its bbox.
struct StashMoveBox {
    double left = std::nan("");
    double top = std::nan("");
    double right = std::nan("");
    double bottom = std::nan("");
};

struct StashMoveExtents {
    double left = 0;
    double up = 0;
    double right = 0;
    double down = 0;
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
// docs/stash-move-research.md § Decision (Live 1e, recorded in round A'8;
// Live 1f and 1g, recorded in round A'9). A route that is false here is not
// called: the Socketable tab is refused while neither socketable path is
// measured, a new identity on a special tab and a merge of more than one unit
// are planned skips. Fixed at build time, never a setting; PlanWith takes
// another value only so the tests can pin what each rule does when it is the
// other way.
struct StashMoveRoutes {
    bool socketNew = false;       // socketRoute new: not-observed (no accepted kind absent from the tab without a person)
    bool socketMerge = true;      // socketMergeRoute: byname (Live 1f and 1g; orb and gem, every identity with a node merges)
    bool newMaterial = true;      // newMaterialRoute: byname
    bool wholeStackMerge = true;  // wholeStackMerge: byname (on the Materials tab)
    // The Socketable tab's merge of more than one unit. Live 1f measured its
    // merge with a count of 1 only (an orb and a gem), so this was off and a
    // socketable stack stayed in the bag. ForgePact #131 turns it on: most bag
    // socketables are stacks, the merge passes the whole count as the Materials
    // tab's measured merge does, and a stack that did not rise by exactly it
    // is still unconfirmed and stops the run. Live procedure 1 of that fix
    // (docs/stash-move-research.md § Live procedure 3, check socket-whole)
    // is its confirmation.
    bool socketWholeStackMerge = true;
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
    // session; turning off is always allowed. Turning on gives a button the
    // adapter could not make one more try.
    bool SetEnabled(bool on) {
        if (on && m_OffThisSession.load()) return false;
        if (on) m_ButtonRefused = false;
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
    // created, and a node left from before is removed. After a refusal
    // (ButtonRefused) it is not made again until the stash window has been
    // closed, or the switch turned on again.
    StashMoveButtonStep ButtonStep(bool stashListed, bool sortListed, bool sortVisible, bool nodeExists) {
        if (!stashListed) m_ButtonRefused = false;
        const bool wanted = IsEnabled() && stashListed && sortListed && sortVisible;
        if (wanted == nodeExists) return StashMoveButtonStep::Keep;
        if (wanted && m_ButtonRefused) return StashMoveButtonStep::Keep;
        return wanted ? StashMoveButtonStep::Create : StashMoveButtonStep::Remove;
    }

    // The node could not be made (the game's node routine refused it, or the
    // Sort row it is placed from did not read). The fail-safe: the line to
    // print, once - empty when this refusal was already reported - and the
    // mod stays on, F4 and the verbs working without the button.
    std::string ButtonRefused(const std::string& reason) {
        if (m_ButtonRefused) return std::string();
        m_ButtonRefused = true;
        return "stashmoveall: button - " + reason + "; F4 still works";
    }

    // buttonRoute: poll (Live 1g). The node is made with no activation, so a
    // click on it runs nothing of the game's; the adapter reads a left press
    // and the mouse's GUI point each frame the node exists, and a press whose
    // point lies inside the node's bbox, read at that frame, is the button
    // press. The sides are inclusive; a side that did not read (NaN) or a box
    // turned inside out is never a press.
    static bool PressInNode(double x, double y, double left, double top, double right, double bottom) {
        return left <= right && top <= bottom && x >= left && x <= right && y >= top && y <= bottom;
    }

    // Whether a poll's mouse point and the node's box all read: every value
    // finite and the box not inside out. False is an unread miss, told apart
    // from a press that fell outside a box that read.
    static bool PressReads(double x, double y, double left, double top, double right, double bottom) {
        return std::isfinite(x) && std::isfinite(y) && std::isfinite(left) && std::isfinite(top)
            && std::isfinite(right) && std::isfinite(bottom) && left <= right && top <= bottom;
    }

    // ---- the button's place (ForgePact #131) --------------------------------
    //
    // UiCreateNode's x, y are the new node's origin. For the mod's node (a
    // UI_Button_Small_obj drawn with Menu_Button_Chat_spr) that origin is its
    // bbox centre, while the Sort node's (the same object, drawn with
    // Inventory_Tab_Button_Solid_spr) is its bbox top-left - so the origin
    // follows the sprite, not the object (measured, Live 1f and 1g: the same
    // numbers both times). The first release placed the node as if its origin
    // were its top-left, so the box sat centred on the point meant for its
    // top-left corner (the owner's report of 2026-09-30). The origin is now
    // worked out from Sort's bbox and the node's own extents, measured on the
    // node itself, so a sprite or GUI-scale change in a game patch still
    // places it right.
    //
    // Where it is checked (the review of #131 round 0): not in the frame the
    // node is made, whose box is not known to be the settled one (Live 1f: the
    // node read visible=0 in the reply and 1 a frame later), but on later
    // ensure steps, once the node reads visible and its box reads the same on
    // two steps in a row (ButtonCheck). Every node made is checked that way,
    // so a later stash open is checked again, and the state line carries what
    // the check read, for comparison with menulayout's rows.

    // Within this many GUI units of the target, the node is on target.
    static constexpr double kButtonTolerance = 1.0;

    // Every side read (finite) and the box not inside out.
    static bool BoxReads(const StashMoveBox& b) {
        return std::isfinite(b.left) && std::isfinite(b.top) && std::isfinite(b.right) && std::isfinite(b.bottom)
            && b.left <= b.right && b.top <= b.bottom;
    }

    // A node's extents from its origin and its bbox, read together; false
    // when any of them did not read.
    static bool ExtentsOf(double x, double y, const StashMoveBox& box, StashMoveExtents& e) {
        if (!std::isfinite(x) || !std::isfinite(y) || !BoxReads(box)) return false;
        e.left = x - box.left;
        e.up = y - box.top;
        e.right = box.right - x;
        e.down = box.bottom - y;
        return true;
    }

    // Before the node has been measured in a session: a box of Sort's own
    // size about its origin, so the first node is already near its place.
    static StashMoveExtents ProvisionalExtents(const StashMoveBox& sort) {
        StashMoveExtents e;
        e.left = e.right = (sort.right - sort.left) / 2;
        e.up = e.down = (sort.bottom - sort.top) / 2;
        return e;
    }

    // ---- the target: the Extra tab's column, Sort's row (the owner, 2026-10-02) ----
    //
    // The bag's page tabs (UI_Button_Inventory_Tab_obj, uiNodeCallstack
    // InventoryTab_1..InventoryTab_5: Main, then four Extra) stand in a row
    // directly above InventorySort's, and Sort sits under the last of them.
    // Move all sits in the slot left of Sort, under InventoryTab_4, and the
    // owner asked for that tab's left and right edges with Sort's top and
    // bottom (docs/stash-move-research.md § Decision buttonTarget). The
    // adapter reads that tab by name at each ensure step (route Tab); when no
    // such tab reads, the target is the same column worked out from Sort's box
    // by the grid's relation (Grid), as fractions of Sort's width and height,
    // which follow a GUI-scale change where GUI units would not. The old rule
    // is the fallback when neither can be had.
    //
    // The grid, two sessions at one GUI scale (2560x1368: the tabs in toolkit
    // #147's stash-bag-layout live 2 with the stash open, Sort in #68's Live
    // 1f and 1g): the five tabs are each Sort's width with no gap between
    // them, their row's bottom is Sort's top, and InventorySort's left and
    // right are InventoryTab_5's - so InventoryTab_4's column is one Sort
    // width left of Sort's left edge, up to it, and the box under it in Sort's
    // row is Sort's height.
    static constexpr double kGridLeftOfSort = -1.0;    // its left edge from Sort's, in Sort widths
    static constexpr double kGridTopOfSort = 0.0;      // its top edge from Sort's, in Sort heights
    static constexpr double kGridWidthOfSort = 1.0;    // its width, in Sort widths
    static constexpr double kGridHeightOfSort = 1.0;   // its height, in Sort heights

    // A box that reads and is not empty: a width and a height above 0.
    static bool BoxSized(const StashMoveBox& b) {
        return BoxReads(b) && b.right > b.left && b.bottom > b.top;
    }

    // The old rule, now the fallback: a box of Sort's size, its right edge
    // `gap` GUI units left of Sort's left edge, level with Sort. Unread when
    // Sort's box or the gap did not read.
    static StashMoveBox SortRuleBox(const StashMoveBox& sort, double gap) {
        StashMoveBox b;
        if (!BoxReads(sort) || !std::isfinite(gap)) return b;
        b.right = sort.left - gap;
        b.left = b.right - (sort.right - sort.left);
        b.top = sort.top;
        b.bottom = sort.bottom;
        return b;
    }

    // The Extra tab's column in Sort's row, from Sort's box by the grid's
    // fractions. Unread when Sort's box did not read or has no width or
    // height to scale by.
    static StashMoveBox GridBox(const StashMoveBox& sort) {
        StashMoveBox b;
        if (!BoxSized(sort)) return b;
        const double w = sort.right - sort.left, h = sort.bottom - sort.top;
        b.left = sort.left + kGridLeftOfSort * w;
        b.top = sort.top + kGridTopOfSort * h;
        b.right = b.left + kGridWidthOfSort * w;
        b.bottom = b.top + kGridHeightOfSort * h;
        return b;
    }

    // The box to make the node to, and which it is. `route` is the one the
    // adapter was built for: Tab takes `tab` (InventoryTab_4's box, read by
    // the adapter) when it is sized - its left and right edges, with Sort's
    // top and bottom - and otherwise, like Grid, the column from Sort's box by
    // the grid's fractions; one that cannot be had falls back to the old rule
    // (Sort), and a Sort box that did not read gives none (None, `target`
    // unread) - the adapter then makes no node. A tab box with no width or
    // height, inside out, or with a side that did not read is never taken.
    static StashMoveButtonRef ButtonTarget(StashMoveButtonRef route, const StashMoveBox& sort, const StashMoveBox& tab,
                                           double gap, StashMoveBox& target) {
        target = StashMoveBox();
        if (!BoxReads(sort)) return StashMoveButtonRef::None;
        if (route == StashMoveButtonRef::Tab && BoxSized(tab)) {
            target.left = tab.left;
            target.right = tab.right;
            target.top = sort.top;
            target.bottom = sort.bottom;
            return StashMoveButtonRef::Tab;
        }
        if (route == StashMoveButtonRef::Tab || route == StashMoveButtonRef::Grid) {
            const StashMoveBox g = GridBox(sort);
            if (BoxReads(g)) {
                target = g;
                return StashMoveButtonRef::Grid;
            }
        }
        target = SortRuleBox(sort, gap);
        return BoxReads(target) ? StashMoveButtonRef::Sort : StashMoveButtonRef::None;
    }

    // The node takes the target's size (the owner, 2026-10-02: the width is
    // the tab's): the scale copied from Sort, times the target's size over
    // Sort's on each axis. False when either box has no size to divide.
    static bool ButtonScale(const StashMoveBox& sort, const StashMoveBox& target, double& sx, double& sy) {
        if (!BoxSized(sort) || !BoxSized(target)) return false;
        sx = (target.right - target.left) / (sort.right - sort.left);
        sy = (target.bottom - target.top) / (sort.bottom - sort.top);
        return true;
    }

    // The target's offset from Sort, for the look members that hold an
    // absolute GUI place (ShiftX/ShiftY): the target box's left and top less
    // Sort's. Sort's own relation between those members and its box is kept
    // by displacing Sort's values, never assumed. False when either box did
    // not read.
    static bool ButtonShift(const StashMoveBox& sort, const StashMoveBox& target, double& dx, double& dy) {
        if (!BoxReads(sort) || !BoxReads(target)) return false;
        dx = target.left - sort.left;
        dy = target.top - sort.top;
        return true;
    }

    // The origin to give UiCreateNode: the node's bbox right edge on the
    // target's, and its vertical centre the target's. False when the target
    // or the extents did not read.
    static bool TargetOrigin(const StashMoveBox& target, const StashMoveExtents& node, double& x, double& y) {
        if (!BoxReads(target) || !std::isfinite(node.left) || !std::isfinite(node.up)
            || !std::isfinite(node.right) || !std::isfinite(node.down))
            return false;
        x = target.right - node.right;
        y = (target.top + target.bottom) / 2 - (node.down - node.up) / 2;
        return true;
    }

    // How far a node's box sits from the target: dx its right edge from the
    // target's, dy its vertical centre from the target's.
    static bool TargetOffset(const StashMoveBox& target, const StashMoveBox& node, double& dx, double& dy) {
        if (!BoxReads(target) || !BoxReads(node)) return false;
        dx = node.right - target.right;
        dy = (node.top + node.bottom) / 2 - (target.top + target.bottom) / 2;
        return true;
    }

    // The node's read box is within kButtonTolerance of the target on both
    // counts. A box that did not read is never on target. The place only:
    // the size is judged apart (TargetSized), so a node of another size is
    // kept, never remade for it.
    static bool OnTarget(const StashMoveBox& target, const StashMoveBox& node) {
        double dx = 0, dy = 0;
        if (!TargetOffset(target, node, dx, dy)) return false;
        return std::fabs(dx) <= kButtonTolerance && std::fabs(dy) <= kButtonTolerance;
    }

    // The node's size is the target's: its width and height each within
    // kButtonTolerance. A box that did not read, the node's or the target's,
    // never is.
    static bool TargetSized(const StashMoveBox& target, const StashMoveBox& node) {
        if (!BoxReads(target) || !BoxReads(node)) return false;
        return std::fabs((node.right - node.left) - (target.right - target.left)) <= kButtonTolerance
            && std::fabs((node.bottom - node.top) - (target.bottom - target.top)) <= kButtonTolerance;
    }

    // The old rule's origin, offset and check, each the target's with the
    // old rule's box (SortRuleBox).
    static bool ButtonOrigin(const StashMoveBox& sort, const StashMoveExtents& node, double gap, double& x, double& y) {
        return TargetOrigin(SortRuleBox(sort, gap), node, x, y);
    }

    static bool ButtonOffset(const StashMoveBox& sort, const StashMoveBox& node, double gap, double& dx, double& dy) {
        return TargetOffset(SortRuleBox(sort, gap), node, dx, dy);
    }

    static bool ButtonOnTarget(const StashMoveBox& sort, const StashMoveBox& node, double gap) {
        return OnTarget(SortRuleBox(sort, gap), node);
    }

    // A node still off target after its second creation is kept (F4 and the
    // press still work) and said once a session - empty when already said;
    // it never turns the mod off.
    std::string ButtonOffTarget(const StashMoveBox& sort, const StashMoveBox& node, double gap) {
        return TargetOffLine(SortRuleBox(sort, gap), node, StashMoveButtonRef::Sort);
    }

    std::string TargetOffLine(const StashMoveBox& target, const StashMoveBox& node, StashMoveButtonRef ref) {
        if (m_ButtonOffSaid) return std::string();
        m_ButtonOffSaid = true;
        double dx = 0, dy = 0;
        if (!TargetOffset(target, node, dx, dy))
            return "stashmoveall: button - its box did not read after it was made, so its place " + PlaceWords(ref)
                   + " is unchecked; F4 still works";
        return "stashmoveall: button - placed " + Tenths(dx) + "," + Tenths(dy) + " off " + OffWords(ref)
            + "; F4 still works";
    }

    // Where the lines say the node sits: beside Sort under the old rule, in
    // the column of the Extra tab above it otherwise.
    static bool InTabColumn(StashMoveButtonRef ref) {
        return ref == StashMoveButtonRef::Tab || ref == StashMoveButtonRef::Grid;
    }

    static std::string PlaceWords(StashMoveButtonRef ref) {
        return InTabColumn(ref) ? "in the column of the Extra tab above it" : "beside Sort";
    }

    static std::string OffWords(StashMoveButtonRef ref) {
        return InTabColumn(ref) ? "the column of the Extra tab above it" : "beside Sort";
    }

    static const char* RefWord(StashMoveButtonRef ref) {
        switch (ref) {
        case StashMoveButtonRef::Tab: return "tab";
        case StashMoveButtonRef::Grid: return "grid";
        case StashMoveButtonRef::Sort: return "sort";
        default: return "none";
        }
    }

    // The target the adapter worked out for the node it is about to make (or
    // check), for the state line's button_ref=, and the tab box it was taken
    // from, for button_tab= (none unless the target is Tab). The grid's
    // column standing in for an unread tab, and the old rule standing in for
    // both, are each said once a session - empty otherwise - and never turn
    // the mod off.
    std::string NoteButtonRef(StashMoveButtonRef ref, const StashMoveBox& tab = StashMoveBox()) {
        {
            std::lock_guard<std::mutex> lock(m_PlaceMutex);
            m_PlaceRef = ref;
            m_PlaceTab = ref == StashMoveButtonRef::Tab ? tab : StashMoveBox();
        }
        if (ref == StashMoveButtonRef::Grid)
            return SayButtonOff(m_ButtonGridSaid, "the Extra tab above it did not read, so its column is worked out "
                                "from the Sort button's box");
        if (ref != StashMoveButtonRef::Sort) return std::string();
        return SayButtonOff(m_ButtonFallbackSaid, "the column of the Extra tab above it could not be worked out, so it "
                            "sits beside Sort by the old rule");
    }

    // A check reads this many ensure steps after a make at most; a node whose
    // box has not settled by then is said unchecked and left as it is.
    static constexpr int kButtonSettleSteps = 6;

    // Two reads of one box are the same box: every side within a twentieth.
    static bool SameBox(const StashMoveBox& a, const StashMoveBox& b) {
        return BoxReads(a) && BoxReads(b) && std::fabs(a.left - b.left) <= 0.05 && std::fabs(a.top - b.top) <= 0.05
            && std::fabs(a.right - b.right) <= 0.05 && std::fabs(a.bottom - b.bottom) <= 0.05;
    }

    // The extents to make a node with: the ones measured on a settled node
    // this session; before that, Sort's own extents about Sort's origin (its
    // x, y and bbox, read by name), scaled per axis to the target's size
    // (ButtonScale), since the node wears Sort's look (owner scope,
    // 2026-09-30) and so lands on target at its first creation; and when
    // Sort's x, y did not read, the provisional box of the target's size
    // about its centre.
    StashMoveExtents ButtonExtentsFor(const StashMoveBox& sort, const StashMoveBox& target, double sortX = std::nan(""),
                                      double sortY = std::nan("")) const {
        if (m_ButtonExtentsRead) return m_ButtonExtents;
        double sx = 1, sy = 1;
        if (!ButtonScale(sort, target, sx, sy)) sx = sy = 1;
        StashMoveExtents own;
        if (ExtentsOf(sortX, sortY, sort, own)) {
            own.left *= sx; own.right *= sx;
            own.up *= sy; own.down *= sy;
            return own;
        }
        return ProvisionalExtents(BoxReads(target) ? target : sort);
    }

    // The same with Sort's own box as the target (the old rule's size).
    StashMoveExtents ButtonExtents(const StashMoveBox& sort, double sortX = std::nan(""),
                                   double sortY = std::nan("")) const {
        return ButtonExtentsFor(sort, sort, sortX, sortY);
    }

    // The button's size under the old rule (owner scope, 2026-09-30):
    // Sort-sized when it is the size of Sort's box (TargetSized).
    static bool ButtonSortSized(const StashMoveBox& sort, const StashMoveBox& node) {
        return TargetSized(sort, node);
    }

    // The adapter made a node: the first of a Create step (remake false) or
    // the one ButtonCheck asked for (remake true). The check starts over on
    // the new node; at most one remake is asked per Create step.
    void NoteButtonMade(bool remake) {
        m_ButtonMakes = remake ? m_ButtonMakes + 1 : 1;
        if (!remake) m_ButtonRemakeAsked = false;
        m_ButtonChecked = false;
        m_ButtonSteps = 0;
        m_ButtonHaveLast = false;
        m_ButtonLook = StashMoveButtonLook::None;
        m_ButtonLookRead = StashMoveLookTally();
        std::lock_guard<std::mutex> lock(m_PlaceMutex);
        m_PlaceWord = "pending";
        m_PlaceBox = StashMoveBox();
        m_PlaceExtents = StashMoveExtents();
        m_PlaceExtentsRead = false;   // never the last node's extents shown as this one's
        m_PlaceMakes = m_ButtonMakes;
        m_PlaceStep = 0;
        m_PlaceLook = StashMoveButtonLook::None;
        m_PlaceLookEqual = m_PlaceLookListed = 0;
        m_PlaceSize = StashMoveBox();   // nor its size
    }

    // Whether the node just made is still being checked: the adapter reads
    // its look again each ensure step until then, and not after.
    bool ButtonLookWanted() const { return m_ButtonMakes > 0 && !m_ButtonChecked; }

    // What the adapter read of the node's look: after it gave the node
    // Sort's, and again each ensure step while the node is checked, so the
    // look judged is the one on the settled read, not only the frame it was
    // written in (whether the UI layer puts its own back is not read). The
    // tally carries the members that read the same and the first that did
    // not, for the state line's button_look_same= and the look lines.
    void NoteButtonLook(const StashMoveLookTally& read) {
        if (m_ButtonMakes == 0 || m_ButtonChecked) return;
        m_ButtonLook = read.Verdict();
        m_ButtonLookRead = read;
        std::lock_guard<std::mutex> lock(m_PlaceMutex);
        m_PlaceLook = m_ButtonLook;
        m_PlaceLookEqual = read.equal;
        m_PlaceLookListed = read.listed;
    }

    // A verdict with no members behind it (no count to print).
    void NoteButtonLook(StashMoveButtonLook look) {
        StashMoveLookTally t;
        t.look = look;
        NoteButtonLook(t);
    }

    // The kinds the look copy writes and compares: those the research
    // probe's lookcopy writes (a number, bool, string or asset). Undefined,
    // a reference, struct, array or method is never written, nor unread.
    static bool LookWritable(StashMoveLookKind k) {
        return k == StashMoveLookKind::Number || k == StashMoveLookKind::Bool || k == StashMoveLookKind::String
            || k == StashMoveLookKind::Asset;
    }

    // The look step for one member read off Sort (fix2's round 2: a
    // member's kind never decides whether the copy runs): one of the
    // writable kinds is written as read, whatever that kind; a scaled
    // member is written scaled by `sx` or `sy`, and a displaced one moved by
    // `dx` or `dy` (ButtonShift), only when it read as a number, and is
    // otherwise not written, so it compares unread - as it does when the
    // scale or offset did not read (NaN), so a place is never left on
    // Sort's for want of one. What the node should read back as is the
    // value written.
    static StashMoveLookStep LookStep(const StashMoveLookValue& read, StashMoveLookWrite how, double sx, double sy,
                                      double dx, double dy) {
        StashMoveLookStep step;
        if (!LookWritable(read.kind)) return step;
        if (how == StashMoveLookWrite::AsRead) {
            step.put = StashMoveLookPut::AsRead;
            step.want = read;
            return step;
        }
        if (read.kind != StashMoveLookKind::Number) return step;
        double v = read.number;
        switch (how) {
        case StashMoveLookWrite::ScaleX: v *= sx; break;
        case StashMoveLookWrite::ScaleY: v *= sy; break;
        case StashMoveLookWrite::ShiftX: v += dx; break;
        case StashMoveLookWrite::ShiftY: v += dy; break;
        default: return step;
        }
        if (!std::isfinite(v)) return step;
        step.put = StashMoveLookPut::Number;
        step.want.kind = StashMoveLookKind::Number;
        step.want.number = v;
        return step;
    }

    // A member read back off the node against what it should read, by kind:
    // strings by their text; numbers, bools and asset references by their
    // value (an asset by its index), so a bool or asset that reads back as a
    // number still compares; a string against any other kind differs. A
    // value not read, or of a kind never written, is unread.
    static StashMoveLookSame LookCompare(const StashMoveLookValue& want, const StashMoveLookValue& got) {
        if (!LookWritable(want.kind) || !LookWritable(got.kind)) return StashMoveLookSame::Unread;
        const bool wantText = want.kind == StashMoveLookKind::String, gotText = got.kind == StashMoveLookKind::String;
        if (wantText || gotText)
            return wantText && gotText && want.text == got.text ? StashMoveLookSame::Same : StashMoveLookSame::Differs;
        if (!std::isfinite(want.number) || !std::isfinite(got.number)) return StashMoveLookSame::Unread;
        return std::fabs(got.number - want.number) <= kLookTolerance ? StashMoveLookSame::Same : StashMoveLookSame::Differs;
    }

    static constexpr double kLookTolerance = 1e-6;

    // The state line's button_look_same=: the members that read the same
    // over those listed, none before a look with members was read.
    static std::string LookSameText(int equal, int listed) {
        if (listed <= 0) return "none";
        return std::to_string(equal) + "/" + std::to_string(listed);
    }

    // The check below under the old rule alone: the target is
    // SortRuleBox(sort, gap).
    StashMoveButtonCheck ButtonCheck(bool visible, const StashMoveBox& sort, double nodeX, double nodeY,
                                     const StashMoveBox& box, double gap, double& x, double& y, std::string& line) {
        return ButtonCheck(visible, sort, SortRuleBox(sort, gap), StashMoveButtonRef::Sort, nodeX, nodeY, box, x, y, line);
    }

    // Each ensure step while the adapter holds a node, with what it read now:
    // whether the node is visible, Sort's box, the target worked out from it
    // this step and which it is (ButtonTarget), the node's x, y and box.
    // Until the node reads visible and its box (and Sort's) reads the same as
    // on the step before, nothing is decided. Then its extents are measured
    // and kept for the session, and the box is judged against the target: on
    // target is said once a session, positively; off target asks for one
    // remake at the origin those extents give (Remake, x and y set), and a
    // node still off after it - or one whose remake was asked and not carried
    // out - is kept and said once. A box that has not settled after
    // kButtonSettleSteps is said unchecked. A node kept (on target, or still
    // off) is judged on that same settled read for its size against the
    // target's and for the look the adapter last read: not the target's
    // size, or a look not taken, is kept - never remade for it - and said
    // once a session each, on a line of its own. `line` is the lines to
    // print, one per cause, empty when none. Never turns the mod off.
    StashMoveButtonCheck ButtonCheck(bool visible, const StashMoveBox& sort, const StashMoveBox& target,
                                     StashMoveButtonRef ref, double nodeX, double nodeY, const StashMoveBox& box,
                                     double& x, double& y, std::string& line) {
        line.clear();
        if (m_ButtonMakes == 0 || m_ButtonChecked) return StashMoveButtonCheck::Keep;
        ++m_ButtonSteps;
        {
            std::lock_guard<std::mutex> lock(m_PlaceMutex);
            m_PlaceRef = ref;
            if (ref != StashMoveButtonRef::Tab) m_PlaceTab = StashMoveBox();   // no tab box behind this target
        }
        const bool reads = visible && BoxReads(box) && BoxReads(sort) && BoxReads(target);
        const bool settled = reads && m_ButtonHaveLast && SameBox(box, m_ButtonLast) && SameBox(sort, m_ButtonLastSort);
        m_ButtonHaveLast = reads;
        m_ButtonLast = box;
        m_ButtonLastSort = sort;
        if (!settled) {
            NotePlace(m_ButtonSteps < kButtonSettleSteps ? "pending" : "unsettled", box, nullptr);
            if (m_ButtonSteps < kButtonSettleSteps) return StashMoveButtonCheck::Keep;
            m_ButtonChecked = true;
            line = SayButtonOff(m_ButtonUnsettledSaid, "its box had not settled " + std::to_string(m_ButtonSteps)
                                + " ensure steps after it was made, so its place " + PlaceWords(ref) + " is unchecked");
            return StashMoveButtonCheck::Keep;
        }
        StashMoveExtents e;
        if (!ExtentsOf(nodeX, nodeY, box, e)) {
            NotePlace("unread", box, nullptr);
            m_ButtonChecked = true;
            line = SayButtonOff(m_ButtonUnreadSaid, "its x, y did not read, so its place " + PlaceWords(ref)
                                + " is unchecked");
            return StashMoveButtonCheck::Keep;
        }
        m_ButtonExtents = e;
        m_ButtonExtentsRead = true;
        if (OnTarget(target, box)) {
            NotePlace("on", box, &e);
            m_ButtonChecked = true;
            if (!m_ButtonPlacedSaid) {
                m_ButtonPlacedSaid = true;
                line = "stashmoveall: button - placed " + PlaceWords(ref) + ", box " + BoxText(box);
            }
            JudgeLook(target, ref, box, line);
            return StashMoveButtonCheck::Keep;
        }
        if (!m_ButtonRemakeAsked && TargetOrigin(target, e, x, y)) {
            m_ButtonRemakeAsked = true;
            NotePlace("remake", box, &e);
            return StashMoveButtonCheck::Remake;
        }
        NotePlace("off", box, &e);
        m_ButtonChecked = true;
        line = TargetOffLine(target, box, ref);
        JudgeLook(target, ref, box, line);
        return StashMoveButtonCheck::Keep;
    }

    // A box's size as <w>x<h> to a tenth, or none when it did not read.
    static std::string SizeText(const StashMoveBox& b) {
        if (!BoxReads(b)) return "none";
        return Tenths(b.right - b.left) + "x" + Tenths(b.bottom - b.top);
    }

    static const char* LookWord(StashMoveButtonLook look) {
        switch (look) {
        case StashMoveButtonLook::Sort: return "sort";
        case StashMoveButtonLook::Differs: return "differs";
        case StashMoveButtonLook::Unread: return "unread";
        default: return "none";
        }
    }

    // A box as l,t,r,b to a tenth, or none when it did not read.
    static std::string BoxText(const StashMoveBox& b) {
        if (!BoxReads(b)) return "none";
        return Tenths(b.left) + "," + Tenths(b.top) + "," + Tenths(b.right) + "," + Tenths(b.bottom);
    }

    // A number to a tenth, for the lines.
    static std::string Tenths(double v) {
        const long long t = std::llround(std::fabs(v) * 10);
        return std::string(v < 0 && t != 0 ? "-" : "") + std::to_string(t / 10) + "." + std::to_string(t % 10);
    }

    // Where each press went, for the state line (the review of Phase C: the
    // player build has no probe, so a click that moved nothing must still say
    // why). The adapter says whether it holds a node; each left press it read
    // while holding one is counted once as inside the node (NoteButtonPress),
    // or as a miss (outside, or unread when PressReads was false), and a poll
    // that threw is counted apart. A session's counts; never reset.
    void NoteButtonHeld(bool held) { m_ButtonHeld.store(held); }

    void NoteButtonMiss(bool pointAndBoxRead) {
        ++m_Presses;
        if (pointAndBoxRead) ++m_PressesOutside;
        else ++m_PressesUnread;
    }

    void NoteButtonPollError() { ++m_PollErrors; }

    // The adapter saw the button pressed (the frame poll). Nothing moves
    // here: the press waits for the frame tick's guard. Off, it is not kept.
    void NoteButtonPress() {
        ++m_Presses;
        ++m_PressesInNode;
        if (IsEnabled()) m_ButtonPressed.store(true);
    }

    // The frame tick takes a recorded press once, under the key's own guard:
    // the game in front, the stash listed, no modifier held. Presses recorded
    // before one tick are one run; a press the guard refuses is dropped, and
    // counted with its reason (off, fg, stash, modifier) for the state line.
    bool TakeButtonPress(bool foreground, bool stashListed, bool modifier) {
        const bool pressed = m_ButtonPressed.exchange(false);
        if (!pressed) return false;
        const char* drop = !IsEnabled() ? "off" : !foreground ? "fg" : !stashListed ? "stash" : modifier ? "modifier" : nullptr;
        if (drop) {
            ++m_PressesDropped;
            m_LastDrop.store(drop);
            return false;
        }
        ++m_PressesTaken;
        return true;
    }

    // The button's fields of the state line: whether the adapter holds a
    // node, the presses it read while holding one, where they went, and the
    // runs they started or why they did not. A click that moved nothing then
    // reads as poll-blind (presses=0 with button=held), a bbox miss (outside
    // or unread above 0, in_node not risen), a poll that threw (errors) or a
    // guard drop (dropped, with last_drop). Then the place check of the last
    // node made (ButtonCheck): its verdict (none before any node, pending,
    // remake, on, off, unsettled, unread), the node's last box and extents it
    // read (none until read), how many nodes that Create step made, and the
    // ensure step after the make that read them - so a bug report, or a live
    // check comparing menulayout's rows, can tell what the mod itself read.
    // Then the node's look as last read (none, sort, differs, unread) and
    // its size on the settled read (none until then), judged against the
    // target; which target that is (RefWord: none, tab, grid, sort) and the
    // tab box it was taken from (none unless tab); last, how many of the
    // look's members read the same over those listed, on that same read
    // (none before one with members).
    std::string ButtonFields() const {
        std::string place;
        {
            std::lock_guard<std::mutex> lock(m_PlaceMutex);
            place = std::string(" button_place=") + m_PlaceWord + " button_box=" + BoxText(m_PlaceBox)
                + " button_extents=" + (m_PlaceExtentsRead ? Tenths(m_PlaceExtents.left) + "," + Tenths(m_PlaceExtents.up) + ","
                                                               + Tenths(m_PlaceExtents.right) + "," + Tenths(m_PlaceExtents.down)
                                                           : std::string("none"))
                + " button_makes=" + std::to_string(m_PlaceMakes) + " button_step=" + std::to_string(m_PlaceStep)
                + " button_look=" + LookWord(m_PlaceLook) + " button_size=" + SizeText(m_PlaceSize)
                + " button_ref=" + RefWord(m_PlaceRef) + " button_tab=" + BoxText(m_PlaceTab)
                + " button_look_same=" + LookSameText(m_PlaceLookEqual, m_PlaceLookListed);
        }
        return std::string(" button=") + (m_ButtonHeld.load() ? "held" : "none")
            + " presses=" + std::to_string(m_Presses.load()) + " in_node=" + std::to_string(m_PressesInNode.load())
            + " outside=" + std::to_string(m_PressesOutside.load()) + " unread=" + std::to_string(m_PressesUnread.load())
            + " errors=" + std::to_string(m_PollErrors.load()) + " taken=" + std::to_string(m_PressesTaken.load())
            + " dropped=" + std::to_string(m_PressesDropped.load()) + " last_drop=" + m_LastDrop.load() + place;
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
        // The source: a bag page for any destination but the Socketable tab; a
        // sub-tab only for the special tab its moves were measured into, and
        // the Socket view is the Socketable tab's only source (every
        // socketable merge ran from it, Live 1f and 1g).
        const bool page = view.bagTab >= 0 && view.bagTab < kBagPageTabs;
        const bool subTab = view.bagTab == kBagMaterialsView || (view.bagTab == kBagSocketView && socketMeasured);
        if (!page && !subTab) return refuse("unsupported bag tab " + std::to_string(view.bagTab));
        const bool fits = page ? tab != StashMoveTab::Socketable
                               : (view.bagTab == kBagMaterialsView && tab == StashMoveTab::Materials)
                                     || (view.bagTab == kBagSocketView && tab == StashMoveTab::Socketable);
        if (!fits) return refuse("unsupported bag tab " + std::to_string(view.bagTab) + " for stash tab "
                                 + std::to_string(view.stashTab));

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
            RouteFor(tab, c, c.destinationStacks, routes, item);
            plan.items.push_back(item);
        }
        if (plan.items.empty()) return refuse("nothing to move");
        return plan;
    }

    // ---- the game's merge, as a model (ForgePact #131) -----------------------
    //
    // Static reading (R) of StashAddToStack, 2026-09-30, in our words: it sets
    // a cap from its sixth argument - 999 without flag 8, 999999 with it - then
    // walks the array it is handed, and for each item of the moved item's
    // identity it merges the moved count only when that stack's count plus it
    // stays at or below the cap, answering true; a stack that would pass the
    // cap is passed over for the next, and it answers false after the last.
    // So the first stack that fits the whole count takes it, and nothing is
    // ever split. The measured merges pass 0 on the pages and the Materials
    // tab and 8 on the Socketable tab (Live 1c to 1g). The order of the walk
    // beyond "array order" is not read, so nothing here depends on which
    // stack takes it, only on whether one does; the adapter confirms a merge
    // by the identity's sum. Live procedure 3's material-overflow and
    // material-partial measure the cap.

    static constexpr int64_t kStackCap = 999;
    static constexpr int64_t kStackCapFlag8 = 999999;
    static constexpr int     kStackFlag8 = 8;
    // StashAddToStack's sixth argument as the game's own moves passed it.
    static constexpr int kPageStackFlags = 0;     // the pages and the Materials tab
    static constexpr int kSocketStackFlags = 8;   // the Socketable tab
    // StackThatFits's answers other than an index.
    static constexpr int kNoStackFits = -1;
    static constexpr int kStacksUnknown = -2;

    static int64_t StackCap(int sixthArgument) {
        return (sixthArgument & kStackFlag8) ? kStackCapFlag8 : kStackCap;
    }

    static int StackFlagsFor(StashMoveTab tab) {
        return tab == StashMoveTab::Socketable ? kSocketStackFlags : kPageStackFlags;
    }

    // The cap a merge into this stash tab meets, from the sixth argument the
    // route passes there.
    static int64_t CapFor(int stashTab) { return StackCap(StackFlagsFor(TabOf(stashTab))); }

    // The stack the game's merge takes the whole count into: its index in
    // the list, kNoStackFits when none has room for it (or there is none),
    // kStacksUnknown when the list, a stack in it, or the count did not read
    // - never "fits" and never "full".
    static int StackThatFits(const StashMoveStacks& stacks, int64_t count, int64_t cap) {
        if (!stacks.read || count < 1) return kStacksUnknown;
        for (int64_t n : stacks.counts)
            if (n < 0) return kStacksUnknown;
        for (size_t i = 0; i < stacks.counts.size(); ++i)
            if (stacks.counts[i] + count <= cap) return (int)i;
        return kNoStackFits;
    }

    // The stack route's room, for MayCall: 1 a stack of the identity has room
    // for the whole count, 0 none has, -1 unknown. A stack that exists is not
    // room: a full one answers false and moves nothing.
    static int StackRoom(const StashMoveStacks& stacks, int64_t count, int64_t cap) {
        const int fit = StackThatFits(stacks, count, cap);
        return fit >= 0 ? 1 : (fit == kNoStackFits ? 0 : -1);
    }

    // The identity's sum on the shown tab, the value a merge is confirmed by;
    // -1 when the list or a stack in it did not read.
    static int64_t StackSum(const StashMoveStacks& stacks) {
        if (!stacks.read) return -1;
        int64_t sum = 0;
        for (int64_t n : stacks.counts) {
            if (n < 0) return -1;
            sum += n;
        }
        return sum;
    }

    // The route of an item the shown tab takes, from its identity's stacks
    // there (ForgePact #131, per stack, not per sum):
    // - a stackable whose stacks or count are unknown is a skip, since the
    //   game's stack routine would merge into one the read missed;
    // - a stack with room for the whole count (StackThatFits): the merge,
    //   where the tab's own measurement allows its count;
    // - none with room, on a page or the Materials tab: a new stack in a free
    //   cell (the tab placement; on Materials, newMaterialRoute's);
    // - none with room on the Socketable tab, which holds one stack per kind:
    //   a full stack is a skip, never a second one; no stack a new kind.
    // A non-stackable on a page is always the placement.
    static void RouteFor(StashMoveTab tab, const StashMoveCell& c, const StashMoveStacks& stacks,
                         const StashMoveRoutes& routes, StashMoveItem& item) {
        item.route = StashMoveRoute::None;
        item.refusal.clear();
        if (tab == StashMoveTab::Grid && !c.stackable) { item.route = StashMoveRoute::Cell; return; }
        const bool socket = tab == StashMoveTab::Socketable;
        const bool many = c.count > 1;
        const int fit = StackThatFits(stacks, c.count, StackCap(StackFlagsFor(tab)));
        if (fit == kStacksUnknown) { item.refusal = "its stack on the shown tab could not be read"; return; }
        if (fit >= 0) {
            // The one-unit merge is measured on the Materials tab
            // (stackMoveRoute); the Socketable tab's by socketMergeRoute, on
            // the item's own node, with no non-stackable case (the gem Live
            // 1e read as one merged too, Live 1f). More than one unit follows
            // each tab's own flag: wholeStackMerge on the pages and the
            // Materials tab, socketWholeStackMerge on the Socketable tab.
            if (socket && !routes.socketMerge) item.refusal = "a socketable merge is not measured";
            else if (many && !socket && !routes.wholeStackMerge) item.refusal = "whole-stack merge not measured";
            else if (many && socket && !routes.socketWholeStackMerge)
                item.refusal = "a socketable merge of more than one unit is not measured";
            else item.route = StashMoveRoute::Stack;
            return;
        }
        if (socket) {
            if (!stacks.counts.empty()) item.refusal = "its stack on the shown tab is full";
            else if (routes.socketNew) item.route = StashMoveRoute::Cell;
            else item.refusal = "a new kind stays in the bag";
            return;
        }
        // A new stack on the Materials tab is the new-identity placement
        // (newMaterialRoute), whether its kind is absent or every stack of it
        // is full; on a page, the tab placement.
        if (tab == StashMoveTab::Materials && !routes.newMaterial) item.refusal = "a new kind stays in the bag";
        else item.route = StashMoveRoute::Cell;
    }

    // The route decided again at the point of use (round-2 review). The plan
    // read the shown tab's stacks once, before the run; an earlier item of the
    // same run may since have made or filled the stack a later item of its
    // identity meets. So just before a stackable's call the adapter re-reads
    // that identity's stacks on the shown tab and this decides with them,
    // whatever the plan said (RouteFor's rule). `planned.cell.count` is the
    // count re-read at the same moment; one that did not read (below 1) is
    // never moved, since both routes pass it. A planned skip stays one with
    // its own reason, and a non-stackable keeps its placement: it needs no
    // stacks.
    static StashMoveItem RouteAtUse(const StashMoveItem& planned, int stashTab, const StashMoveStacks& stacksNow,
                                    const StashMoveRoutes& routes = kMeasuredRoutes) {
        if (planned.route == StashMoveRoute::None || !planned.cell.stackable) return planned;
        StashMoveItem item = planned;
        RouteFor(TabOf(stashTab), planned.cell, stacksNow, routes, item);
        return item;
    }

    // The cell route's StashAddToStack answered true: the game merged where
    // the model read no stack with room for the whole count. The item is
    // decided as a merge - moved only when its identity's sum rose by exactly
    // its count - never as a placement, so it cannot read as a loss by
    // construction, nor as a move when one unit was taken from a larger stack.
    static StashMoveItem AsMerge(const StashMoveItem& item) {
        StashMoveItem merged = item;
        merged.route = StashMoveRoute::Stack;
        return merged;
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
    // the panel reads from the last of these lines (the state word first; the
    // button's fields follow the key). After a loss the button's fields stay,
    // so a click that ended in one reads from this line too, and the reason,
    // free text, goes last.
    std::string StateLine() const {
        if (OffThisSession()) return "stashmoveall: state=off-for-this-session" + ButtonFields() + " reason=" + m_OffReason;
        return std::string("stashmoveall: state=") + (IsEnabled() ? "on" : "off") + " key=F4" + ButtonFields();
    }

    // `stashmoveall 1` after a loss: still off, and why.
    std::string OffForSessionLine() const { return LossLine("stashmoveall", m_OffReason); }

private:
    // The place check's verdict and what it read, for the state line.
    void NotePlace(const char* word, const StashMoveBox& box, const StashMoveExtents* e) {
        std::lock_guard<std::mutex> lock(m_PlaceMutex);
        m_PlaceWord = word;
        m_PlaceBox = box;
        if (e) { m_PlaceExtents = *e; m_PlaceExtentsRead = true; }
        m_PlaceMakes = m_ButtonMakes;
        m_PlaceStep = m_ButtonSteps;
    }

    // A place that could not be checked, said once a session per cause, each
    // with its own flag, so an early one never hides a later off-target line
    // (the review of #131 round 1); F4 and the press still work.
    static std::string SayButtonOff(bool& said, const std::string& why) {
        if (said) return std::string();
        said = true;
        return "stashmoveall: button - " + why + "; F4 still works";
    }

    // The look and size of a kept node, on the settled read the place was
    // judged on (owner scope, 2026-09-30): its size against the target's
    // (the Sort button's under the old rule, the Extra tab column's
    // otherwise) and the look the adapter last read. Either one off is kept
    // as it is - a remake is for the place only - and said once a session on
    // a line of its own, added to `line`.
    void JudgeLook(const StashMoveBox& target, StashMoveButtonRef ref, const StashMoveBox& box, std::string& line) {
        {
            std::lock_guard<std::mutex> lock(m_PlaceMutex);
            m_PlaceSize = box;
            m_PlaceLook = m_ButtonLook;
            m_PlaceLookEqual = m_ButtonLookRead.equal;
            m_PlaceLookListed = m_ButtonLookRead.listed;
        }
        auto add = [&line](const std::string& said) {
            if (said.empty()) return;
            line += (line.empty() ? "" : "\n") + said;
        };
        const char* whose = InTabColumn(ref) ? "the Extra tab column's " : "the Sort button's ";
        if (!TargetSized(target, box))
            add(SayButtonOff(m_ButtonSizeSaid, "its size " + SizeText(box) + " is not " + whose
                             + SizeText(target) + ", so it is kept as it is"));
        // The member that did not read the same, first in the list, and how
        // many did (fix2's round 2: a look off names its cause).
        const StashMoveLookTally& r = m_ButtonLookRead;
        const std::string which = r.first.empty() ? std::string()
            : " (" + r.first + (r.firstWas == StashMoveLookSame::Differs ? " differs" : " did not read") + "; "
                + LookSameText(r.equal, r.listed) + " members the same)";
        if (m_ButtonLook == StashMoveButtonLook::Differs)
            add(SayButtonOff(m_ButtonLookSaid, "it did not take the Sort button's look" + which
                             + ", so it is kept with its own"));
        else if (m_ButtonLook == StashMoveButtonLook::Unread)
            add(SayButtonOff(m_ButtonLookUnreadSaid, "its look beside Sort could not be read" + which
                             + ", so it is unchecked"));
    }

    std::atomic<bool> m_Enabled{false};
    std::atomic<bool> m_OffThisSession{false};
    std::atomic<bool> m_ButtonPressed{false};
    std::atomic<bool> m_ButtonHeld{false};
    std::atomic<int>  m_Presses{0};
    std::atomic<int>  m_PressesInNode{0};
    std::atomic<int>  m_PressesOutside{0};
    std::atomic<int>  m_PressesUnread{0};
    std::atomic<int>  m_PollErrors{0};
    std::atomic<int>  m_PressesTaken{0};
    std::atomic<int>  m_PressesDropped{0};
    std::atomic<const char*> m_LastDrop{"none"};
    bool              m_KeyWasDown = false;
    bool              m_ButtonRefused = false;   // a refusal already reported, while the stash stays open
    bool              m_ButtonOffSaid = false;   // the off-target line already said this session
    bool              m_ButtonUnsettledSaid = false; // the not-settled line already said this session
    bool              m_ButtonUnreadSaid = false; // the x, y-unread line already said this session
    bool              m_ButtonPlacedSaid = false; // the placed line already said this session
    bool              m_ButtonSizeSaid = false;  // the not-Sort-sized line already said this session
    bool              m_ButtonLookSaid = false;  // the look-not-taken line already said this session
    bool              m_ButtonLookUnreadSaid = false; // the look-unread line already said this session
    bool              m_ButtonFallbackSaid = false; // the old-rule fallback line already said this session
    bool              m_ButtonGridSaid = false;  // the grid-column fallback line already said this session
    StashMoveButtonLook m_ButtonLook = StashMoveButtonLook::None; // the node's look as last read
    StashMoveLookTally m_ButtonLookRead;          // the members behind it
    // The place check (ButtonCheck), on the frame tick's thread.
    int               m_ButtonMakes = 0;         // nodes made by the current Create step, 0 before any
    int               m_ButtonSteps = 0;         // ensure steps checked since the last make
    bool              m_ButtonRemakeAsked = false;
    bool              m_ButtonChecked = false;   // this node's place decided
    bool              m_ButtonHaveLast = false;  // the step before read a visible node and both boxes
    StashMoveBox      m_ButtonLast;
    StashMoveBox      m_ButtonLastSort;
    StashMoveExtents  m_ButtonExtents;           // measured on a settled node, kept for the session
    bool              m_ButtonExtentsRead = false;
    // What the state line prints of it, read on whichever thread prints it.
    mutable std::mutex m_PlaceMutex;
    const char*       m_PlaceWord = "none";
    StashMoveBox      m_PlaceBox;
    StashMoveExtents  m_PlaceExtents;
    bool              m_PlaceExtentsRead = false;
    int               m_PlaceMakes = 0;
    int               m_PlaceStep = 0;
    StashMoveButtonLook m_PlaceLook = StashMoveButtonLook::None;
    int               m_PlaceLookEqual = 0;      // its members that read the same
    int               m_PlaceLookListed = 0;     // of those listed, 0 before a read with members
    StashMoveBox      m_PlaceSize;              // the settled box its size is read from
    StashMoveButtonRef m_PlaceRef = StashMoveButtonRef::None; // the target the last node was made or checked to
    StashMoveBox      m_PlaceTab;               // the tab box that target was taken from, unread unless Tab
    std::string       m_OffReason;
};

} // namespace ForgePact
