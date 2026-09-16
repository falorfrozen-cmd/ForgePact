#pragma once

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ForgePact {

// Minimap marker smoothing - the game-independent core (ForgePact issue #19,
// "map in performance mode interpolation").
//
// STATUS: research stage. Nothing in the player build calls this yet. Which
// routine refills the marker container, which routine draws from it, and what
// a marker record looks like are all unmeasured; `mmprobe` (research build) is
// the instrument that answers them, and docs/minimap-smoothing-research.md is
// where the answers go. The adapter that feeds this class is written only once
// those answers exist.
//
// What it does, independent of any of that: when the game refreshes the
// minimap's dynamic markers once every N frames, each marker is drawn at a
// position interpolated from where it was DISPLAYED at the latest refresh to
// where that refresh reported it, over the measured refresh period. The
// minimap therefore lags the game by exactly one refresh period and never
// shows a marker anywhere it was not reported (no extrapolation).
//
// Pure C++ on purpose: no runtime include, no YYToolkit interface. The adapter passes
// samples in and takes positions out, so tests/minimap_smooth_harness.cpp
// compiles this header verbatim and every decision below is exercised
// without a game process.
//
// Gate (human decision D1, 2026-09-16): smoothing engages only when the
// MEASURED gap between the last two refreshes is at least kMinSmoothGapFrames.
// That needs no knowledge of how the game stores its performance-mode option,
// cannot misfire when that option is renamed, and is evaluated where the
// positions are written rather than at a frame boundary. With the game
// refreshing every frame nothing is written at all.
struct MarkerSample {
    // Identity the marker keeps across refreshes (an instance id when the
    // record carries one). When the adapter can only offer the index, it says
    // so via OnRefresh's `keysAreIndices`, and a count change then means every
    // marker is new rather than silently pairing unrelated markers.
    uint64_t key = 0;
    double   x   = 0.0;
    double   y   = 0.0;
};

class MinimapSmoothManager {
public:
    // Below this measured refresh gap the game is updating the minimap
    // (nearly) every frame and there is nothing to smooth.
    static constexpr int64_t kMinSmoothGapFrames = 4;
    // Upper bound on markers written per frame; the excess is drawn where the
    // game put it and is counted in `truncated`, never dropped silently.
    static constexpr size_t  kMaxSmoothedMarkers = 128;
    // Period assumed before a second refresh has been measured.
    static constexpr int64_t kDefaultPeriodFrames = 60;
    // Clamp for a measured gap (a pause, a zone load) so one long stall does
    // not stretch the next glide over many seconds.
    static constexpr int64_t kMaxPeriodFrames = 600;

    struct Counters {
        uint64_t refreshes         = 0;   // OnRefresh calls
        uint64_t engagedFrames     = 0;   // Interpolate calls that produced positions
        uint64_t passthroughFrames = 0;   // enabled, but nothing to write (cadence too fast / nothing captured)
        uint64_t writtenTotal      = 0;   // positions produced, summed over engaged frames
        uint64_t matched           = 0;   // per refresh: markers found in the previous snapshot
        uint64_t newMarkers        = 0;   // per refresh: markers shown where reported
        uint64_t droppedMarkers    = 0;   // per refresh: markers gone since the previous snapshot
        uint64_t truncated         = 0;   // per engaged frame: markers above kMaxSmoothedMarkers
        int64_t  lastGap           = -1;  // frames between the last two refreshes; -1 until measured
        int64_t  period            = kDefaultPeriodFrames;
    };

    static MinimapSmoothManager& Instance() {
        static MinimapSmoothManager s_Instance;
        return s_Instance;
    }

    bool IsEnabled() const { return m_Enabled.load(std::memory_order_relaxed); }
    void SetEnabled(bool enabled) {
        m_Enabled.store(enabled, std::memory_order_relaxed);
        // Whatever was last produced is not what is on screen any more once
        // the game draws its own positions again.
        if (!enabled) m_LastOut.clear();
    }

    // Forget every snapshot and counter. The enabled flag is left alone.
    void Reset() {
        m_Prev.clear();
        m_Cur.clear();
        m_LastOut.clear();
        m_PrevIndexForCur.clear();
        m_HaveCur = false;
        m_CurFrame = 0;
        m_Engaged = false;
        m_Stats = Counters{};
    }

    bool Engaged() const { return m_Engaged; }
    int64_t PeriodFrames() const { return m_Stats.period; }
    const Counters& Stats() const { return m_Stats; }

    // The game refreshed its marker container on `frame`; `fresh` is what it
    // now holds, in container order. Called once per refresh, not per frame.
    void OnRefresh(uint64_t frame, const std::vector<MarkerSample>& fresh, bool keysAreIndices = false) {
        ++m_Stats.refreshes;
        if (m_HaveCur) {
            const int64_t gap = (int64_t)(frame - m_CurFrame);
            m_Stats.lastGap = gap;
            m_Stats.period = std::clamp<int64_t>(gap, 1, kMaxPeriodFrames);
            m_Engaged = m_Stats.period >= kMinSmoothGapFrames;
        } else {
            // One refresh is not a cadence. Engaging on the default period
            // would write positions on the very first frame of a minimap the
            // game refreshes every frame.
            m_Engaged = false;
        }

        // Rotate from what was DISPLAYED, not from the previous snapshot. A
        // refresh that arrives early lands while the glide is still short of
        // the old target; starting the new glide at the old target instead
        // would jump every marker forward in one frame - the pop this exists
        // to remove.
        m_Prev = m_LastOut.empty() ? m_Cur : m_LastOut;
        m_Cur = fresh;
        m_CurFrame = frame;
        m_HaveCur = true;

        // Pair markers once per refresh so the per-frame path does no lookups.
        m_PrevIndexForCur.assign(m_Cur.size(), -1);
        const bool indexIdentityBroken = keysAreIndices && m_Prev.size() != m_Cur.size();
        size_t matchedNow = 0;
        if (!indexIdentityBroken && !m_Prev.empty()) {
            std::unordered_map<uint64_t, ptrdiff_t> prevByKey;
            prevByKey.reserve(m_Prev.size());
            for (size_t i = 0; i < m_Prev.size(); ++i) prevByKey.emplace(m_Prev[i].key, (ptrdiff_t)i);
            for (size_t i = 0; i < m_Cur.size(); ++i) {
                auto it = prevByKey.find(m_Cur[i].key);
                if (it != prevByKey.end()) { m_PrevIndexForCur[i] = it->second; ++matchedNow; }
            }
        }
        m_Stats.matched += matchedNow;
        m_Stats.newMarkers += m_Cur.size() - matchedNow;
        m_Stats.droppedMarkers += m_Prev.size() > matchedNow ? m_Prev.size() - matchedNow : 0;
    }

    // Positions to draw on `frame`. Returns false - and the caller writes
    // nothing - when disabled, when the measured cadence does not call for
    // smoothing, or before anything was captured. On true, out[i] is the
    // position for element i of the latest refresh, for the first
    // min(size, kMaxSmoothedMarkers) elements.
    bool Interpolate(uint64_t frame, std::vector<MarkerSample>& out) {
        out.clear();
        if (!IsEnabled()) return false;
        if (!m_Engaged || m_Cur.empty()) {
            ++m_Stats.passthroughFrames;
            m_LastOut.clear();   // the game's own positions are what is on screen
            return false;
        }

        const double elapsed = frame > m_CurFrame ? (double)(frame - m_CurFrame) : 0.0;
        double t = elapsed / (double)m_Stats.period;
        t = std::clamp(t, 0.0, 1.0);

        const size_t n = std::min(m_Cur.size(), kMaxSmoothedMarkers);
        m_Stats.truncated += m_Cur.size() - n;
        out.resize(n);
        for (size_t i = 0; i < n; ++i) {
            const MarkerSample& c = m_Cur[i];
            out[i].key = c.key;
            const ptrdiff_t p = m_PrevIndexForCur[i];
            if (p < 0) {
                // New since the previous refresh: shown where it was reported.
                out[i].x = c.x;
                out[i].y = c.y;
            } else {
                const MarkerSample& from = m_Prev[(size_t)p];
                out[i].x = from.x + (c.x - from.x) * t;
                out[i].y = from.y + (c.y - from.y) * t;
            }
        }
        ++m_Stats.engagedFrames;
        m_Stats.writtenTotal += n;
        m_LastOut = out;
        return true;
    }

private:
    std::atomic<bool> m_Enabled{ false };
    std::vector<MarkerSample> m_Prev;
    std::vector<MarkerSample> m_Cur;
    std::vector<MarkerSample> m_LastOut;
    std::vector<ptrdiff_t> m_PrevIndexForCur;
    bool m_HaveCur = false;
    uint64_t m_CurFrame = 0;
    bool m_Engaged = false;
    Counters m_Stats{};
};

} // namespace ForgePact
