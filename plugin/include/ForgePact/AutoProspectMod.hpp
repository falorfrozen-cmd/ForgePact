#pragma once

#include "Common.hpp"

namespace ForgePact {

// Auto-prospect on insert (ForgePact issue #9, Stage B). The human chose it on
// 2026-09-18 over a bigger prospect grid: while the mod is on, every item
// moved into the Prospect Cube's grid is prospected at once by the game's own
// Prospect operation, so the 9x6 grid stops being the limit on a batch
// (docs/prospect-window-research.md, § Decision gate and § Stage B).
//
// This header is only the DECISION: when an insert counts, when to invoke,
// when to refuse, and what to say. It is game-independent on purpose - it
// names no runtime interface, builtin, log call or runtime value type
// (test_auto_prospect_contract.py checks the spellings) - so
// tests/auto_prospect_harness.cpp compiles it whole with no runtime stub. The
// adapter in ModuleMain.cpp stays the only code that touches the game:
//   - the m_MoveItemToGrid hook calls OnInsert during the step, and never
//     invokes anything itself. It installs through the both-route
//     HookOneScript (table swap plus inline detour, the trampoline forwarded
//     to the hook body), never HookOneScriptTable: compiled GML calls this
//     closure directly, so a table-only install would ship armed and inert;
//   - FrameCallback re-finds the window, the ProspectGrid node and the Prospect
//     button, each by what it is, reads the node's cells into an
//     AutoProspectView, and calls Decide - so the permission is checked at the
//     point of use, on the objects being acted on, never on state a hook
//     cached earlier;
//   - on Invoke it runs the handler once, re-reads the grid straight after the
//     call, and reports both with OnInvoked.
//
// Phase 1 (research doc, § Stage B results) proved the invoke live on
// 2026-09-18 with one shape, the only one the adapter uses: the handler by its
// own asset index through script_execute, self = the Prospect button found by
// its handler variable, other = the window, and the button's own argument
// array. Its names are below, so the harness pins them too.
//
// Stage C (research doc, § Stage C ship design): with the `bag` sub-option on
// (the default), the frame a landed insert is about to be prospected first
// sends the previous prospect's batch to the player's materials tab, by the
// route Stage C's M7 recorded. The core asks for that pass (MoveMaterials),
// turns each cell's report into an outcome, and then decides the invoke as
// before in the same frame. The adapter decides what a material is, through
// the SDK; nothing here knows an item-type value.
//
// Stage D (research doc, § Stage D results and ship design): a material whose
// type has no stack in the tab yet is refused by the has-a-stack check, and
// the game's own click-move then places it through the grid the game prefers
// for it - measured landing in the main bag grid, not the materials tab. The
// adapter takes that route after a "no", and the core classifies it
// (ClassifyMove) and counts it (`moved-new`).
//
// The batch is identified by what it is, not by being a material (round 1):
// live on e63eed5 an inserted ore - itself a material - was moved back to the
// tab in the pass before its own prospect and never prospected. The core
// cannot tell which cell an insert filled (an insert is known only by a
// count), but it can tell what its own invoke produced: OnInvoked's `after`
// read shares the invoke's frame, so the fingerprints in `after` that were
// not in the invoking view are the batch, and nothing inserted can be among
// them. A cell is named only when its fingerprint is in that batch AND the
// adapter flagged it as a material. Anything the core cannot account for -
// first sight of a node (a new node, the window or grid gone, the parent
// toggled), a removal, a `not-landed` expiry - forgets the batch, and a
// forgotten batch moves nothing: the materials stay in the grid. One pass
// consumes the batch, whatever became of each cell.

// The recorded shape's names. The Prospect button is the UI_Button_Small_obj
// whose kAutoProspectHandlerVar is a method value of UiAProspectButton (three
// small buttons link to the window live, and only this one carries the
// handler); its kAutoProspectArgsVar array is what a press is handed. The grid
// is the UI_Inventory_Grid_obj whose uiNodeCallstack names
// kAutoProspectGridName.
static constexpr const char* kAutoProspectHandlerVar = "activationFunc";
static constexpr const char* kAutoProspectArgsVar = "activationArgs";
static constexpr const char* kAutoProspectGridName = "ProspectGrid";

// Free cells an invoke needs. Prospected materials stay in the grid, one
// single-cell stack per material type, not observed to merge, filling column
// 0 and then column 1 (Phase 1 P-free-cells), so a grid that is nearly full
// refuses rather than invoke into it. 9 free cells is the fewest measured to
// still take an insert and prospect; 6-8 are not measured, so this stays one
// full column.
static constexpr int kAutoProspectMinFreeCells = 6;
// Frames a pending insert may wait for the filled count to rise above the
// settled count before it expires as `not-landed`. A click-in's cell is
// already filled when the insert closure runs (Phase 1: `contents=6->6`
// across the hook), so a real insert has usually landed by the first frame;
// a rearrangement inside the grid never raises the count at all.
static constexpr int kAutoProspectLandFrames = 30;

// MoveMaterials (Stage C): before the invoke, send the named cells to the
// materials tab, report each cell with OnMoveReport, re-read, and call Decide
// again in the same frame.
enum class AutoProspectAction { None, Invoke, Refuse, MoveMaterials };

// One cell of the ProspectGrid as the adapter read it. `material` is decided
// by the adapter, by what the item is (its item type, through the SDK); the
// core has no item-type value of its own and never looks at the fingerprint's
// text. The adapter fills `cells` only on frames NeedsMaterials() asks for.
struct AutoProspectCell {
    int         row = 0;
    int         col = 0;
    std::string fingerprint;
    bool        material = false;
};

// What became of one cell of a move pass (Stage C, Stage D). Moved: the add or
// the place reported success and the cell no longer holds the material. The
// four below it leave the material where it was and the pass stays on; the
// last two turn the pass off for the session: a material left the grid without
// the game confirming the move (a possible loss), or the game confirmed the
// move and the grid cannot show the material gone (a possible duplicate).
//
// Stage C's `not-stackable` (the has-a-stack check said no) is retired: for a
// material whose type has no stack yet that check always says no, and Stage D
// measured the route the game's own click takes then (research doc, § Stage D
// results) - so a "no" is a route now, and its refusals are the two below.
enum class AutoProspectMoveOutcome : int {
    None = 0,
    Moved,
    NoPreferredGrid,  // the has-a-stack check said no, and the game named no grid for the item; nothing was placed
    NotPlaced,        // the place into that grid did not report success, and the cell is unchanged
    NotAdded,         // the add did not report success, and the cell is unchanged
    MoveFailed,       // a call did not run, the lookup was not an item, the cell was unreadable or already changed
    Vanished,         // the cell lost the material without the add or the place reporting success
    CellKept,         // the add or the place reported success, and the final read still holds it or could not be made
    Count
};

// Which route a cell took: the existing stack (the has-a-stack check said yes,
// then the add), or a new type's place (it said no, the game's preferred grid
// for the item, then the place). None: neither call was made.
enum class AutoProspectMoveRoute : int { None = 0, Stack, Place };

// Which step a `move-failed` cell failed at: the earliest call that did not
// run, or the read that could not be made (FailedStep). None: the cell was
// not move-failed. The player's one move-failed line names it.
enum class AutoProspectMoveStep : int {
    None = 0,
    CellBefore,     // the cell was unreadable, or no longer held the fingerprint, before the first call
    ItemLookup,     // the item lookup did not run, or returned no material
    HasStackCheck,  // the has-a-stack check did not run
    PreferredGrid,  // the preferred-grid lookup did not run
    Add,            // the add did not run (the stack route)
    Place,          // the place did not run (the new-type route, grid named)
    FinalRead       // every call ran without success, and the final re-read could not be made
};

// How the calls for one cell went, as the adapter saw them, in order: the
// cell re-read before the first call, the item lookup, the has-a-stack check,
// then either the add or the preferred-grid lookup and the place, the clear,
// and the cell re-read at the end.
struct AutoProspectMoveReport {
    bool heldBefore = false;    // the cell was read and still held the fingerprint the view saw
    bool lookup = false;        // the item lookup ran and returned an item
    bool canAddRan = false;     // the has-a-stack check ran
    bool canAdd = false;        // and said yes
    bool addRan = false;        // the add ran (the stack route)
    bool preferredRan = false;  // the preferred-grid lookup ran (the new-type route)
    bool preferredOk = false;   // and returned the recorded shape: a struct whose `grid` member is an array
    bool placeRan = false;      // the place into that grid ran
    AutoProspectMoveRoute route = AutoProspectMoveRoute::None;
    bool success = false;       // the add's or the place's result said success
    bool clearRan = false;      // the clear ran (only ever after success, on the same fingerprint)
    int  heldAfter = -1;        // the final re-read: 1 still holds it, 0 no longer does, -1 unreadable
};

// Why an insert was not prospected. Every one drops the pending insert: the
// item stays in the grid for the player, and nothing retries it every frame.
enum class AutoProspectRefusal : int {
    None = 0,
    NoWindow,      // no open prospect window
    NoGrid,        // no ProspectGrid node
    Unreadable,    // the node's cells could not be read
    NodeChanged,   // the pending insert was into a node that is gone
    NoButton,      // the Prospect button was not found, or was ambiguous
    GridFull,      // fewer free cells than kAutoProspectMinFreeCells
    NoArgs,        // the button was found, but its kAutoProspectArgsVar is not an array
    Count
};

// What the adapter re-read this frame. An unread field is never a value: a
// missing window, node or cell read is its own flag, not a zero.
struct AutoProspectView {
    bool        window = false;
    bool        grid = false;       // the ProspectGrid node was found
    bool        button = false;     // exactly one Prospect button, linked to the window
    bool        args = false;       // that button's kAutoProspectArgsVar is an array
    bool        contents = false;   // the node's cells were read
    int64_t     nodeId = -1;
    int         filled = 0;         // cells holding something (idcheck's empty rule)
    int         empty = 0;
    std::string fingerprints;       // the distinct nodeFingerprint values, in order, joined
    std::vector<std::string> printList;    // the same values, one per entry, never re-split from the joined text
    std::vector<AutoProspectCell> cells;   // filled only when NeedsMaterials() asked for it
};

struct AutoProspectDecision {
    AutoProspectAction  action = AutoProspectAction::None;
    AutoProspectRefusal reason = AutoProspectRefusal::None;
    std::vector<AutoProspectCell> moves;   // MoveMaterials: the batch's material cells, and only those
};

// Threading: every caller runs on the game thread - the hook body inside the
// game's own call, the tick and the IPC poll from FrameCallback - so nothing is
// locked. The flag and counters are atomic only so a stat read can never
// observe a torn value.
class AutoProspectMod {
public:
    static AutoProspectMod& Instance() {
        static AutoProspectMod s_Instance;
        return s_Instance;
    }

    // Public so the harness can build a fresh instance per scenario; the
    // plugin only ever uses Instance().
    AutoProspectMod() = default;

    bool IsEnabled() const { return m_Enabled.load(); }

    // Either way the node is forgotten, so the first frame after turning on
    // settles the count again, and a pending insert is dropped. Counters and
    // the once-per-reason report survive: they are what a stat line is read
    // from, and a refusal already named this session is not named again. The
    // recorded batch is forgotten (while off the core sees nothing, so it
    // cannot know the grid still holds it). The bag sub-option is left alone:
    // it is its own switch.
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled);
        DropPending();
        m_NodeId = -1;
        m_Settled = -1;
        m_CheckEffect = false;
        m_Batch.clear();
    }

    // The `bag` sub-option (Stage C): on by default, under the off-by-default
    // parent. Turning it on is refused (false) once a `vanished` or
    // `cell-kept` has turned the pass off for the session.
    bool BagEnabled() const { return m_BagEnabled.load(); }
    bool BagOffThisSession() const { return m_BagOffThisSession.load(); }
    // Empty while the pass is healthy; the sentence the panel shows otherwise.
    const char* BagOffReason() const { return m_BagOffReason.load(); }
    bool SetBagEnabled(bool on) {
        if (on && m_BagOffThisSession.load()) return false;
        m_BagEnabled.store(on);
        return true;
    }
    // Whether a move pass may run at all: the sub-option on and not turned off
    // for the session. The adapter re-checks it before every cell, so a pass
    // stops at the first cell that turns it off.
    bool MovePassOn() const { return m_BagEnabled.load() && !m_BagOffThisSession.load(); }

    // True while the next Decide may ask for a move pass, so the adapter reads
    // each cell's item type (at most one lookup per cell) only then: an
    // insert is pending, the pass is on, it has not run for this insert, and
    // there is a recorded batch - so the first prospect after anything that
    // forgets the batch does no item lookup at all.
    bool NeedsMaterials() const {
        return m_Enabled.load() && m_Pending && !m_PassDone && MovePassOn() && !m_Batch.empty();
    }

    // How many distinct fingerprints the recorded batch holds (`batch=` on
    // the stat line).
    size_t BatchSize() const { return m_Batch.size(); }
    // The recorded batch itself, a copy (Stage D: the research build's
    // per-pass and per-invoke log lines name its fingerprints). Reading it
    // changes nothing.
    std::vector<std::string> BatchList() const { return m_Batch; }

    // From the m_MoveItemToGrid hook, after the game's own function ran.
    // `isProspectGrid`: the call's self is the ProspectGrid node, identified by
    // what it is. `invoking`: the adapter is inside its own call of the
    // handler, which may move materials into the grid - those never count.
    void OnInsert(int64_t nodeId, bool isProspectGrid, bool invoking) {
        if (!m_Enabled.load()) { m_SkippedDisabled.fetch_add(1); return; }
        if (invoking) { m_WhileInvoking.fetch_add(1); return; }
        if (!isProspectGrid) { m_Elsewhere.fetch_add(1); return; }
        m_Inserts.fetch_add(1);
        if (m_Pending && m_PendingNode == nodeId) m_Coalesced.fetch_add(1);
        else { m_Landed = false; m_PassDone = false; }
        m_Pending = true;
        m_PendingNode = nodeId;
        m_PendingAge = 0;
    }

    bool HasPending() const { return m_Pending; }

    // True while the next Decide can use the button and the fingerprints: an
    // insert is pending, or an invoke's effect is checked this frame. The
    // adapter reads them only then, so an open window with nothing happening
    // costs a cell count per frame, not a button search.
    bool NeedsDetail() const { return m_Pending || m_CheckEffect; }

    // Once per frame while the mod is on. Invoke only when an insert was seen
    // into THIS node AND the filled count is above the SETTLED count.
    //
    // The settled count is what the grid held when the core last accounted
    // for all of it: first sight of the node, the grid re-read straight after
    // an invoke, an insert that was refused (its item stays), or a pending
    // insert that expired. Between those it only ever goes DOWN, following
    // removals (the player took materials out); a fill the core was not told
    // about never raises it. That matters because a click-in's cell is already
    // filled when the insert closure runs (Phase 1: `contents=6->6` across the
    // hook), and whether that fill can fall in an earlier frame than the hook
    // is not measured: an earlier core re-read the count every frame and so
    // read such an insert as a rearrangement. Moving something already
    // settled inside the grid never raises the count, so it still never fires.
    AutoProspectDecision Decide(const AutoProspectView& in) {
        AutoProspectDecision d;
        if (!m_Enabled.load()) { DropPending(); return d; }
        CheckEffect(in);
        if (!in.window) { m_NodeId = -1; m_Settled = -1; return RefuseIfPending(AutoProspectRefusal::NoWindow); }
        if (!in.grid) { m_NodeId = -1; m_Settled = -1; return RefuseIfPending(AutoProspectRefusal::NoGrid); }
        if (!in.contents) return RefuseIfPending(AutoProspectRefusal::Unreadable);
        if (in.nodeId != m_NodeId || m_Settled < 0) {
            // First sight of this node (or its count was lost): the count is
            // the baseline, and no batch recorded before it can be accounted for.
            m_NodeId = in.nodeId;
            m_Settled = in.filled;
            m_Batch.clear();
            if (m_Pending && m_PendingNode != in.nodeId) return RefuseIfPending(AutoProspectRefusal::NodeChanged);
        }
        if (!m_Pending) {
            // A removal: the player took something out. If it was a batch
            // stack and comes back, it comes back as an insert, so forget it.
            if (in.filled < m_Settled) { m_Settled = in.filled; m_Batch.clear(); }
            return d;
        }
        if (m_PendingNode != in.nodeId) return RefuseIfPending(AutoProspectRefusal::NodeChanged);
        // Once landed, the insert stays landed until it is invoked or refused:
        // a move pass takes the previous batch out of the grid, so the read
        // after it can be at or below the settled count, and that must not
        // read as not-landed or a rearrangement.
        if (!m_Landed) {
            if (in.filled <= m_Settled) {
                if (++m_PendingAge > kAutoProspectLandFrames) {
                    // A rearrangement or a swap changed the grid without a
                    // rise: which cell holds what is no longer known.
                    DropPending();
                    m_NotLanded.fetch_add(1);
                    m_Settled = in.filled;
                    m_Batch.clear();
                }
                return d;
            }
            m_Landed = true;
        }
        // The insert landed. Everything below re-reads this frame's objects,
        // and a refusal settles the count: the refused item stays in the grid,
        // and moving it later must not prospect it.
        if (!in.button) return RefuseIfPending(AutoProspectRefusal::NoButton, &in);
        if (!in.args) return RefuseIfPending(AutoProspectRefusal::NoArgs, &in);
        // Stage C: only the free-cell check is left, so this is the frame the
        // insert is about to be prospected - move the previous prospect's
        // batch first, once per insert. A cell is named only when its
        // fingerprint is in the recorded batch and the adapter flagged it as
        // a material, at most one cell per fingerprint, matched by
        // fingerprint so a batch stack moved inside the grid is taken from
        // where it sits now. The insert is never in the batch, so it is never
        // named, whatever kind of item it is. The pass consumes the batch: a
        // cell a refusal left behind is not named again.
        if (MovePassOn() && !m_PassDone) {
            m_PassDone = true;
            std::vector<std::string> named;
            for (const AutoProspectCell& c : in.cells)
                if (c.material && Contains(m_Batch, c.fingerprint) && !Contains(named, c.fingerprint)) {
                    named.push_back(c.fingerprint);
                    d.moves.push_back(c);
                }
            m_Batch.clear();
            if (!d.moves.empty()) {
                m_Passes.fetch_add(1);
                d.action = AutoProspectAction::MoveMaterials;
                return d;
            }
        }
        if (in.empty < kAutoProspectMinFreeCells) {
            m_LastFree = in.empty;
            return RefuseIfPending(AutoProspectRefusal::GridFull, &in);
        }
        DropPending();
        m_EffectNode = in.nodeId;
        m_EffectFilled = in.filled;
        m_EffectPrints = in.fingerprints;
        m_InvokePrints = in.printList;   // what the grid held as the invoke was decided, the insert included
        d.action = AutoProspectAction::Invoke;
        return d;
    }

    // After the adapter's one call. `dispatched`: the call reported success
    // (not that the body ran; that is what the next frame's cells show).
    // `after`: the grid re-read straight after the call - the handler changes
    // it inside the call (Phase 1 P-shapes), so its count, materials included,
    // is the new settled count. An unreadable `after` loses the count, and the
    // next readable frame takes it again as a first sight: an insert pending
    // by then waits for a rise, and expires if it never comes.
    //
    // The batch this invoke produced is recorded here: the fingerprints in
    // `after` that were not in the view the invoke was decided on. Both reads
    // share one frame, and an insert the game makes inside our call counts as
    // while-invoking, so nothing the player inserts can be among them. A call
    // that did not dispatch, or an `after` that cannot show the same node,
    // records an empty batch.
    void OnInvoked(bool dispatched, const AutoProspectView& after) {
        m_Invoked.fetch_add(1);
        const bool readable = after.window && after.grid && after.contents && after.nodeId == m_EffectNode;
        if (readable) m_Settled = after.filled;
        else m_Settled = -1;
        m_Batch.clear();
        if (dispatched && readable)
            for (const std::string& fp : after.printList)
                if (!Contains(m_InvokePrints, fp)) m_Batch.push_back(fp);
        m_InvokePrints.clear();
        if (!dispatched) { m_InvokeFailed.fetch_add(1); m_CheckEffect = false; return; }
        m_CheckEffect = true;
    }

    // What one cell's calls add up to (Stage C, the recorded stackmove route
    // plus the success check; Stage D, the recorded new-type route). Nothing
    // before the add or the place changes the grid, so a failure there leaves
    // the material: a has-a-stack "no" whose preferred-grid lookup never ran
    // is move-failed (a call did not run - not the game's answer), one whose
    // lookup ran and named no grid of the recorded shape is no-preferred-grid,
    // a place that ran without success on an unchanged cell is not-placed.
    // After either call, a cell that lost the material counts as moved only
    // when that call said success. After a success, a final read that still
    // holds the material, or that could not be made, cannot show the material
    // gone: the destination has it and the grid may too, a possible duplicate
    // (round 1; round 0 called the unreadable case move-failed and kept the
    // pass on).
    static AutoProspectMoveOutcome ClassifyMove(const AutoProspectMoveReport& r) {
        if (!r.heldBefore || !r.lookup || !r.canAddRan) return AutoProspectMoveOutcome::MoveFailed;
        if (!r.canAdd) {
            if (!r.preferredRan) return AutoProspectMoveOutcome::MoveFailed;
            if (!r.preferredOk) return AutoProspectMoveOutcome::NoPreferredGrid;
            if (r.placeRan && !r.success && r.heldAfter == 1) return AutoProspectMoveOutcome::NotPlaced;
        }
        if (r.success) {
            if (r.heldAfter == 0) return AutoProspectMoveOutcome::Moved;
            return AutoProspectMoveOutcome::CellKept;
        }
        if (r.heldAfter == 0) return AutoProspectMoveOutcome::Vanished;
        if (r.heldAfter < 0 || !r.canAdd || !r.addRan) return AutoProspectMoveOutcome::MoveFailed;
        return AutoProspectMoveOutcome::NotAdded;
    }

    // For a `move-failed` cell, the earliest step that failed, in the order
    // the adapter makes the calls; None for any other outcome.
    static AutoProspectMoveStep FailedStep(const AutoProspectMoveReport& r) {
        if (ClassifyMove(r) != AutoProspectMoveOutcome::MoveFailed) return AutoProspectMoveStep::None;
        if (!r.heldBefore) return AutoProspectMoveStep::CellBefore;
        if (!r.lookup) return AutoProspectMoveStep::ItemLookup;
        if (!r.canAddRan) return AutoProspectMoveStep::HasStackCheck;
        if (!r.canAdd && !r.preferredRan) return AutoProspectMoveStep::PreferredGrid;
        if (!r.canAdd && !r.placeRan) return AutoProspectMoveStep::Place;
        if (r.canAdd && !r.addRan) return AutoProspectMoveStep::Add;
        return AutoProspectMoveStep::FinalRead;
    }

    static const char* MoveStepText(AutoProspectMoveStep s) {
        switch (s) {
        case AutoProspectMoveStep::CellBefore:    return "the cell could not be read, or had changed, before the move";
        case AutoProspectMoveStep::ItemLookup:    return "the item lookup did not run or found no material";
        case AutoProspectMoveStep::HasStackCheck: return "the has-a-stack check did not run";
        case AutoProspectMoveStep::PreferredGrid: return "the preferred-grid lookup did not run";
        case AutoProspectMoveStep::Add:           return "the add to your materials tab did not run";
        case AutoProspectMoveStep::Place:         return "the place into your bag did not run";
        case AutoProspectMoveStep::FinalRead:     return "the cell could not be re-read after the move";
        default:                                  return "";
        }
    }

    // The adapter's report for one cell of a MoveMaterials pass: counted, a
    // non-moved reason queued for its one line per session, and `vanished`
    // or `cell-kept` turning the pass off for the session.
    AutoProspectMoveOutcome OnMoveReport(const AutoProspectMoveReport& r) {
        const AutoProspectMoveOutcome o = ClassifyMove(r);
        m_MoveOutcomes[(int)o].fetch_add(1);
        if (o == AutoProspectMoveOutcome::Moved) {
            if (r.route == AutoProspectMoveRoute::Place) m_MovedNew.fetch_add(1);
            if (m_Moved.fetch_add(1) == 0) m_FirstMoveDue = true;
            return o;
        }
        if (o == AutoProspectMoveOutcome::Vanished || o == AutoProspectMoveOutcome::CellKept) {
            // Why the pass shut itself down, so the panel can say it rather
            // than keep painting the saved preference (review of #54).
            m_BagOffReason.store(o == AutoProspectMoveOutcome::Vanished
                                 ? "a material left the grid without the game confirming it arrived"
                                 : "a material was added but the grid kept it, so it may be a duplicate");
            m_BagOffThisSession.store(true);
        }
        const unsigned bit = 1u << (int)o;
        if (!(m_MoveReportedMask & bit)) {
            m_MoveReportedMask |= bit;
            m_MoveUnreported.push_back(o);
            // Still one line per reason: a move-failed line names the step of
            // the first move-failed cell of the session.
            if (o == AutoProspectMoveOutcome::MoveFailed) m_MoveFailedStep = FailedStep(r);
        }
        return o;
    }

    // Each non-moved reason at most once per session, in the order first seen;
    // None when nothing new is waiting.
    AutoProspectMoveOutcome TakeFirstMoveProblem() {
        if (m_MoveUnreported.empty()) return AutoProspectMoveOutcome::None;
        const AutoProspectMoveOutcome o = m_MoveUnreported.front();
        m_MoveUnreported.erase(m_MoveUnreported.begin());
        return o;
    }

    static const char* MoveOutcomeName(AutoProspectMoveOutcome o) {
        switch (o) {
        case AutoProspectMoveOutcome::Moved:           return "moved";
        case AutoProspectMoveOutcome::NoPreferredGrid: return "no-preferred-grid";
        case AutoProspectMoveOutcome::NotPlaced:       return "not-placed";
        case AutoProspectMoveOutcome::NotAdded:        return "not-added";
        case AutoProspectMoveOutcome::MoveFailed:      return "move-failed";
        case AutoProspectMoveOutcome::Vanished:        return "vanished";
        case AutoProspectMoveOutcome::CellKept:        return "cell-kept";
        default:                                       return "none";
        }
    }

    // The one line a move problem is reported with. The prospect still runs
    // in every case, decided by the free-cell rule.
    std::string MoveProblemLine(AutoProspectMoveOutcome o) const {
        const std::string head = std::string("autoprospect: ") + MoveOutcomeName(o) + " - ";
        switch (o) {
        case AutoProspectMoveOutcome::NoPreferredGrid:
            return head + "a material stays in the grid; it has no stack in your materials tab yet and the game named no grid for it";
        case AutoProspectMoveOutcome::NotPlaced:
            return head + "a material stays in the grid; it has no stack in your materials tab yet and the game did not confirm placing it in your bag";
        case AutoProspectMoveOutcome::NotAdded:
            return head + "a material stays in the grid; the game did not confirm adding it to your materials tab";
        case AutoProspectMoveOutcome::MoveFailed:
            if (m_MoveFailedStep == AutoProspectMoveStep::None)
                return head + "a material stays in the grid; the move could not be made or checked";
            return head + "a material stays in the grid; the move could not be made or checked ("
                + MoveStepText(m_MoveFailedStep) + ")";
        case AutoProspectMoveOutcome::Vanished:
            return head + "a material left the grid without the game confirming the move; moving materials to the bag is off for this session";
        case AutoProspectMoveOutcome::CellKept:
            return head + "the game confirmed the move but the grid did not show the material gone; moving materials to the bag is off for this session";
        default:
            return head + "nothing";
        }
    }

    // True once per session, after the first pass that moved something - for
    // the player build's one line naming work done.
    bool TakeFirstMove() {
        if (!m_FirstMoveDue) return false;
        m_FirstMoveDue = false;
        return true;
    }

    // `autoprospect: first move to bag - invoked=0 ... moved=2 ...`
    std::string FirstMoveLine() const { return HeadedStatLine("first move to bag"); }

    long Moved() const { return m_Moved.load(); }
    // Of those, the ones the new-type route placed (`moved-new=`).
    long MovedNew() const { return m_MovedNew.load(); }
    long MovePasses() const { return m_Passes.load(); }
    long MoveOutcomes(AutoProspectMoveOutcome o) const {
        const int i = (int)o;
        return i > 0 && i < (int)AutoProspectMoveOutcome::Count ? m_MoveOutcomes[i].load() : 0;
    }

    // True once per session, on the frame the first prospect is confirmed -
    // for the one line the PLAYER build logs, naming work done rather than
    // armed state (research doc, S-player-dll). `autoprospect stat` is
    // research-build only.
    bool TakeFirstProspect() {
        if (!m_FirstProspectDue) return false;
        m_FirstProspectDue = false;
        return true;
    }

    // `autoprospect: first prospect - invoked=1 prospected=1 ...`: StatLine's
    // fields, headed by what happened.
    std::string FirstProspectLine() const { return HeadedStatLine("first prospect"); }

    // True once per session, on the frame the first invoke that dispatched
    // but left the grid unchanged is seen (ran-no-effect), and likewise for
    // the first whose effect could not be read (unverified). The player build
    // logs each, so "ON and nothing happened" is a line in out.txt rather
    // than a missing one (Phase 3 S7 saw ran-no-effect=1 only by luck).
    bool TakeFirstRanNoEffect() {
        if (!m_FirstNoEffectDue) return false;
        m_FirstNoEffectDue = false;
        return true;
    }
    bool TakeFirstUnverified() {
        if (!m_FirstUnverifiedDue) return false;
        m_FirstUnverifiedDue = false;
        return true;
    }

    std::string RanNoEffectLine() const { return HeadedStatLine("the Prospect ran but the grid did not change"); }
    std::string UnverifiedLine() const { return HeadedStatLine("the Prospect ran but the grid could not be read afterwards"); }

    // Each reason at most once per session, in the order first seen, for the
    // adapter's one log line; None when nothing new is waiting.
    AutoProspectRefusal TakeFirstRefusal() {
        if (m_Unreported.empty()) return AutoProspectRefusal::None;
        const AutoProspectRefusal r = m_Unreported.front();
        m_Unreported.erase(m_Unreported.begin());
        return r;
    }

    static const char* RefusalName(AutoProspectRefusal r) {
        switch (r) {
        case AutoProspectRefusal::NoWindow:    return "no-window";
        case AutoProspectRefusal::NoGrid:      return "no-grid";
        case AutoProspectRefusal::Unreadable:  return "unreadable";
        case AutoProspectRefusal::NodeChanged: return "node-changed";
        case AutoProspectRefusal::NoButton:    return "no-button";
        case AutoProspectRefusal::GridFull:    return "grid-full";
        case AutoProspectRefusal::NoArgs:      return "no-args";
        default:                               return "none";
        }
    }

    // The one line a refusal is reported with: what the mod did, and what the
    // player can do about it.
    std::string RefusalLine(AutoProspectRefusal r) const {
        const std::string head = std::string("autoprospect: ") + RefusalName(r) + " - ";
        switch (r) {
        case AutoProspectRefusal::GridFull:
            return head + "holding back - " + std::to_string(m_LastFree) + " free cells, needs "
                + std::to_string(kAutoProspectMinFreeCells) + "; empty some of the grid";
        case AutoProspectRefusal::NoButton:
            return head + "the item stays in the grid; the Prospect button was not found (or not uniquely)";
        case AutoProspectRefusal::NoArgs:
            return head + "the item stays in the grid; the Prospect button's " + kAutoProspectArgsVar + " is not an array";
        case AutoProspectRefusal::NoWindow:
            return head + "the window closed with an insert pending; nothing was prospected";
        case AutoProspectRefusal::NoGrid:
            return head + "no prospect grid found; nothing was prospected";
        case AutoProspectRefusal::Unreadable:
            return head + "the grid could not be read; nothing was prospected";
        case AutoProspectRefusal::NodeChanged:
            return head + "the grid was replaced with an insert pending; nothing was prospected";
        default:
            return head + "nothing";
        }
    }

    long Inserts() const { return m_Inserts.load(); }
    long Coalesced() const { return m_Coalesced.load(); }
    long Elsewhere() const { return m_Elsewhere.load(); }
    long WhileInvoking() const { return m_WhileInvoking.load(); }
    long SkippedDisabled() const { return m_SkippedDisabled.load(); }
    long Invoked() const { return m_Invoked.load(); }
    long InvokeFailed() const { return m_InvokeFailed.load(); }
    long Prospected() const { return m_Prospected.load(); }
    long RanNoEffect() const { return m_RanNoEffect.load(); }
    long EffectUnread() const { return m_EffectUnread.load(); }
    long NotLanded() const { return m_NotLanded.load(); }
    long Refused(AutoProspectRefusal r) const {
        const int i = (int)r;
        return i > 0 && i < (int)AutoProspectRefusal::Count ? m_Refused[i].load() : 0;
    }
    int Settled() const { return m_Settled; }

    // `autoprospect: ON invoked=3 prospected=2 ran-no-effect=1 failed=0 ...`
    // A line that names what was done, so "ON and nothing happened" reads as
    // invoked=0 or ran-no-effect=N instead of as silence (guide, Known
    // Limitations item 7).
    std::string StatLine() const {
        std::string refused;
        for (int i = 1; i < (int)AutoProspectRefusal::Count; ++i)
            refused += std::string(i > 1 ? " " : "") + RefusalName((AutoProspectRefusal)i) + "=" + std::to_string(m_Refused[i].load());
        return std::string("autoprospect: ") + (IsEnabled() ? "ON" : "off")
            + " invoked=" + std::to_string(Invoked())
            + " prospected=" + std::to_string(Prospected())
            + " ran-no-effect=" + std::to_string(RanNoEffect())
            + " unverified=" + std::to_string(EffectUnread())
            + " failed=" + std::to_string(InvokeFailed())
            + " inserts=" + std::to_string(Inserts())
            + " (coalesced=" + std::to_string(Coalesced())
            + " while-invoking=" + std::to_string(WhileInvoking())
            + " elsewhere=" + std::to_string(Elsewhere())
            + " while-off=" + std::to_string(SkippedDisabled()) + ")"
            + " not-landed=" + std::to_string(NotLanded())
            + " refused(" + refused + ")"
            + " moved=" + std::to_string(Moved())
            + " moved-new=" + std::to_string(MovedNew())
            + " passes=" + std::to_string(MovePasses())
            + " no-preferred-grid=" + std::to_string(MoveOutcomes(AutoProspectMoveOutcome::NoPreferredGrid))
            + " not-placed=" + std::to_string(MoveOutcomes(AutoProspectMoveOutcome::NotPlaced))
            + " not-added=" + std::to_string(MoveOutcomes(AutoProspectMoveOutcome::NotAdded))
            + " move-failed=" + std::to_string(MoveOutcomes(AutoProspectMoveOutcome::MoveFailed))
            + " vanished=" + std::to_string(MoveOutcomes(AutoProspectMoveOutcome::Vanished))
            + " cell-kept=" + std::to_string(MoveOutcomes(AutoProspectMoveOutcome::CellKept))
            + " batch=" + std::to_string(BatchSize())
            + " bag=" + (m_BagOffThisSession.load() ? "off-this-session" : (m_BagEnabled.load() ? "on" : "off"));
    }

private:
    // `autoprospect: <what happened> - invoked=1 prospected=1 ...`: StatLine's
    // fields, headed by what happened instead of by ON/off.
    std::string HeadedStatLine(const std::string& what) const {
        const std::string stat = StatLine();
        const std::string head = std::string("autoprospect: ") + (IsEnabled() ? "ON " : "off ");
        return "autoprospect: " + what + " - " + (stat.rfind(head, 0) == 0 ? stat.substr(head.size()) : stat);
    }

    static bool Contains(const std::vector<std::string>& list, const std::string& fp) {
        for (const std::string& s : list)
            if (s == fp) return true;
        return false;
    }

    // The pending insert is done with - invoked, refused, expired, or the mod
    // turned off - and with it its landed state and its move pass.
    void DropPending() {
        m_Pending = false;
        m_PendingAge = 0;
        m_Landed = false;
        m_PassDone = false;
    }

    // `settle`: the frame the refusal read, whose count now includes the
    // refused item; null when this frame could not read the node.
    AutoProspectDecision RefuseIfPending(AutoProspectRefusal r, const AutoProspectView* settle = nullptr) {
        AutoProspectDecision d;
        if (!m_Pending) return d;
        DropPending();
        if (settle) m_Settled = settle->filled;
        m_Refused[(int)r].fetch_add(1);
        const unsigned bit = 1u << (int)r;
        if (!(m_ReportedMask & bit)) { m_ReportedMask |= bit; m_Unreported.push_back(r); }
        d.action = AutoProspectAction::Refuse;
        d.reason = r;
        return d;
    }

    // One frame after a dispatched invoke: the same node with the same filled
    // count and the same fingerprints means the call ran and did nothing (the
    // idea behind petquest's dispatched-but-item-remained). A frame that
    // cannot show the node is counted apart, never as either.
    void CheckEffect(const AutoProspectView& in) {
        if (!m_CheckEffect) return;
        m_CheckEffect = false;
        if (!in.window || !in.grid || !in.contents || in.nodeId != m_EffectNode) {
            if (m_EffectUnread.fetch_add(1) == 0) m_FirstUnverifiedDue = true;
            return;
        }
        if (in.filled == m_EffectFilled && in.fingerprints == m_EffectPrints) {
            if (m_RanNoEffect.fetch_add(1) == 0) m_FirstNoEffectDue = true;
        }
        else if (m_Prospected.fetch_add(1) == 0) m_FirstProspectDue = true;
    }

    std::atomic<bool> m_Enabled{ false };
    std::atomic<bool> m_BagEnabled{ true };          // the `bag` sub-option; on by default
    std::atomic<bool> m_BagOffThisSession{ false };  // a vanished or cell-kept turned the pass off
    std::atomic<const char*> m_BagOffReason{ "" };   // why, for the panel

    bool        m_Pending = false;
    int64_t     m_PendingNode = -1;
    int         m_PendingAge = 0;
    bool        m_Landed = false;     // the pending insert has landed; it stays landed across a move pass
    bool        m_PassDone = false;   // the move pass was considered for the pending insert
    int64_t     m_NodeId = -1;        // the node the settled count belongs to
    int         m_Settled = -1;
    int         m_LastFree = 0;       // free cells at the last grid-full, for its line
    // The previous prospect's batch: the distinct fingerprints the last
    // invoke produced (OnInvoked), until a pass consumes it or something the
    // core cannot account for forgets it. The only cells a pass may name.
    std::vector<std::string> m_Batch;
    std::vector<std::string> m_InvokePrints;   // the invoking view's fingerprints, for OnInvoked

    bool        m_CheckEffect = false;
    bool        m_FirstProspectDue = false;
    bool        m_FirstNoEffectDue = false;
    bool        m_FirstUnverifiedDue = false;
    int64_t     m_EffectNode = -1;
    int         m_EffectFilled = 0;
    std::string m_EffectPrints;

    unsigned    m_ReportedMask = 0;
    std::vector<AutoProspectRefusal> m_Unreported;

    std::atomic<long> m_Inserts{ 0 };
    std::atomic<long> m_Coalesced{ 0 };
    std::atomic<long> m_Elsewhere{ 0 };
    std::atomic<long> m_WhileInvoking{ 0 };
    std::atomic<long> m_SkippedDisabled{ 0 };
    std::atomic<long> m_Invoked{ 0 };
    std::atomic<long> m_InvokeFailed{ 0 };
    std::atomic<long> m_Prospected{ 0 };
    std::atomic<long> m_RanNoEffect{ 0 };
    std::atomic<long> m_EffectUnread{ 0 };
    std::atomic<long> m_NotLanded{ 0 };
    std::atomic<long> m_Refused[(int)AutoProspectRefusal::Count]{};

    bool        m_FirstMoveDue = false;
    unsigned    m_MoveReportedMask = 0;
    std::vector<AutoProspectMoveOutcome> m_MoveUnreported;
    AutoProspectMoveStep m_MoveFailedStep = AutoProspectMoveStep::None;
    std::atomic<long> m_Moved{ 0 };
    std::atomic<long> m_MovedNew{ 0 };
    std::atomic<long> m_Passes{ 0 };
    std::atomic<long> m_MoveOutcomes[(int)AutoProspectMoveOutcome::Count]{};
};

} // namespace ForgePact
