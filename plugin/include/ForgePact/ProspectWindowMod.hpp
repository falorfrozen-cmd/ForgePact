#pragma once

#include "Common.hpp"

namespace ForgePact {

// Bigger prospect window (ForgePact issue #9): the prospecting cube's input
// grid (`UI_Prospect_obj`) holds too few items next to the inventory it sits
// beside.
//
// RESEARCH STAGE. What sizes that grid in the game is not yet measured - it is
// either the arguments of a named sizing call (Design A) or dimension variables
// the window's Create event leaves on the instance (Design B); see
// docs/prospect-window-research.md, whose Phase 0 decides between them. This
// header is therefore only the DECISION the mod will make once the mechanism
// is known, kept game-independent on purpose: it names no runtime interface,
// builtin, log call or runtime value type (test_prospect_window_contract.py
// checks the spellings), so tests/prospect_window_harness.cpp compiles it
// whole with no runtime stub, and whichever adapter Stage B adds in
// ModuleMain.cpp stays the only code that touches the runtime.
//
// Nothing calls this class yet, and no command exposes it.

struct GridSize { int cols; int rows; };

// PLACEHOLDERS until the live session measures the vanilla grid (R3) and the
// inventory grid (R8); the default decision (D2) is the inventory grid's size,
// capped at 2x vanilla per axis. The harness reads these by declaration, so
// retuning them does not change what its scenarios mean.
static constexpr int kProspectColsFactor = 2;
static constexpr int kProspectRowsFactor = 2;
static constexpr int kProspectMaxCols = 24;
static constexpr int kProspectMaxRows = 24;
// How many window instance ids the once-per-instance latch remembers. A window
// is one instance per open, so this is far more than can be live at once; the
// bound only stops a long session from growing the set forever.
static constexpr size_t kProspectLatchCapacity = 64;

// Threading: every caller runs on the game thread - the IPC poll is driven
// from FrameCallback, and a script hook body runs inside the game's own call -
// so the latch is not locked. The flag and counters are atomic only so a stat
// read can never observe a torn value.
class ProspectWindowMod {
public:
    static ProspectWindowMod& Instance() {
        static ProspectWindowMod s_Instance;
        return s_Instance;
    }

    // Public so the harness can build a fresh instance per scenario; the
    // plugin only ever uses Instance().
    ProspectWindowMod() = default;

    bool IsEnabled() const { return m_Enabled.load(); }

    // Disabling forgets which windows were already resized, so turning the mod
    // back on applies to a window again; the counters are kept, because they
    // are what a pasted stat line is read from.
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled);
        if (!enabled) { m_Seen.clear(); m_SeenOrder.clear(); }
    }

    // The size a window should get. Returns false with out = vanilla when the
    // mod is off, or when the vanilla size is not a real grid (either axis
    // <= 0 - an unreadable value must never be scaled into a plausible one).
    // Otherwise out = vanilla x factor, clamped to the cap per axis, and never
    // smaller than vanilla, and returns true.
    bool Decide(GridSize vanilla, GridSize& out) const {
        out = vanilla;
        if (!m_Enabled.load()) { m_SkippedDisabled.fetch_add(1); return false; }
        if (vanilla.cols <= 0 || vanilla.rows <= 0) { m_RefusedInvalid.fetch_add(1); return false; }
        out.cols = Axis(vanilla.cols, kProspectColsFactor, kProspectMaxCols);
        out.rows = Axis(vanilla.rows, kProspectRowsFactor, kProspectMaxRows);
        return true;
    }

    // True exactly once per window instance id while enabled, so a window is
    // resized when it opens and never compounded on a later call or frame.
    bool ShouldApplyTo(int64_t instanceId) {
        if (!m_Enabled.load()) return false;
        if (m_Seen.find(instanceId) != m_Seen.end()) { m_SkippedAlready.fetch_add(1); return false; }
        m_Seen.insert(instanceId);
        m_SeenOrder.push_back(instanceId);
        while (m_SeenOrder.size() > kProspectLatchCapacity) {
            m_Seen.erase(m_SeenOrder.front());
            m_SeenOrder.erase(m_SeenOrder.begin());
        }
        return true;
    }

    // Called by the adapter after the new size was actually written, so
    // `applied` counts what the mod did rather than what it decided.
    void RecordApplied(GridSize vanilla, GridSize applied) {
        m_Applied.fetch_add(1);
        m_LastVanilla = vanilla;
        m_LastApplied = applied;
    }

    size_t LatchSize() const { return m_SeenOrder.size(); }
    long Applied() const { return m_Applied.load(); }
    long SkippedDisabled() const { return m_SkippedDisabled.load(); }
    long SkippedAlready() const { return m_SkippedAlready.load(); }
    long RefusedInvalid() const { return m_RefusedInvalid.load(); }
    GridSize LastVanilla() const { return m_LastVanilla; }
    GridSize LastApplied() const { return m_LastApplied; }

    // `prospectsize: ON applied=1 last=4x4->8x8 skipped(already=0 disabled=0) refused(invalid=0)`
    // A line that names what was done, so "ON and nothing happened" reads as
    // applied=0 instead of as silence (guide, Known Limitations item 7).
    std::string StatLine() const {
        return std::string("prospectsize: ") + (IsEnabled() ? "ON" : "off")
            + " applied=" + std::to_string(Applied())
            + " last=" + Str(m_LastVanilla) + "->" + Str(m_LastApplied)
            + " skipped(already=" + std::to_string(SkippedAlready())
            + " disabled=" + std::to_string(SkippedDisabled()) + ")"
            + " refused(invalid=" + std::to_string(RefusedInvalid()) + ")";
    }

private:
    static int Axis(int vanilla, int factor, int cap) {
        const long long scaled = (long long)vanilla * (long long)factor;
        const long long capped = scaled < (long long)cap ? scaled : (long long)cap;
        return (int)(capped > (long long)vanilla ? capped : (long long)vanilla);
    }
    static std::string Str(GridSize g) { return std::to_string(g.cols) + "x" + std::to_string(g.rows); }

    std::atomic<bool> m_Enabled{ false };
    std::unordered_set<int64_t> m_Seen;
    std::vector<int64_t> m_SeenOrder;   // insertion order, oldest first, for eviction
    mutable std::atomic<long> m_SkippedDisabled{ 0 };
    mutable std::atomic<long> m_RefusedInvalid{ 0 };
    std::atomic<long> m_SkippedAlready{ 0 };
    std::atomic<long> m_Applied{ 0 };
    GridSize m_LastVanilla{ 0, 0 };
    GridSize m_LastApplied{ 0, 0 };
};

} // namespace ForgePact
