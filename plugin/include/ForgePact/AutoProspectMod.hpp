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

enum class AutoProspectAction { None, Invoke, Refuse };

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
};

struct AutoProspectDecision {
    AutoProspectAction  action = AutoProspectAction::None;
    AutoProspectRefusal reason = AutoProspectRefusal::None;
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
    // from, and a refusal already named this session is not named again.
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled);
        m_Pending = false;
        m_PendingAge = 0;
        m_NodeId = -1;
        m_Settled = -1;
        m_CheckEffect = false;
    }

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
        if (!m_Enabled.load()) { m_Pending = false; return d; }
        CheckEffect(in);
        if (!in.window) { m_NodeId = -1; m_Settled = -1; return RefuseIfPending(AutoProspectRefusal::NoWindow); }
        if (!in.grid) { m_NodeId = -1; m_Settled = -1; return RefuseIfPending(AutoProspectRefusal::NoGrid); }
        if (!in.contents) return RefuseIfPending(AutoProspectRefusal::Unreadable);
        if (in.nodeId != m_NodeId || m_Settled < 0) {
            // First sight of this node (or its count was lost): the count is
            // the baseline.
            m_NodeId = in.nodeId;
            m_Settled = in.filled;
            if (m_Pending && m_PendingNode != in.nodeId) return RefuseIfPending(AutoProspectRefusal::NodeChanged);
        }
        if (!m_Pending) {
            if (in.filled < m_Settled) m_Settled = in.filled;
            return d;
        }
        if (m_PendingNode != in.nodeId) return RefuseIfPending(AutoProspectRefusal::NodeChanged);
        if (in.filled <= m_Settled) {
            if (++m_PendingAge > kAutoProspectLandFrames) {
                m_Pending = false;
                m_NotLanded.fetch_add(1);
                m_Settled = in.filled;
            }
            return d;
        }
        // The insert landed. Everything below re-reads this frame's objects,
        // and a refusal settles the count: the refused item stays in the grid,
        // and moving it later must not prospect it.
        if (!in.button) return RefuseIfPending(AutoProspectRefusal::NoButton, &in);
        if (!in.args) return RefuseIfPending(AutoProspectRefusal::NoArgs, &in);
        if (in.empty < kAutoProspectMinFreeCells) {
            m_LastFree = in.empty;
            return RefuseIfPending(AutoProspectRefusal::GridFull, &in);
        }
        m_Pending = false;
        m_PendingAge = 0;
        m_EffectNode = in.nodeId;
        m_EffectFilled = in.filled;
        m_EffectPrints = in.fingerprints;
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
    void OnInvoked(bool dispatched, const AutoProspectView& after) {
        m_Invoked.fetch_add(1);
        if (after.window && after.grid && after.contents && after.nodeId == m_EffectNode) m_Settled = after.filled;
        else m_Settled = -1;
        if (!dispatched) { m_InvokeFailed.fetch_add(1); m_CheckEffect = false; return; }
        m_CheckEffect = true;
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
    std::string FirstProspectLine() const {
        const std::string stat = StatLine();
        const std::string head = std::string("autoprospect: ") + (IsEnabled() ? "ON " : "off ");
        return "autoprospect: first prospect - " + (stat.rfind(head, 0) == 0 ? stat.substr(head.size()) : stat);
    }

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
                + std::to_string(kAutoProspectMinFreeCells) + "; take the materials out";
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
            + " refused(" + refused + ")";
    }

private:
    // `settle`: the frame the refusal read, whose count now includes the
    // refused item; null when this frame could not read the node.
    AutoProspectDecision RefuseIfPending(AutoProspectRefusal r, const AutoProspectView* settle = nullptr) {
        AutoProspectDecision d;
        if (!m_Pending) return d;
        m_Pending = false;
        m_PendingAge = 0;
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
        if (!in.window || !in.grid || !in.contents || in.nodeId != m_EffectNode) { m_EffectUnread.fetch_add(1); return; }
        if (in.filled == m_EffectFilled && in.fingerprints == m_EffectPrints) m_RanNoEffect.fetch_add(1);
        else if (m_Prospected.fetch_add(1) == 0) m_FirstProspectDue = true;
    }

    std::atomic<bool> m_Enabled{ false };

    bool        m_Pending = false;
    int64_t     m_PendingNode = -1;
    int         m_PendingAge = 0;
    int64_t     m_NodeId = -1;        // the node the settled count belongs to
    int         m_Settled = -1;
    int         m_LastFree = 0;       // free cells at the last grid-full, for its line

    bool        m_CheckEffect = false;
    bool        m_FirstProspectDue = false;
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
};

} // namespace ForgePact
