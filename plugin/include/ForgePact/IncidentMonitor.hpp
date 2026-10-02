#pragma once
// DropManager.hpp's scope line looks for this (a harness splices that header
// without its includes).
#define FORGEPACT_INCIDENT_MONITOR_HPP 1

// ForgePact::Incident - crash, freeze and FPS-drop detection with a per-mod
// impact report (issue #76).
//
// Three questions a player's bug report has to answer: was it our code, which
// mod was active or busy, and how much frame time our hooks were taking. So:
//
//   * The game's frame thread only takes QueryPerformanceCounter readings and
//     does single-writer atomic stores: IncidentScope around each mod's hook
//     body or tick adds its time to that mod's per-frame counter and marks it
//     as the code the frame thread is in; OnFrame, once a frame, moves the
//     counters into a ring of the last kRingFrames frames.
//   * A monitor thread (ModuleMain.cpp's adapter, an ExitSafeThread on the
//     heap-held Monitor below) wakes every kWakeMs, copies the new frames out
//     of the ring and hands them to the Detector, which decides hitch,
//     sustained slowdown, freeze or nothing from the frames and a few plain
//     inputs (the time, focus, the last room change, the in-hook tag). The
//     monitor thread never touches the game's runtime: it reads only these
//     counters and writes files.
//   * A crash runs no code of ours, so it is found afterwards: at the next
//     load, the previous session's part of out.txt either ends with the line
//     g_ShutdownMarker's destructor writes during a normal ExitProcess, or it
//     does not.
//
// Game-independent: Win32 and the standard library only, no runtime
// interface. tests/incident_monitor_harness.cpp drives the same classes on a
// simulated clock, and tests/incident_shutdown_probe.cpp loads the marker in
// a DLL of its own. Avoids the two-argument min/max of the standard library:
// ModuleMain.cpp includes <windows.h> without NOMINMAX.

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ExitSafeThread.hpp"

namespace ForgePact::Incident {

// ---- thresholds (the plan's D5; no verb changes them) ----------------------

inline constexpr double kHitchMs = 250.0;          // one frame this long is a hitch
inline constexpr double kSustainedMult = 2.5;      // frames this many times the baseline...
inline constexpr double kSustainedMs = 2000.0;     // ...for this long are a sustained slowdown
inline constexpr double kFreezeMs = 3000.0;        // no frame for this long is a freeze
inline constexpr double kRoomGraceMs = 5000.0;     // no PERF verdict this long after a room change
inline constexpr size_t kRingFrames = 600;         // frames the ring keeps; also the baseline's length
inline constexpr double kEpisodeGapMs = 30000.0;   // at most one PERF episode this often
inline constexpr unsigned kMaxEpisodes = 50;       // episodes a session
inline constexpr double kBundleGapMs = 300000.0;   // at most one PERF report bundle this often
inline constexpr unsigned kMaxBundles = 10;        // report bundles a session
inline constexpr unsigned kKeepReports = 10;       // report directories kept on disk

// ---- how the monitor applies them ------------------------------------------

inline constexpr unsigned kWakeMs = 250;           // the monitor thread's period
// A frame is judged this long after it ended. The frame thread samples the
// room once a second, so by then a room change that caused the frame is known.
inline constexpr double kJudgeDelayMs = 1500.0;
inline constexpr double kRoomLeadMs = 1500.0;      // the grace also covers this much before the change was seen
inline constexpr double kFocusSlackMs = 300.0;     // a focus sample covers frames this close to it
inline constexpr size_t kMinBaselineFrames = 60;   // fewer frames than this make no baseline for a slowdown
inline constexpr size_t kHistoryFrames = 4 * kRingFrames;
inline constexpr unsigned kSampleEvery = 16;       // IncidentSampledScope times one call in this many
inline constexpr size_t kTailLines = 500;          // out-tail.txt's length
// The next-load crash check waits this long, so the panel can write exit.json
// for the session that just ended (it polls the game every 5 s).
inline constexpr double kCrashCheckDelayMs = 10000.0;

inline constexpr char kSessionBanner[] = "==== BloodPact plugin loaded ====";
inline constexpr char kCleanShutdownLine[] = "==== clean shutdown ====";
inline constexpr char kMonitorRunningLine[] = "incident: monitor running";

// ---- the mods a report names -------------------------------------------------

enum class Mod : uint8_t {
    none,          // no ForgePact code on the frame thread
    frame,         // FrameCallback's whole body, the mods it calls included
    density,       // DensityCopiesTick, and DoMultiCreate (sampled)
    mapreveal,     // MapRevealManager::OnFrame and the pack markers
    drops,         // DropRelic and the dropmult hooks
    autoprospect,
    hudlabels,     // the DrawHudBuffs hook
    farsleep,
    gems,
    miner,
    stashmoveall,
    ipc,           // PollCommands: reading and running the panel's commands
    Count
};
inline constexpr size_t kModCount = static_cast<size_t>(Mod::Count);

inline const char* ModName(Mod m) noexcept
{
    static const char* const names[kModCount] = {
        "none", "frame", "density", "mapreveal", "drops", "autoprospect",
        "hudlabels", "farsleep", "gems", "miner", "stashmoveall", "ipc",
    };
    const size_t i = static_cast<size_t>(m);
    return i < kModCount ? names[i] : "none";
}

// ---- clock ---------------------------------------------------------------------

inline int64_t Qpc() noexcept
{
    LARGE_INTEGER v;
    QueryPerformanceCounter(&v);
    return v.QuadPart;
}

inline int64_t QpcFrequency() noexcept
{
    static const int64_t frequency = [] {
        LARGE_INTEGER v;
        QueryPerformanceFrequency(&v);
        return v.QuadPart > 0 ? v.QuadPart : 1;
    }();
    return frequency;
}

inline double QpcToMs(int64_t ticks) noexcept
{
    return static_cast<double>(ticks) * 1000.0 / static_cast<double>(QpcFrequency());
}

// ---- per-frame accounting: the frame thread writes, the monitor reads ---------

// One frame as the monitor sees it.
struct FrameSample {
    double endMs = 0.0;               // when the frame ended, on the QPC clock
    double frameMs = 0.0;             // since the previous frame ended
    float modMs[kModCount] = {};      // each mod's time inside the frame
};

class Accounting {
public:
    Accounting() = default;
    Accounting(const Accounting&) = delete;
    Accounting& operator=(const Accounting&) = delete;

    // ---- frame thread. Every scope runs there: the game runs its scripts and
    // draws on that one thread, so each counter has a single writer and needs
    // no read-modify-write instruction.
    void Add(Mod m, int64_t ticks) noexcept
    {
        auto& a = m_Mod[static_cast<size_t>(m)];
        a.store(a.load(std::memory_order_relaxed) + ticks, std::memory_order_relaxed);
    }

    Mod Enter(Mod m) noexcept
    {
        const Mod previous = m_InHook.load(std::memory_order_relaxed);
        m_InHook.store(m, std::memory_order_relaxed);
        return previous;
    }

    void Leave(Mod previous) noexcept { m_InHook.store(previous, std::memory_order_relaxed); }

    // True for one call in kSampleEvery of this mod's sampled scopes.
    bool TakeSample(Mod m) noexcept
    {
        auto& c = m_Calls[static_cast<size_t>(m)];
        const uint32_t n = c.load(std::memory_order_relaxed);
        c.store(n + 1, std::memory_order_relaxed);
        return n % kSampleEvery == 0;
    }

    // The frame boundary: the time since the last one, and every mod's time
    // inside it, go into the ring.
    void OnFrame(int64_t nowQpc) noexcept
    {
        const int64_t last = m_LastFrameQpc.load(std::memory_order_relaxed);
        m_LastFrameQpc.store(nowQpc, std::memory_order_release);
        if (last == 0) {
            for (auto& a : m_Mod) a.store(0, std::memory_order_relaxed);
            return;
        }
        const uint64_t written = m_Written.load(std::memory_order_relaxed);
        Slot& slot = m_Ring[written % kRingFrames];
        slot.endQpc.store(nowQpc, std::memory_order_relaxed);
        slot.ticks.store(nowQpc - last, std::memory_order_relaxed);
        for (size_t i = 0; i < kModCount; ++i) {
            slot.mod[i].store(m_Mod[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
            m_Mod[i].store(0, std::memory_order_relaxed);
        }
        m_Written.store(written + 1, std::memory_order_release);
    }

    // ---- any thread.
    Mod InHook() const noexcept { return m_InHook.load(std::memory_order_relaxed); }
    int64_t LastFrameQpc() const noexcept { return m_LastFrameQpc.load(std::memory_order_acquire); }
    uint64_t Frames() const noexcept { return m_Written.load(std::memory_order_acquire); }

    // ---- monitor thread: the frames written since `cursor`, oldest first.
    // Frames the ring overwrote before they were read are added to `lost`.
    void CopySince(uint64_t& cursor, std::vector<FrameSample>& out, uint64_t& lost) const
    {
        const uint64_t written = m_Written.load(std::memory_order_acquire);
        uint64_t first = cursor;
        if (written > kRingFrames && first < written - kRingFrames + 1) first = written - kRingFrames + 1;
        const size_t base = out.size();
        for (uint64_t i = first; i < written; ++i) {
            const Slot& slot = m_Ring[i % kRingFrames];
            FrameSample f;
            f.endMs = QpcToMs(slot.endQpc.load(std::memory_order_relaxed));
            f.frameMs = QpcToMs(slot.ticks.load(std::memory_order_relaxed));
            for (size_t m = 0; m < kModCount; ++m)
                f.modMs[m] = static_cast<float>(QpcToMs(slot.mod[m].load(std::memory_order_relaxed)));
            out.push_back(f);
        }
        // A slot the frame thread started to rewrite while it was copied is
        // dropped: index i is safe while i + kRingFrames is still unwritten.
        std::atomic_thread_fence(std::memory_order_acquire);
        const uint64_t after = m_Written.load(std::memory_order_relaxed);
        uint64_t keepFrom = first;
        if (after >= kRingFrames && keepFrom < after - kRingFrames + 1) keepFrom = after - kRingFrames + 1;
        if (keepFrom > first) {
            const size_t drop = static_cast<size_t>(keepFrom - first) < out.size() - base
                ? static_cast<size_t>(keepFrom - first) : out.size() - base;
            out.erase(out.begin() + static_cast<std::ptrdiff_t>(base),
                      out.begin() + static_cast<std::ptrdiff_t>(base + drop));
        }
        const uint64_t start = keepFrom > first ? keepFrom : first;
        if (start > cursor) lost += start - cursor;
        cursor = written;
    }

private:
    struct Slot {
        std::atomic<int64_t> endQpc{ 0 };
        std::atomic<int64_t> ticks{ 0 };
        std::atomic<int64_t> mod[kModCount]{};
    };

    std::atomic<int64_t> m_Mod[kModCount]{};
    std::atomic<uint32_t> m_Calls[kModCount]{};
    std::atomic<Mod> m_InHook{ Mod::none };
    std::atomic<int64_t> m_LastFrameQpc{ 0 };
    std::atomic<uint64_t> m_Written{ 0 };
    Slot m_Ring[kRingFrames]{};
};

// Constant-initialised and trivially destructible: nothing runs for it at load
// or at exit, so a hook that fires early or late still finds it whole.
inline constinit Accounting g_Accounting;
static_assert(std::is_trivially_destructible_v<Accounting>, "the accounting must have nothing to run at exit");

// One mod's time on the frame thread. Same-mod nesting (a dropmult hook the
// game calls from another) is counted once; `frame` contains the per-frame
// mods it calls, so the table reads it as the total.
class IncidentScope {
public:
    explicit IncidentScope(Mod m) noexcept
        : m_Mod(m), m_Previous(g_Accounting.Enter(m)), m_Start(m_Previous == m ? 0 : Qpc()) {}
    ~IncidentScope()
    {
        if (m_Start) g_Accounting.Add(m_Mod, Qpc() - m_Start);
        g_Accounting.Leave(m_Previous);
    }
    IncidentScope(const IncidentScope&) = delete;
    IncidentScope& operator=(const IncidentScope&) = delete;

private:
    Mod m_Mod;
    Mod m_Previous;
    int64_t m_Start;
};

// The same for a body called far more often than once a frame (DoMultiCreate):
// one call in kSampleEvery is timed and counted kSampleEvery times. The in-hook
// tag is set on every call.
class IncidentSampledScope {
public:
    explicit IncidentSampledScope(Mod m) noexcept
        : m_Mod(m), m_Previous(g_Accounting.Enter(m)),
          m_Start(m_Previous != m && g_Accounting.TakeSample(m) ? Qpc() : 0) {}
    ~IncidentSampledScope()
    {
        if (m_Start) g_Accounting.Add(m_Mod, (Qpc() - m_Start) * static_cast<int64_t>(kSampleEvery));
        g_Accounting.Leave(m_Previous);
    }
    IncidentSampledScope(const IncidentSampledScope&) = delete;
    IncidentSampledScope& operator=(const IncidentSampledScope&) = delete;

private:
    Mod m_Mod;
    Mod m_Previous;
    int64_t m_Start;
};

// ---- episodes ------------------------------------------------------------------

enum class Kind : uint8_t { none, hitch, sustained, freeze, crash };

inline const char* KindName(Kind k) noexcept
{
    switch (k) {
    case Kind::hitch: return "hitch";
    case Kind::sustained: return "sustained";
    case Kind::freeze: return "freeze";
    case Kind::crash: return "crash";
    default: return "none";
    }
}

// The report directory's kind: both PERF episodes are `perf`.
inline const char* BundleKindName(Kind k) noexcept
{
    switch (k) {
    case Kind::hitch:
    case Kind::sustained: return "perf";
    case Kind::freeze: return "freeze";
    case Kind::crash: return "crash";
    default: return "none";
    }
}

struct ModCost {
    Mod mod = Mod::none;
    double avgMs = 0.0;     // per frame
    double worstMs = 0.0;   // in one frame
};

struct Episode {
    Kind kind = Kind::none;
    double atMs = 0.0;        // the frame's end (PERF) or the moment it was seen (freeze)
    double worstMs = 0.0;     // the slowest frame; for a freeze, the time without one
    double baselineMs = 0.0;  // median frame before it
    double duringMs = 0.0;    // median frame during it
    double seconds = 0.0;     // how long it lasted when it was reported
    Mod inHook = Mod::none;   // freeze: the innermost ForgePact code the frame thread was in
    std::vector<ModCost> cost;  // per mod over the episode's frames, costliest first
};

// What the monitor sees at one wake, as plain values.
struct Inputs {
    double nowMs = 0.0;
    double lastFrameMs = -1.0;    // end of the newest frame; below zero before the first
    bool armed = false;           // the game is past its start-up (ForgePact's setup ran)
    bool focused = true;          // the game's window is in front
    bool minimized = false;
    bool windowAlive = true;      // the game's window exists and is shown
    double roomChangeMs = -1.0;   // the newest room change the frame thread saw; below zero for none
    Mod inHook = Mod::none;
};

namespace detail {

inline double Median(std::vector<double>& v)
{
    if (v.empty()) return 0.0;
    const auto mid = v.begin() + static_cast<std::ptrdiff_t>(v.size() / 2);
    std::nth_element(v.begin(), mid, v.end());
    return *mid;
}

inline double Quantile(std::vector<double>& v, double q)
{
    if (v.empty()) return 0.0;
    const auto at = v.begin() + static_cast<std::ptrdiff_t>(q * static_cast<double>(v.size() - 1));
    std::nth_element(v.begin(), at, v.end());
    return *at;
}

inline void SortCosts(std::vector<ModCost>& rows)
{
    std::sort(rows.begin(), rows.end(), [](const ModCost& a, const ModCost& b) {
        if (a.avgMs != b.avgMs) return a.avgMs > b.avgMs;
        return a.mod < b.mod;
    });
}

template <typename It>
std::vector<ModCost> CostOf(It begin, It end)
{
    double sum[kModCount] = {};
    double worst[kModCount] = {};
    size_t frames = 0;
    for (It it = begin; it != end; ++it, ++frames) {
        for (size_t m = 0; m < kModCount; ++m) {
            sum[m] += it->modMs[m];
            if (it->modMs[m] > worst[m]) worst[m] = it->modMs[m];
        }
    }
    std::vector<ModCost> rows;
    if (!frames) return rows;
    for (size_t m = 1; m < kModCount; ++m)
        if (worst[m] > 0.0) rows.push_back({ static_cast<Mod>(m), sum[m] / static_cast<double>(frames), worst[m] });
    SortCosts(rows);
    return rows;
}

} // namespace detail

// Decides, wake by wake, whether the frames add up to an episode. Monitor
// thread only. Analyze reports at most one episode a call; call it again
// until it answers Kind::none.
class Detector {
public:
    void Feed(const FrameSample* frames, size_t n)
    {
        for (size_t i = 0; i < n; ++i) {
            m_History.push_back(frames[i]);
            ++m_Frames;
            if (frames[i].frameMs > m_WorstMs) m_WorstMs = frames[i].frameMs;
        }
        while (m_History.size() > kHistoryFrames) {
            m_History.pop_front();
            if (m_Judged) --m_Judged;
        }
    }

    Episode Analyze(const Inputs& in)
    {
        Note(in);
        if (!in.armed) {
            // Start-up's slow frames are the game loading, not a verdict.
            m_Judged = m_History.size();
            m_InFreeze = false;
            return {};
        }

        // Freeze: no frame for kFreezeMs, judged now, while it lasts.
        if (in.lastFrameMs >= 0.0) {
            if (m_InFreeze && in.lastFrameMs > m_FreezeFromMs) {
                m_InFreeze = false;
                m_FreezeEnded = true;
                m_FreezeEndedMs = in.lastFrameMs - m_FreezeFromMs;
            }
            const double gap = in.nowMs - in.lastFrameMs;
            // A minimized game may stop drawing; the wait counts from the
            // later of its last frame and the last wake that saw it minimized.
            const double since = in.lastFrameMs > m_LastMinimizedMs ? in.lastFrameMs : m_LastMinimizedMs;
            if (!m_InFreeze && in.nowMs - since >= kFreezeMs && !in.minimized && in.windowAlive) {
                m_InFreeze = true;
                m_FreezeFromMs = in.lastFrameMs;
                m_FreezeStarts.push_back(in.lastFrameMs);
                if (m_FreezeStarts.size() > 8) m_FreezeStarts.pop_front();
                if (Allowed(in.nowMs, true)) {
                    Episode e;
                    e.kind = Kind::freeze;
                    e.atMs = in.nowMs;
                    e.worstMs = gap;
                    e.seconds = gap / 1000.0;
                    e.inHook = in.inHook;
                    e.baselineMs = BaselineBefore(m_History.size(), 1);
                    Counted(in.nowMs);
                    return e;
                }
                ++m_Suppressed;
            }
        }

        // Hitches: each frame once, kJudgeDelayMs after it ended.
        const double horizon = in.nowMs - kJudgeDelayMs;
        while (m_Judged < m_History.size() && m_History[m_Judged].endMs <= horizon) {
            const size_t i = m_Judged++;
            const FrameSample& f = m_History[i];
            if (f.frameMs < kHitchMs || IsFreezeFrame(f)) continue;
            if (IsQuiet(f)) { ++m_Quiet; continue; }
            if (!Allowed(f.endMs, false)) { ++m_Suppressed; continue; }
            Episode e;
            e.kind = Kind::hitch;
            e.atMs = f.endMs;
            e.worstMs = f.frameMs;
            e.duringMs = f.frameMs;
            e.seconds = f.frameMs / 1000.0;
            e.baselineMs = BaselineBefore(i, 1);
            e.cost = detail::CostOf(m_History.begin() + static_cast<std::ptrdiff_t>(i),
                                    m_History.begin() + static_cast<std::ptrdiff_t>(i + 1));
            Counted(f.endMs);
            return e;
        }

        // Sustained: the judged frames of the last kSustainedMs, most of them
        // kSustainedMult times the frames before them.
        const size_t end = m_Judged;
        size_t begin = end;
        double span = 0.0;
        while (begin > 0 && span < kSustainedMs) span += m_History[--begin].frameMs;
        bool slow = false;
        double baseline = 0.0;
        std::vector<double> during;
        if (span >= kSustainedMs) {
            bool quiet = false;
            for (size_t i = begin; i < end && !quiet; ++i) {
                quiet = IsQuiet(m_History[i]) || IsFreezeFrame(m_History[i]);
                during.push_back(m_History[i].frameMs);
            }
            baseline = quiet ? 0.0 : BaselineBefore(begin, kMinBaselineFrames);
            if (baseline > 0.0) {
                std::vector<double> lower = during;
                // Three frames in four, not the mean: one long frame is a hitch.
                slow = detail::Quantile(lower, 0.25) >= kSustainedMult * baseline;
            }
        }
        if (!slow) { m_SustainedActive = false; return {}; }
        if (m_SustainedActive) return {};
        m_SustainedActive = true;
        if (!Allowed(horizon, false)) { ++m_Suppressed; return {}; }
        Episode e;
        e.kind = Kind::sustained;
        e.atMs = horizon;
        e.baselineMs = baseline;
        for (const double d : during) if (d > e.worstMs) e.worstMs = d;
        e.duringMs = detail::Median(during);
        e.seconds = span / 1000.0;
        e.cost = detail::CostOf(m_History.begin() + static_cast<std::ptrdiff_t>(begin),
                                m_History.begin() + static_cast<std::ptrdiff_t>(end));
        Counted(horizon);
        return e;
    }

    // The end of a reported (or held back) freeze, once: how long the frame
    // thread was gone.
    bool TakeFreezeEnded(double& ms) noexcept
    {
        if (!m_FreezeEnded) return false;
        m_FreezeEnded = false;
        ms = m_FreezeEndedMs;
        return true;
    }

    uint64_t Frames() const noexcept { return m_Frames; }
    double WorstMs() const noexcept { return m_WorstMs; }
    unsigned Episodes() const noexcept { return m_Episodes; }
    unsigned Suppressed() const noexcept { return m_Suppressed; }  // held back by the rate limits
    unsigned Quiet() const noexcept { return m_Quiet; }            // slow, but near a room change or unfocused
    bool InFreeze() const noexcept { return m_InFreeze; }
    double BaselineMs() const { return BaselineBefore(m_History.size(), 1); }
    bool GraceActive(double nowMs) const noexcept
    {
        for (const double r : m_RoomChanges)
            if (nowMs >= r && nowMs - r < kRoomGraceMs) return true;
        return false;
    }

private:
    void Note(const Inputs& in)
    {
        if (!in.focused || in.minimized) m_Unfocused.push_back(in.nowMs);
        if (in.minimized) m_LastMinimizedMs = in.nowMs;
        while (!m_Unfocused.empty() && in.nowMs - m_Unfocused.front() > 60000.0) m_Unfocused.pop_front();
        if (in.roomChangeMs >= 0.0 && (m_RoomChanges.empty() || m_RoomChanges.back() != in.roomChangeMs)) {
            m_RoomChanges.push_back(in.roomChangeMs);
            if (m_RoomChanges.size() > 16) m_RoomChanges.pop_front();
        }
    }

    bool InGrace(const FrameSample& f) const noexcept
    {
        const double start = f.endMs - f.frameMs;
        for (const double r : m_RoomChanges)
            if (f.endMs >= r - kRoomLeadMs && start <= r + kRoomGraceMs) return true;
        return false;
    }

    bool Unfocused(const FrameSample& f) const noexcept
    {
        const double start = f.endMs - f.frameMs;
        for (const double t : m_Unfocused)
            if (t >= start - kFocusSlackMs && t <= f.endMs + kFocusSlackMs) return true;
        return false;
    }

    bool IsQuiet(const FrameSample& f) const noexcept { return InGrace(f) || Unfocused(f); }

    // The frame that ended a freeze already has its report.
    bool IsFreezeFrame(const FrameSample& f) const noexcept
    {
        const double start = f.endMs - f.frameMs;
        for (const double s : m_FreezeStarts)
            if (f.endMs > s && start <= s + 1.0 && start >= s - 1.0) return true;
        return false;
    }

    // Median of up to kRingFrames ordinary frames before `index`; zero when
    // fewer than `minimum` qualify.
    double BaselineBefore(size_t index, size_t minimum) const
    {
        std::vector<double> v;
        v.reserve(kRingFrames);
        for (size_t i = index; i > 0 && v.size() < kRingFrames;) {
            const FrameSample& f = m_History[--i];
            if (IsQuiet(f) || IsFreezeFrame(f)) continue;
            v.push_back(f.frameMs);
        }
        if (v.size() < minimum || v.empty()) return 0.0;
        return detail::Median(v);
    }

    bool Allowed(double atMs, bool freeze) const noexcept
    {
        if (m_Episodes >= kMaxEpisodes) return false;
        // A freeze is not held back by the PERF gap: it is the one a player
        // most needs reported.
        if (freeze) return true;
        return m_LastEpisodeMs < 0.0 || atMs - m_LastEpisodeMs >= kEpisodeGapMs;
    }

    void Counted(double atMs) noexcept
    {
        ++m_Episodes;
        if (atMs > m_LastEpisodeMs) m_LastEpisodeMs = atMs;
    }

    std::deque<FrameSample> m_History;
    size_t m_Judged = 0;
    uint64_t m_Frames = 0;
    double m_WorstMs = 0.0;
    std::deque<double> m_Unfocused;
    std::deque<double> m_RoomChanges;
    std::deque<double> m_FreezeStarts;
    double m_LastMinimizedMs = -1.0;
    bool m_InFreeze = false;
    double m_FreezeFromMs = 0.0;
    bool m_FreezeEnded = false;
    double m_FreezeEndedMs = 0.0;
    bool m_SustainedActive = false;
    double m_LastEpisodeMs = -1.0;
    unsigned m_Episodes = 0;
    unsigned m_Suppressed = 0;
    unsigned m_Quiet = 0;
};

// Per-mod cost over the last minute, in one-second bins. Monitor thread only.
class MinuteTable {
public:
    void Feed(const FrameSample& f) noexcept
    {
        const int64_t second = static_cast<int64_t>(f.endMs / 1000.0);
        Bin& b = m_Bins[static_cast<size_t>(second) % m_Bins.size()];
        if (b.second != second) {
            b = Bin{};
            b.second = second;
        }
        ++b.frames;
        for (size_t m = 0; m < kModCount; ++m) {
            b.sum[m] += f.modMs[m];
            if (f.modMs[m] > b.worst[m]) b.worst[m] = f.modMs[m];
        }
    }

    std::vector<ModCost> Rows(double nowMs) const
    {
        const int64_t now = static_cast<int64_t>(nowMs / 1000.0);
        double sum[kModCount] = {};
        double worst[kModCount] = {};
        uint64_t frames = 0;
        for (const Bin& b : m_Bins) {
            if (b.second < 0 || b.second > now || now - b.second >= 60) continue;
            frames += b.frames;
            for (size_t m = 0; m < kModCount; ++m) {
                sum[m] += b.sum[m];
                if (b.worst[m] > worst[m]) worst[m] = b.worst[m];
            }
        }
        std::vector<ModCost> rows;
        if (!frames) return rows;
        for (size_t m = 1; m < kModCount; ++m)
            if (worst[m] > 0.0) rows.push_back({ static_cast<Mod>(m), sum[m] / static_cast<double>(frames), worst[m] });
        detail::SortCosts(rows);
        return rows;
    }

private:
    struct Bin {
        int64_t second = -1;
        uint32_t frames = 0;
        double sum[kModCount] = {};
        double worst[kModCount] = {};
    };
    std::array<Bin, 64> m_Bins{};
};

// How many report bundles a session writes. A PERF bundle at most once per
// kBundleGapMs; a freeze or a crash is not held back by the gap. All count
// toward kMaxBundles. Monitor thread only.
class BundleLimiter {
public:
    bool Allow(Kind k, double nowMs) const noexcept
    {
        if (m_Written >= kMaxBundles) return false;
        if ((k == Kind::hitch || k == Kind::sustained) && m_LastPerfMs >= 0.0 && nowMs - m_LastPerfMs < kBundleGapMs)
            return false;
        return true;
    }
    void Wrote(Kind k, double nowMs) noexcept
    {
        ++m_Written;
        if (k == Kind::hitch || k == Kind::sustained) m_LastPerfMs = nowMs;
    }
    unsigned Written() const noexcept { return m_Written; }

private:
    unsigned m_Written = 0;
    double m_LastPerfMs = -1.0;
};

// ---- text: pure functions over strings and structs, no file IO -------------

inline std::string Fixed(double v, int decimals)
{
    if (!std::isfinite(v)) v = 0.0;
    char b[64];
    std::snprintf(b, sizeof(b), "%.*f", decimals, v);
    return b;
}

inline std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (const char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if (c < 0x20 || c >= 0x80) {
            // Non-ASCII bytes are the ANSI code page's; as \u00XX the file
            // stays valid JSON whatever they were.
            char b[8];
            std::snprintf(b, sizeof(b), "\\u%04x", static_cast<unsigned>(c));
            out += b;
        } else out += ch;
    }
    return out;
}

inline std::string JsonString(const std::string& s) { return "\"" + JsonEscape(s) + "\""; }

// A top-level-or-nested `"key": "value"` string field. Enough for the panel's
// exit.json and panel.json, which this module reads and never writes.
inline bool JsonStringField(const std::string& json, const std::string& key, std::string& value)
{
    const std::string quoted = "\"" + key + "\"";
    for (size_t at = json.find(quoted); at != std::string::npos; at = json.find(quoted, at + 1)) {
        size_t i = at + quoted.size();
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (i >= json.size() || json[i] != ':') continue;
        ++i;
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (i >= json.size() || json[i] != '"') return false;
        std::string out;
        for (++i; i < json.size(); ++i) {
            if (json[i] == '\\' && i + 1 < json.size()) { out += json[++i]; continue; }
            if (json[i] == '"') { value = out; return true; }
            out += json[i];
        }
        return false;
    }
    return false;
}

inline bool JsonNumberField(const std::string& json, const std::string& key, double& value)
{
    const std::string quoted = "\"" + key + "\"";
    for (size_t at = json.find(quoted); at != std::string::npos; at = json.find(quoted, at + 1)) {
        size_t i = at + quoted.size();
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        if (i >= json.size() || json[i] != ':') continue;
        ++i;
        while (i < json.size() && std::isspace(static_cast<unsigned char>(json[i]))) ++i;
        const char* start = json.c_str() + i;
        char* stop = nullptr;
        const double v = std::strtod(start, &stop);
        if (stop == start) return false;
        value = v;
        return true;
    }
    return false;
}

// Every path under the player's profile directory, in the three spellings a
// report carries (Windows, forward slashes, JSON-escaped), becomes the literal
// %USERPROFILE%, so no account name leaves the machine in a report. Case-
// insensitive, and only where the profile path ends: "C:\Users\Jane Doe2" is
// another directory and stays as it is.
inline std::string ScrubProfile(const std::string& text, const std::string& profile)
{
    if (profile.size() < 4) return text;
    std::string backslash, forward, escaped;
    for (const char c : profile) {
        const bool sep = c == '\\' || c == '/';
        backslash += sep ? '\\' : c;
        forward += sep ? '/' : c;
        escaped += sep ? std::string("\\\\") : std::string(1, c);
    }
    while (!backslash.empty() && backslash.back() == '\\') {
        backslash.pop_back();
        forward.pop_back();
        escaped.erase(escaped.size() - 2);
    }
    const std::string forms[] = { escaped, backslash, forward };
    auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    auto matchesAt = [&](const std::string& form, size_t at) {
        if (at + form.size() > text.size()) return false;
        for (size_t k = 0; k < form.size(); ++k)
            if (lower(text[at + k]) != lower(form[k])) return false;
        const size_t next = at + form.size();
        if (next == text.size()) return true;
        const auto c = static_cast<unsigned char>(text[next]);
        return !(std::isalnum(c) || c == '_' || c == '-' || c == '.' || c >= 0x80);
    };
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        bool replaced = false;
        for (const std::string& form : forms) {
            if (matchesAt(form, i)) {
                out += "%USERPROFILE%";
                i += form.size();
                replaced = true;
                break;
            }
        }
        if (!replaced) out += text[i++];
    }
    return out;
}

// The last `count` lines of `text`.
inline std::string TailLines(const std::string& text, size_t count)
{
    if (!count || text.empty()) return {};
    size_t at = text.size();
    if (text.back() == '\n') --at;
    size_t lines = 0;
    while (at > 0) {
        if (text[at - 1] == '\n' && ++lines == count) break;
        --at;
    }
    return text.substr(at);
}

// Where each session's banner line starts.
inline std::vector<size_t> BannerStarts(const std::string& text)
{
    std::vector<size_t> starts;
    for (size_t at = text.find(kSessionBanner); at != std::string::npos; at = text.find(kSessionBanner, at + 1))
        if (at == 0 || text[at - 1] == '\n') starts.push_back(at);
    return starts;
}

// The session before this one: between out.txt's last two banners, or, when
// out.txt holds only this session's (it was rotated at this load), the tail of
// out.prev.txt from its last banner. False on a first run.
inline bool PreviousSession(const std::string& outText, const std::string& prevText, std::string& session)
{
    const std::vector<size_t> here = BannerStarts(outText);
    if (here.size() >= 2) {
        session = outText.substr(here[here.size() - 2], here.back() - here[here.size() - 2]);
        return true;
    }
    if (here.size() == 1) {
        const std::vector<size_t> before = BannerStarts(prevText);
        if (before.empty()) return false;
        session = prevText.substr(before.back());
        return true;
    }
    return false;
}

inline bool SessionEndedCleanly(const std::string& session)
{
    return session.find(kCleanShutdownLine) != std::string::npos;
}

// Only a session that ran the monitor can be judged: one from an older plugin
// never writes the clean-shutdown line at all.
inline bool SessionRanMonitor(const std::string& session)
{
    return session.find(kMonitorRunningLine) != std::string::npos;
}

inline std::string FirstLine(const std::string& text)
{
    std::string line = text.substr(0, text.find('\n'));
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    return line;
}

inline std::string UtcText(const SYSTEMTIME& t)
{
    char b[32];
    std::snprintf(b, sizeof(b), "%04u-%02u-%02uT%02u:%02u:%02uZ", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

// reports\<yyyymmdd-HHMMSS>_<perf|freeze|crash>, in UTC.
inline std::string BundleDirName(const SYSTEMTIME& t, Kind k)
{
    char b[48];
    std::snprintf(b, sizeof(b), "%04u%02u%02u-%02u%02u%02u_%s", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                  t.wSecond, BundleKindName(k));
    return b;
}

inline bool IsReportDirName(const std::string& name)
{
    if (name.size() < 17) return false;
    for (size_t i = 0; i < 15; ++i) {
        if (i == 8) { if (name[i] != '-') return false; continue; }
        if (!std::isdigit(static_cast<unsigned char>(name[i]))) return false;
    }
    if (name[15] != '_') return false;
    const std::string kind = name.substr(16);
    return kind == "perf" || kind == "freeze" || kind == "crash";
}

// The report directories to delete so that `keep` remain, oldest first. Only
// names this module writes are counted or touched.
inline std::vector<std::string> ReportsToRemove(std::vector<std::string> names, size_t keep)
{
    std::vector<std::string> ours;
    for (auto& n : names) if (IsReportDirName(n)) ours.push_back(std::move(n));
    std::sort(ours.begin(), ours.end());
    if (ours.size() <= keep) return {};
    ours.resize(ours.size() - keep);
    return ours;
}

// The costliest mod, `frame` (the total) left out: "density 3.2 ms/frame".
inline std::string TopMod(const std::vector<ModCost>& cost)
{
    for (const ModCost& c : cost)
        if (c.mod != Mod::frame && c.mod != Mod::none) return std::string(ModName(c.mod)) + " " + Fixed(c.avgMs, 1) + " ms/frame";
    return "none";
}

// out.txt's PERF line.
inline std::string PerfLine(const Episode& e, const std::string& room)
{
    const std::string where = room.empty() ? std::string("unknown") : room;
    if (e.kind == Kind::sustained) {
        const double mult = e.baselineMs > 0.0 ? e.duringMs / e.baselineMs : 0.0;
        return "PERF sustained " + Fixed(mult, 1) + "x for " + Fixed(e.seconds, 0) + " s | baseline "
               + Fixed(e.baselineMs, 1) + " ms | during " + Fixed(e.duringMs, 1) + " ms | room " + where
               + " | top " + TopMod(e.cost);
    }
    return "PERF hitch " + Fixed(e.worstMs, 0) + " ms frame | baseline " + Fixed(e.baselineMs, 1) + " ms | room "
           + where + " | top " + TopMod(e.cost);
}

inline std::string FreezeLine(const Episode& e)
{
    return "FREEZE " + Fixed(e.seconds, 0) + " s without a frame | in-hook " + ModName(e.inHook);
}

inline std::string FreezeEndedLine(double ms)
{
    return "FREEZE ended - the next frame came after " + Fixed(ms / 1000.0, 1) + " s";
}

inline std::string CrashLine(const std::string& exitCode, const std::string& module)
{
    return "CRASH previous session ended without a clean shutdown | exit "
           + (exitCode.empty() ? std::string("unknown") : exitCode) + " | module "
           + (module.empty() ? std::string("unknown") : module);
}

inline std::string ReportWrittenLine(const std::string& dir) { return "incident: report written " + dir; }

// ---- report.json ---------------------------------------------------------------

struct ReportFacts {
    Kind kind = Kind::none;
    std::string utc;
    std::string pluginVersion;
    std::string panelVersion;      // from bp_ipc\panel.json, when the panel wrote one
    std::string gameVersion;
    std::string room;
    long long monsters = -1;
    long long instances = -1;
    Episode episode;
    std::vector<ModCost> lastMinute;
    std::string exitJson;          // crash: the panel's exit.json, folded in as it is
    std::string previousBanner;    // crash: the banner line of the session that ended
    unsigned episodes = 0;
    unsigned suppressed = 0;
};

inline std::string CostJson(const std::vector<ModCost>& rows)
{
    std::string out = "[";
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i) out += ", ";
        out += "{\"mod\": " + JsonString(ModName(rows[i].mod)) + ", \"avgMs\": " + Fixed(rows[i].avgMs, 3)
               + ", \"worstMs\": " + Fixed(rows[i].worstMs, 3) + "}";
    }
    return out + "]";
}

inline std::string ReportJson(const ReportFacts& f)
{
    auto text = [](const std::string& s) { return s.empty() ? std::string("null") : JsonString(s); };
    auto count = [](long long v) { return v < 0 ? std::string("null") : std::to_string(v); };
    const Episode& e = f.episode;
    std::string exitObject = "null";
    {
        size_t a = f.exitJson.find_first_not_of(" \t\r\n");
        size_t b = f.exitJson.find_last_not_of(" \t\r\n");
        if (a != std::string::npos && f.exitJson[a] == '{' && f.exitJson[b] == '}') exitObject = f.exitJson.substr(a, b - a + 1);
        else if (a != std::string::npos) exitObject = JsonString(f.exitJson);
    }
    std::string j = "{\n";
    j += "  \"schema\": \"forgepact-incident/1\",\n";
    j += "  \"kind\": " + JsonString(BundleKindName(f.kind)) + ",\n";
    j += "  \"episode\": " + JsonString(KindName(e.kind == Kind::none ? f.kind : e.kind)) + ",\n";
    j += "  \"utc\": " + text(f.utc) + ",\n";
    j += "  \"pluginVersion\": " + text(f.pluginVersion) + ",\n";
    j += "  \"panelVersion\": " + text(f.panelVersion) + ",\n";
    j += "  \"gameVersion\": " + text(f.gameVersion) + ",\n";
    j += "  \"frames\": {\"medianBeforeMs\": " + Fixed(e.baselineMs, 2) + ", \"medianDuringMs\": " + Fixed(e.duringMs, 2)
         + ", \"worstMs\": " + Fixed(e.worstMs, 2) + ", \"seconds\": " + Fixed(e.seconds, 2)
         + ", \"inHook\": " + JsonString(ModName(e.inHook)) + ", \"room\": " + text(f.room)
         + ", \"monsters\": " + count(f.monsters) + ", \"instances\": " + count(f.instances) + "},\n";
    j += "  \"modsLastMinute\": " + CostJson(f.lastMinute) + ",\n";
    j += "  \"modsEpisode\": " + CostJson(e.cost) + ",\n";
    j += "  \"session\": {\"episodes\": " + std::to_string(f.episodes) + ", \"suppressed\": " + std::to_string(f.suppressed) + "},\n";
    j += "  \"exit\": " + exitObject + ",\n";
    j += "  \"previousSession\": " + text(f.previousBanner) + "\n";
    j += "}\n";
    return j;
}

// ---- mods.txt and system.txt --------------------------------------------------

struct ModFile {
    std::string name;
    unsigned long long size = 0;
    std::string version;
    std::string sha256;
};

inline std::string ModsText(const std::vector<ModFile>& files)
{
    std::string out = "name | size | file version | sha256\r\n";
    for (const ModFile& m : files)
        out += m.name + " | " + std::to_string(m.size) + " | " + (m.version.empty() ? "-" : m.version) + " | "
               + (m.sha256.empty() ? "-" : m.sha256) + "\r\n";
    return out;
}

struct SystemFacts {
    std::string windows;
    std::string cpu;
    unsigned cores = 0;
    std::vector<std::pair<std::string, std::string>> gpus;   // name, driver version
    unsigned long long ramTotalMb = 0;
    unsigned long long ramFreeMb = 0;
};

inline std::string SystemText(const SystemFacts& s)
{
    std::string out = "windows: " + (s.windows.empty() ? std::string("unknown") : s.windows) + "\r\n";
    out += "cpu: " + (s.cpu.empty() ? std::string("unknown") : s.cpu) + " (" + std::to_string(s.cores) + " logical processors)\r\n";
    if (s.gpus.empty()) out += "gpu: unknown\r\n";
    for (const auto& g : s.gpus) out += "gpu: " + g.first + " | driver " + (g.second.empty() ? std::string("unknown") : g.second) + "\r\n";
    out += "ram: " + std::to_string(s.ramTotalMb) + " MB, " + std::to_string(s.ramFreeMb) + " MB free\r\n";
    return out;
}

// ---- `incident stat` ------------------------------------------------------------

struct StatFacts {
    bool running = false;
    uint64_t frames = 0;
    uint64_t lost = 0;
    double baselineMs = 0.0;
    double worstMs = 0.0;
    bool grace = false;
    bool focused = true;
    bool armed = false;
    Mod inHook = Mod::none;
    unsigned episodes = 0;
    unsigned suppressed = 0;
    unsigned quiet = 0;
    unsigned bundles = 0;
    std::vector<ModCost> lastMinute;
    std::string lastReport;
};

inline std::vector<std::string> StatLines(const StatFacts& s)
{
    std::vector<std::string> lines;
    if (!s.running) {
        lines.push_back("incident: monitor not running - no frame is being watched");
        return lines;
    }
    lines.push_back("incident: monitor running | frames " + std::to_string(s.frames) + (s.lost ? " (" + std::to_string(s.lost) + " missed)" : std::string())
                    + " | baseline " + Fixed(s.baselineMs, 1) + " ms | worst " + Fixed(s.worstMs, 1) + " ms"
                    + " | watching " + (s.armed ? "yes" : "not yet (start-up)") + " | grace " + (s.grace ? "yes" : "no")
                    + " | focus " + (s.focused ? "yes" : "no") + " | in-hook " + ModName(s.inHook));
    lines.push_back("incident: episodes " + std::to_string(s.episodes) + ", held back " + std::to_string(s.suppressed)
                    + ", ignored near a room change or unfocused " + std::to_string(s.quiet)
                    + " | reports written " + std::to_string(s.bundles)
                    + (s.lastReport.empty() ? std::string() : " | last " + s.lastReport));
    if (s.lastMinute.empty()) {
        lines.push_back("incident: per mod over the last minute: no frames yet");
    } else {
        std::string row = "incident: per mod over the last minute, avg / worst ms per frame:";
        for (size_t i = 0; i < s.lastMinute.size(); ++i)
            row += std::string(i ? ", " : " ") + ModName(s.lastMinute[i].mod) + " " + Fixed(s.lastMinute[i].avgMs, 2)
                   + " / " + Fixed(s.lastMinute[i].worstMs, 2);
        lines.push_back(row);
    }
    return lines;
}

// ---- the monitor's shared state ------------------------------------------------

class Monitor {
public:
    // Heap-held and never freed: a process-lifetime object has nothing to run
    // at ExitProcess (see ExitSafeThread.hpp for why that matters here).
    static Monitor& Instance()
    {
        static Monitor* monitor = new Monitor();
        return *monitor;
    }

    // Starts the monitor thread once. The thread is never joined: ExitProcess
    // ends it, and the ExitSafeThread holder has no destructor to object.
    bool Start(void (*routine)() noexcept) noexcept
    {
        if (m_Started.load()) return true;
        if (!m_Thread.Start(routine)) return false;
        m_Started.store(true);
        return true;
    }
    bool Running() const noexcept { return m_Started.load(); }

    // ---- frame thread, once a second.
    void Arm() noexcept { m_Armed.store(true, std::memory_order_release); }

    void ObserveRoom(int64_t key, int64_t nowQpc) noexcept
    {
        if (key == INT64_MIN) return;   // unreadable is not a room
        if (m_RoomKey.load(std::memory_order_relaxed) == key) return;
        m_RoomKey.store(key, std::memory_order_relaxed);
        m_RoomChangeQpc.store(nowQpc, std::memory_order_release);
    }

    void StoreContext(const std::string& room, long long instances, long long monsters) noexcept
    {
        const uint32_t seq = m_RoomSeq.load(std::memory_order_relaxed);
        m_RoomSeq.store(seq + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        size_t i = 0;
        for (; i + 1 < kRoomChars && i < room.size(); ++i) m_RoomName[i].store(room[i], std::memory_order_relaxed);
        m_RoomName[i].store('\0', std::memory_order_relaxed);
        m_RoomSeq.store(seq + 2, std::memory_order_release);
        m_Instances.store(instances, std::memory_order_relaxed);
        m_Monsters.store(monsters, std::memory_order_relaxed);
    }

    // ---- any thread.
    bool Armed() const noexcept { return m_Armed.load(std::memory_order_acquire); }
    double RoomChangeMs() const noexcept
    {
        const int64_t q = m_RoomChangeQpc.load(std::memory_order_acquire);
        return q ? QpcToMs(q) : -1.0;
    }
    long long Instances() const noexcept { return m_Instances.load(std::memory_order_relaxed); }
    long long Monsters() const noexcept { return m_Monsters.load(std::memory_order_relaxed); }

    // The frame thread rewrites the name once a second; a copy taken while it
    // does is retried, and given up on (empty) rather than read torn.
    std::string Room() const
    {
        for (int attempt = 0; attempt < 8; ++attempt) {
            const uint32_t before = m_RoomSeq.load(std::memory_order_acquire);
            if (before & 1u) { Sleep(0); continue; }
            char copy[kRoomChars];
            for (size_t i = 0; i < kRoomChars; ++i) copy[i] = m_RoomName[i].load(std::memory_order_relaxed);
            std::atomic_thread_fence(std::memory_order_acquire);
            if (m_RoomSeq.load(std::memory_order_relaxed) != before) continue;
            copy[kRoomChars - 1] = '\0';
            return copy;
        }
        return {};
    }

    void Publish(StatFacts facts)
    {
        std::lock_guard<std::mutex> lock(m_StatLock);
        m_Stat = std::move(facts);
    }
    StatFacts Snapshot() const
    {
        std::lock_guard<std::mutex> lock(m_StatLock);
        StatFacts copy = m_Stat;
        copy.running = m_Started.load();
        return copy;
    }

    // ---- monitor thread only.
    Detector detector;
    MinuteTable minute;
    BundleLimiter bundles;
    uint64_t cursor = 0;
    uint64_t lost = 0;
    std::string lastReport;

private:
    Monitor() = default;
    static constexpr size_t kRoomChars = 64;

    ExitSafeThread m_Thread;
    std::atomic<bool> m_Started{ false };
    std::atomic<bool> m_Armed{ false };
    std::atomic<int64_t> m_RoomKey{ INT64_MIN };
    std::atomic<int64_t> m_RoomChangeQpc{ 0 };
    std::atomic<uint32_t> m_RoomSeq{ 0 };
    std::atomic<char> m_RoomName[kRoomChars]{};
    std::atomic<long long> m_Instances{ -1 };
    std::atomic<long long> m_Monsters{ -1 };
    mutable std::mutex m_StatLock;
    StatFacts m_Stat;
};

// ---- the clean-shutdown marker -------------------------------------------------

// Aurie runs no module code at process exit and this module has no DllMain,
// so the one piece of ForgePact that runs on a normal ExitProcess is this
// DLL's static destructors, at DLL_PROCESS_DETACH. A crash (Windows Error
// Reporting, TerminateProcess) runs none of them. So this object's destructor
// appends kCleanShutdownLine to out.txt, and the next load reads its absence
// as a crash.
//
// The destructor uses CreateFileA, WriteFile and CloseHandle only: by then the
// C runtime and the loader are tearing down, and a stream, an allocation or a
// library load there is the class of exit crash (0xC0000409) the guide's
// Known Limitations describe. The path is copied into a fixed buffer when the
// marker is armed, at load.
class ShutdownMarker {
public:
    ShutdownMarker() = default;
    ShutdownMarker(const ShutdownMarker&) = delete;
    ShutdownMarker& operator=(const ShutdownMarker&) = delete;

    void Arm(const char* path) noexcept
    {
        if (!path) return;
        const size_t n = std::strlen(path);
        if (!n || n >= sizeof(m_Path)) return;
        std::memcpy(m_Path, path, n + 1);
        m_Armed.store(true, std::memory_order_release);
    }

    bool Armed() const noexcept { return m_Armed.load(std::memory_order_acquire); }

    ~ShutdownMarker()
    {
        if (!m_Armed.load(std::memory_order_acquire)) return;
        const HANDLE file = CreateFileA(m_Path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return;
        DWORD written = 0;
        WriteFile(file, kCleanShutdownLine, static_cast<DWORD>(sizeof(kCleanShutdownLine) - 1), &written, nullptr);
        WriteFile(file, "\r\n", 2, &written, nullptr);
        CloseHandle(file);
    }

private:
    char m_Path[MAX_PATH] = {};
    std::atomic<bool> m_Armed{ false };
};

inline ShutdownMarker g_ShutdownMarker;

} // namespace ForgePact::Incident
