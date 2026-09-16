// Behavioral harness for the minimap smoothing core.
//
// The Python runner (tests/test_minimap_smooth_behavior.py) splices the REAL
// plugin/include/ForgePact/MinimapSmoothManager.hpp in at the marker below.
// No game API is involved: the class takes marker samples in and hands
// positions out, and this file plays the game's part - a container refilled
// every N frames, drawn from every frame.
//
// Frame order modelled, per AGENTS.md "Check a Permission Where It Is Used":
// on a refresh frame the game refills the container first (OnRefresh, from
// the refresh hook), then draws (Interpolate, from the pre-draw hook). What is
// "on screen" for a frame is Interpolate's output when it returns true, and the
// container's own values when it returns false.
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

// PRODUCTION_MINIMAPSMOOTH

using ForgePact::MarkerSample;
using ForgePact::MinimapSmoothManager;

static int failures = 0;
static void checkNum(const std::string& label, double got, double want) {
    const bool ok = std::fabs(got - want) < 1e-9;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}
static void checkInt(const std::string& label, long long got, long long want) {
    const bool ok = (got == want);
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}

// One frame of the modelled game: refresh (if due) then draw. `shown` is what
// the minimap draws this frame; shownX() is NAN for a marker not drawn at all.
struct Frame { bool smoothed; size_t written; std::vector<MarkerSample> shown; };
static Frame runFrame(MinimapSmoothManager& m, uint64_t frame, const std::vector<MarkerSample>* refresh,
                      std::vector<MarkerSample>& container, bool keysAreIndices = false) {
    if (refresh) { container = *refresh; m.OnRefresh(frame, container, keysAreIndices); }
    std::vector<MarkerSample> out;
    Frame f{};
    f.smoothed = m.Interpolate(frame, out);
    f.written = f.smoothed ? out.size() : 0;
    f.shown = container;
    for (size_t i = 0; i < out.size() && i < f.shown.size(); ++i) { f.shown[i].x = out[i].x; f.shown[i].y = out[i].y; }
    return f;
}
static double shownX(const Frame& f, uint64_t key) {
    for (const MarkerSample& s : f.shown) if (s.key == key) return s.x;
    return NAN;
}

int main() {
    // ---- baseline: the unmodified path stays unmodified -------------------

    // off/no_writes - the toggle off (the default) must reproduce vanilla:
    // the game refreshes every 60 frames, and over 120 frames nothing is
    // ever written. Observed failing: with the `if (!IsEnabled()) return
    // false;` line removed, "FAIL off/no_writes got=120 want=0".
    {
        MinimapSmoothManager m;
        std::vector<MarkerSample> container;
        long long writes = 0, smoothed = 0;
        for (uint64_t fr = 0; fr < 120; ++fr) {
            std::vector<MarkerSample> snap{ { 1, (double)fr, 0.0 } };
            Frame f = runFrame(m, fr, (fr % 60) == 0 ? &snap : nullptr, container);
            writes += (long long)f.written;
            smoothed += f.smoothed ? 1 : 0;
        }
        checkInt("off/no_writes", writes + smoothed, 0);
    }

    // fast_cadence/passthrough - performance mode off: the game refreshes
    // every frame, so enabling the mod must still write nothing on any frame.
    // Observed failing with kMinSmoothGapFrames = 1:
    // "FAIL fast_cadence/passthrough got=119 want=0".
    {
        MinimapSmoothManager m;
        m.SetEnabled(true);
        std::vector<MarkerSample> container;
        long long smoothed = 0;
        for (uint64_t fr = 0; fr < 120; ++fr) {
            std::vector<MarkerSample> snap{ { 1, (double)fr, 0.0 } };
            Frame f = runFrame(m, fr, &snap, container);
            smoothed += f.smoothed ? 1 : 0;
        }
        checkInt("fast_cadence/passthrough", smoothed, 0);
    }

    // ---- target: what smoothing must do once engaged -----------------------

    // A steady performance-mode cadence: refreshes on frames 0 and 60, one
    // marker reported at x=0 then x=60. The first refresh measures nothing, so
    // frames 0-59 pass through; the second engages with period 60.
    {
        MinimapSmoothManager m;
        m.SetEnabled(true);
        std::vector<MarkerSample> container;
        const std::vector<MarkerSample> at0{ { 1, 0.0, 0.0 } };
        const std::vector<MarkerSample> at60{ { 1, 60.0, 30.0 } };
        for (uint64_t fr = 0; fr < 60; ++fr) runFrame(m, fr, fr == 0 ? &at0 : nullptr, container);
        runFrame(m, 60, &at60, container);
        for (uint64_t fr = 61; fr < 90; ++fr) runFrame(m, fr, nullptr, container);

        // steady/linear_progress - half a period after the refresh the marker
        // is half way. Observed failing with t divided by a fixed 120 frames
        // instead of the measured period: "FAIL steady/linear_progress got=15 want=30".
        Frame half = runFrame(m, 90, nullptr, container);
        checkNum("steady/linear_progress", shownX(half, 1), 30.0);

        // steady/arrives_at_cur - one full period after the refresh the marker
        // is exactly where the game reported it. Observed failing with the
        // interpolation left at `from.x` (never advanced):
        // "FAIL steady/arrives_at_cur got=0 want=60".
        for (uint64_t fr = 91; fr < 120; ++fr) runFrame(m, fr, nullptr, container);
        Frame full = runFrame(m, 120, nullptr, container);
        checkNum("steady/arrives_at_cur", shownX(full, 1), 60.0);

        // late_refresh/clamped_at_cur - the next refresh is late (90 frames,
        // not 60). The marker waits at the reported position; it is never
        // drawn past it. Observed failing with the clamp on t removed:
        // "FAIL late_refresh/clamped_at_cur got=90 want=60".
        for (uint64_t fr = 121; fr < 150; ++fr) runFrame(m, fr, nullptr, container);
        Frame late = runFrame(m, 150, nullptr, container);
        checkNum("late_refresh/clamped_at_cur", shownX(late, 1), 60.0);
    }

    // no_pop/monotonic_across_refreshes - a marker whose true position is
    // x = frame, refreshed on an irregular cadence (60, 60, then an EARLY one
    // at 30, then 60). Displayed x must never move backwards and never jump
    // more than 3 px in one frame (the fastest honest glide here is ~2 px).
    // Counts offending frames. Observed failing with `m_Prev = m_Cur`
    // (rotating from the old snapshot instead of what was displayed):
    // "FAIL no_pop/monotonic_across_refreshes got=1 want=0" - the single
    // early-refresh frame, where the marker jumps forward to the old target.
    {
        MinimapSmoothManager m;
        m.SetEnabled(true);
        std::vector<MarkerSample> container;
        const uint64_t refreshAt[] = { 0, 60, 120, 150, 210, 270 };
        double lastShown = NAN;
        long long offending = 0;
        for (uint64_t fr = 0; fr < 300; ++fr) {
            bool due = false;
            for (uint64_t r : refreshAt) if (r == fr) due = true;
            std::vector<MarkerSample> snap{ { 1, (double)fr, 0.0 } };
            Frame f = runFrame(m, fr, due ? &snap : nullptr, container);
            const double x = shownX(f, 1);
            if (!std::isnan(lastShown)) {
                const double step = x - lastShown;
                if (step < -1e-9 || step > 3.0) ++offending;
            }
            lastShown = x;
        }
        checkInt("no_pop/monotonic_across_refreshes", offending, 0);
    }

    // new_marker/shown_at_cur - a marker that first appears in a refresh is
    // drawn where the game reported it, not glided in from anywhere.
    // dropped_marker/not_written - a marker absent from the latest refresh
    // gets no position at all. Observed failing with every unmatched marker
    // glided in from the origin (`out[i].x = c.x * t`):
    // "FAIL new_marker/shown_at_cur got=250 want=500"; and with markers found
    // only in the previous snapshot appended to the output:
    // "FAIL dropped_marker/not_written got=1 want=0".
    {
        MinimapSmoothManager m;
        m.SetEnabled(true);
        std::vector<MarkerSample> container;
        const std::vector<MarkerSample> first{ { 1, 0.0, 0.0 }, { 2, 100.0, 100.0 } };
        const std::vector<MarkerSample> second{ { 1, 0.0, 0.0 }, { 2, 100.0, 100.0 } };
        const std::vector<MarkerSample> third{ { 1, 60.0, 0.0 }, { 7, 500.0, 500.0 } };
        runFrame(m, 0, &first, container);
        runFrame(m, 60, &second, container);
        runFrame(m, 120, &third, container);
        std::vector<MarkerSample> out;
        m.Interpolate(150, out);
        double key7 = NAN;
        long long key2 = 0;
        for (const MarkerSample& s : out) { if (s.key == 7) key7 = s.x; if (s.key == 2) ++key2; }
        checkNum("new_marker/shown_at_cur", key7, 500.0);
        checkInt("dropped_marker/not_written", key2, 0);
    }

    // cap/truncated_counted - 200 markers: exactly kMaxSmoothedMarkers get a
    // position, and the other 72 are counted rather than dropped silently.
    // got/want encode both numbers as written*1000 + truncated. Observed
    // failing with the cap removed (`n = m_Cur.size()`):
    // "FAIL cap/truncated_counted got=200000 want=128072".
    {
        MinimapSmoothManager m;
        m.SetEnabled(true);
        std::vector<MarkerSample> container;
        std::vector<MarkerSample> many;
        for (uint64_t k = 0; k < 200; ++k) many.push_back({ 1000 + k, (double)k, 0.0 });
        runFrame(m, 0, &many, container);
        runFrame(m, 60, &many, container);
        std::vector<MarkerSample> out;
        const MinimapSmoothManager::Counters before = m.Stats();
        m.Interpolate(61, out);
        const long long truncated = (long long)(m.Stats().truncated - before.truncated);
        checkInt("cap/truncated_counted", (long long)out.size() * 1000 + truncated,
                 (long long)MinimapSmoothManager::kMaxSmoothedMarkers * 1000 + 72);
    }

    // index_keys_count_change/all_new - when the only identity is the index,
    // a refresh with a different count cannot pair markers, so every marker
    // is shown where reported instead of gliding between unrelated ones.
    // Counts how many of the first three are at their reported x. Observed
    // failing with `keysAreIndices` ignored (index 0..2 paired with the old
    // 0..2 and glided half way): "FAIL index_keys_count_change/all_new got=0 want=3".
    {
        MinimapSmoothManager m;
        m.SetEnabled(true);
        std::vector<MarkerSample> container;
        const std::vector<MarkerSample> three{ { 0, 0.0, 0.0 }, { 1, 10.0, 0.0 }, { 2, 20.0, 0.0 } };
        const std::vector<MarkerSample> four{ { 0, 100.0, 0.0 }, { 1, 110.0, 0.0 }, { 2, 120.0, 0.0 }, { 3, 130.0, 0.0 } };
        runFrame(m, 0, &three, container, true);
        runFrame(m, 60, &three, container, true);
        runFrame(m, 120, &four, container, true);
        std::vector<MarkerSample> out;
        m.Interpolate(150, out);
        long long atReported = 0;
        for (size_t i = 0; i < out.size() && i < 3; ++i) if (std::fabs(out[i].x - four[i].x) < 1e-9) ++atReported;
        checkInt("index_keys_count_change/all_new", atReported, 3);
    }

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
