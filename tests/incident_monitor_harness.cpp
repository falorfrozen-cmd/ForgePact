// The incident monitor (ForgePact issue #76) without a game.
//
// Compiles plugin/include/ForgePact/IncidentMonitor.hpp whole and drives it
// the way ModuleMain.cpp's adapter does: frames arrive on a clock, the monitor
// wakes every kWakeMs, and each wake hands the Detector what it can see (the
// newest frame's time, focus, the last room change, the two tags). Here the
// clock is simulated, so ten seconds of 60 fps take no time; the per-mod
// accounting, the installer's tag thunks and the clean-shutdown marker run for
// real.
//
// Usage:  incident_monitor_harness.exe <incident_shutdown_probe.dll> <work dir> <fixture dir>
//         incident_monitor_harness.exe --child <exit|terminate> <probe.dll> <marker path>
//
// <fixture dir> is tests/fixtures/incident: the exit.json and panel.json the
// panel writes, read here through the same parsers the plugin uses.
//
// Each scenario prints one line, `<name> | <pass|fail> | <detail>`, and the
// process exits 0 only when every scenario passed.
// tests/test_incident_monitor_behavior.py compiles and runs it.

#include <ForgePact/IncidentMonitor.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace inc = ForgePact::Incident;

namespace {

int g_Failures = 0;

void Report(const char* name, bool ok, const std::string& detail)
{
    std::printf("%s | %s | %s\n", name, ok ? "pass" : "fail", detail.c_str());
    std::fflush(stdout);
    if (!ok) ++g_Failures;
}

std::string Num(double v)
{
    char b[32];
    std::snprintf(b, sizeof(b), "%.1f", v);
    return b;
}

std::string HookName(const char* id) { return id ? id : "none"; }

// ---- a monitor on a simulated clock ---------------------------------------

struct Sim {
    inc::Detector detector;
    inc::Inputs in;
    double now = 1000.0;                  // ms on the simulated clock
    double nextWake = 1000.0 + inc::kWakeMs;
    double lastFed = -1.0;                // end of the newest frame the monitor has seen
    double carry = 0.0;                   // stall time the next frame's duration includes
    std::vector<inc::FrameSample> pending;
    std::vector<inc::Episode> episodes;
    // A room change the frame thread has not sampled yet: it becomes visible
    // to the monitor at `roomSeenAt`, the way the once-a-second sample lands.
    double roomSeenAt = -1.0;
    double roomChange = -1.0;
    // The freeze ends the monitor logged (FreezeEndedLine).
    int ended = 0;
    double endedMs = 0.0;
    bool endedLoad = false;

    Sim() { in.armed = true; in.focused = true; in.windowAlive = true; }

    void Wake()
    {
        std::vector<inc::FrameSample> feed;
        std::vector<inc::FrameSample> keep;
        for (const auto& f : pending) (f.endMs <= nextWake ? feed : keep).push_back(f);
        pending.swap(keep);
        if (!feed.empty()) lastFed = feed.back().endMs;
        detector.Feed(feed.data(), feed.size());
        if (roomSeenAt >= 0.0 && nextWake >= roomSeenAt) in.roomChangeMs = roomChange;
        in.nowMs = nextWake;
        in.lastFrameMs = lastFed;
        for (int i = 0; i < 16; ++i) {
            const inc::Episode e = detector.Analyze(in);
            if (e.kind == inc::Kind::none) break;
            episodes.push_back(e);
        }
        double ms = 0.0;
        bool load = false;
        if (detector.TakeFreezeEnded(ms, load)) {
            ++ended;
            endedMs = ms;
            endedLoad = load;
        }
        nextWake += inc::kWakeMs;
    }

    void Frame(double ms)
    {
        now += ms;
        inc::FrameSample f;
        f.endMs = now;
        f.frameMs = ms + carry;
        carry = 0.0;
        pending.push_back(f);
        while (nextWake <= now) Wake();
    }

    void Frames(double seconds, double ms)
    {
        const int n = static_cast<int>(seconds * 1000.0 / ms + 0.5);
        for (int i = 0; i < n; ++i) Frame(ms);
    }

    // No frame at all for `ms`: the next frame's duration includes it.
    void Stall(double ms)
    {
        now += ms;
        carry += ms;
        while (nextWake <= now) Wake();
    }

    // A room change the frame thread sees with the next frame, as after a load.
    void RoomChangesWithNextFrame()
    {
        roomChange = now + 16.7;
        roomSeenAt = now + 16.7;
    }

    // Lets the monitor judge everything already fed (frames are judged
    // kJudgeDelayMs after they end) without starving it into a freeze.
    void Settle() { Frames(3.0, 16.7); }

    int Count(inc::Kind k) const
    {
        int n = 0;
        for (const auto& e : episodes) n += e.kind == k ? 1 : 0;
        return n;
    }

    std::string Describe() const
    {
        std::string s = std::to_string(episodes.size()) + " episode(s)";
        for (const auto& e : episodes)
            s += std::string(" [") + inc::KindName(e.kind) + " worst " + Num(e.worstMs) + " ms baseline "
                 + Num(e.baselineMs) + " in-hook " + (e.inHook.empty() ? "none" : e.inHook) + " in-mod "
                 + inc::ModName(e.inMod) + "]";
        s += ", suppressed " + std::to_string(detector.Suppressed()) + ", quiet " + std::to_string(detector.Quiet());
        return s;
    }
};

// Busy-waits on the clock: Sleep's granularity is too coarse to give a scope
// a known cost.
void Spin(double ms)
{
    const int64_t end = inc::Qpc() + static_cast<int64_t>(ms * static_cast<double>(inc::QpcFrequency()) / 1000.0);
    while (inc::Qpc() < end) {}
}

// ---- scenarios ------------------------------------------------------------

// Baseline: ordinary play, including a zone change whose loading frames would
// read as hitches anywhere else.
void SteadyWithZoneChange()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.Frame(120.0);
    sim.Frame(400.0);
    sim.Frame(120.0);
    sim.Frame(120.0);
    // The frame thread samples the room once a second, so the change is seen
    // a little after the loading frames ended.
    sim.roomChange = sim.now + 600.0;
    sim.roomSeenAt = sim.now + 600.0;
    sim.Frames(5.0, 16.7);
    sim.Settle();
    Report("steady-60fps-with-zone-change", sim.episodes.empty() && sim.detector.Quiet() >= 1, sim.Describe());
}

void SingleHitch()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.Frame(400.0);
    sim.Frames(5.0, 16.7);
    sim.Settle();
    bool ok = sim.episodes.size() == 1 && sim.Count(inc::Kind::hitch) == 1;
    std::string line;
    if (ok) {
        const inc::Episode& e = sim.episodes[0];
        line = inc::PerfLine(e, "Act_01_01");
        ok = e.worstMs > 399.0 && e.worstMs < 401.0 && e.baselineMs > 16.0 && e.baselineMs < 17.5
             && line.rfind("PERF hitch 400 ms frame | baseline 16.7 ms | room Act_01_01", 0) == 0;
    }
    Report("single-400ms-frame", ok, sim.Describe() + " | " + line);
}

void Sustained()
{
    Sim sim;
    sim.Frames(10.0, 16.7);
    sim.Frames(3.0, 42.0);
    sim.Frames(3.0, 16.7);
    sim.Settle();
    bool ok = sim.episodes.size() == 1 && sim.Count(inc::Kind::sustained) == 1 && sim.Count(inc::Kind::hitch) == 0;
    std::string line;
    if (ok) {
        const inc::Episode& e = sim.episodes[0];
        line = inc::PerfLine(e, "");
        ok = e.duringMs > 41.0 && e.duringMs < 43.0 && e.baselineMs > 16.0 && e.baselineMs < 17.5
             && e.seconds >= 2.0 && line.rfind("PERF sustained 2.5x for 2 s", 0) == 0;
    }
    Report("sustained-2.5x-3s", ok, sim.Describe() + " | " + line);
}

// Both tags come from the real accounting: the hook id from the RAII tag the
// installer's thunk uses, the mod from a real scope. A freeze is reported
// once frames are back and no room change explains it, and names both.
void FreezeInHook()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    size_t during = 0;
    {
        inc::IncidentScope scope(inc::Mod::mapreveal);
        inc::IncidentHookTag tag("harness_hook");
        sim.in.inMod = inc::g_Accounting.InMod();
        sim.in.inHookId = inc::g_Accounting.InHookId();
        sim.Stall(4000.0);
        during = sim.episodes.size();
    }
    sim.in.inMod = inc::g_Accounting.InMod();
    sim.in.inHookId = inc::g_Accounting.InHookId();
    const bool restored = sim.in.inMod == inc::Mod::none && sim.in.inHookId == nullptr;
    sim.Frames(3.0, 16.7);
    sim.Settle();
    bool ok = restored && during == 0 && sim.episodes.size() == 1 && sim.Count(inc::Kind::freeze) == 1
              && sim.ended == 1 && sim.endedMs >= 4000.0 && !sim.endedLoad;
    std::string line;
    if (ok) {
        const inc::Episode& e = sim.episodes[0];
        line = inc::FreezeLine(e);
        ok = e.inMod == inc::Mod::mapreveal && e.inHook == "harness_hook" && e.seconds >= 4.0
             && line.rfind("FREEZE 4 s without a frame | in-hook harness_hook | in-mod mapreveal", 0) == 0;
    }
    Report("freeze-4s-in-hook", ok, sim.Describe() + " | " + line + " | reported while frozen " + std::to_string(during)
           + " | ended after " + Num(sim.endedMs) + " ms");
}

void FreezeNoHook()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.in.inMod = inc::g_Accounting.InMod();
    sim.in.inHookId = inc::g_Accounting.InHookId();
    sim.Stall(4000.0);
    const size_t during = sim.episodes.size();
    sim.Frames(3.0, 16.7);
    sim.Settle();
    bool ok = during == 0 && sim.episodes.size() == 1 && sim.Count(inc::Kind::freeze) == 1;
    std::string line;
    if (ok) {
        line = inc::FreezeLine(sim.episodes[0]);
        ok = sim.episodes[0].inMod == inc::Mod::none && sim.episodes[0].inHook.empty()
             && line.find("| in-hook none | in-mod none") != std::string::npos;
    }
    // Negative control: a minimized game that stops drawing is not frozen.
    Sim hidden;
    hidden.Frames(5.0, 16.7);
    hidden.in.minimized = true;
    hidden.Stall(4000.0);
    hidden.in.minimized = false;
    hidden.Frames(3.0, 16.7);
    hidden.Settle();
    ok = ok && hidden.Count(inc::Kind::freeze) == 0;
    Report("freeze-4s-no-hook", ok, sim.Describe() + " | " + line + " | minimized: " + hidden.Describe());
}

// D13: a zone load blocks the frame thread, and the room change is seen only
// with the first frame after it. That gap is a load, not a freeze.
void FreezeLoadRoomChange()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.Stall(3500.0);
    const size_t during = sim.episodes.size();
    sim.RoomChangesWithNextFrame();
    sim.Frames(3.0, 16.7);
    sim.Settle();
    const std::string line = inc::FreezeEndedLine(sim.endedMs, sim.endedLoad);
    bool ok = during == 0 && sim.episodes.empty() && sim.detector.Quiet() == 1 && sim.ended == 1 && sim.endedLoad
              && line.find("after a room change: a load, not reported") != std::string::npos;
    // Control: a room change from before the gap began explains nothing.
    Sim early;
    early.Frames(5.0, 16.7);
    early.RoomChangesWithNextFrame();
    early.Frames(1.0, 16.7);
    early.Stall(3500.0);
    early.Frames(3.0, 16.7);
    early.Settle();
    ok = ok && early.Count(inc::Kind::freeze) == 1 && !early.endedLoad;
    Report("freeze-load-room-change", ok, sim.Describe() + " | " + line + " | room change before the gap: " + early.Describe());
}

// A freeze that does not end is reported once it reaches kFreezeHoldMs, and
// its end is still logged when frames come back, with no second episode.
void FreezeNeverEnds()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.Stall(16000.0);
    const int during = sim.Count(inc::Kind::freeze);
    const double seconds = during == 1 ? sim.episodes[0].seconds : 0.0;
    sim.Frames(3.0, 16.7);
    sim.Settle();
    const bool ok = during == 1 && seconds >= inc::kFreezeHoldMs / 1000.0 && sim.Count(inc::Kind::freeze) == 1
                    && sim.episodes.size() == 1 && sim.ended == 1 && !sim.endedLoad && sim.endedMs >= 16000.0;
    Report("freeze-never-ends", ok, sim.Describe() + " | reported after " + Num(seconds) + " s | "
           + inc::FreezeEndedLine(sim.endedMs, sim.endedLoad));
}

void Unfocused()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.in.focused = false;
    sim.Frames(1.0, 16.7);
    sim.Frame(400.0);
    sim.Frames(1.0, 16.7);
    sim.in.focused = true;
    sim.Frames(5.0, 16.7);
    sim.Settle();
    Report("unfocused-suppressed", sim.episodes.empty() && sim.detector.Quiet() == 1, sim.Describe());
}

void RateLimit()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.Frame(400.0);
    sim.Frames(5.0, 16.7);
    sim.Frame(400.0);
    sim.Frames(5.0, 16.7);
    sim.Settle();
    // Past the gap, a third one is reported again.
    sim.Frames(30.0, 16.7);
    sim.Frame(400.0);
    sim.Frames(3.0, 16.7);
    sim.Settle();
    const bool ok = sim.Count(inc::Kind::hitch) == 2 && sim.detector.Suppressed() == 1
                    && sim.episodes.size() == 2 && sim.episodes[1].atMs - sim.episodes[0].atMs >= inc::kEpisodeGapMs;
    Report("rate-limit-30s", ok, sim.Describe());
}

void PerModAccounting()
{
    auto& acct = inc::g_Accounting;
    uint64_t cursor = acct.Frames();
    uint64_t lost = 0;
    acct.OnFrame(inc::Qpc());
    cursor = acct.Frames();
    for (int frame = 0; frame < 3; ++frame) {
        { inc::IncidentScope s(inc::Mod::gems); Spin(8.0); }
        { inc::IncidentScope s(inc::Mod::miner); Spin(2.0); }
        // Sixteen calls, one of them timed and counted sixteen times.
        for (int call = 0; call < 16; ++call) { inc::IncidentSampledScope s(inc::Mod::density); Spin(1.0); }
        // A scope inside one of its own mod counts once, not twice.
        { inc::IncidentScope outer(inc::Mod::drops); { inc::IncidentScope inner(inc::Mod::drops); Spin(1.0); } }
        acct.OnFrame(inc::Qpc());
    }
    std::vector<inc::FrameSample> frames;
    acct.CopySince(cursor, frames, lost);
    inc::MinuteTable table;
    for (const auto& f : frames) table.Feed(f);
    const std::vector<inc::ModCost> rows = frames.empty() ? std::vector<inc::ModCost>{} : table.Rows(frames.back().endMs);
    std::string detail = std::to_string(frames.size()) + " frames:";
    for (const auto& r : rows) detail += std::string(" ") + inc::ModName(r.mod) + " " + Num(r.avgMs) + "/" + Num(r.worstMs);
    bool ok = frames.size() == 3 && lost == 0 && rows.size() == 4
              && rows[0].mod == inc::Mod::density && rows[1].mod == inc::Mod::gems
              && rows[2].mod == inc::Mod::miner && rows[3].mod == inc::Mod::drops;
    if (ok) {
        ok = rows[0].avgMs >= 12.0 && rows[0].avgMs <= 24.0    // 1 ms x 16, not 1 ms
             && rows[1].avgMs >= 7.5 && rows[1].avgMs <= 11.0
             && rows[3].avgMs >= 0.9 && rows[3].avgMs <= 1.8;  // nested drops counted once
        // Every frame's duration covers the scopes that ran in it.
        for (const auto& f : frames) ok = ok && f.frameMs >= 26.0;
    }
    inc::Episode e;
    e.kind = inc::Kind::hitch;
    e.worstMs = 400.0;
    e.baselineMs = 16.7;
    e.cost = rows;
    const std::string line = inc::PerfLine(e, "Harness_rm");
    ok = ok && line.find("| top density ") != std::string::npos && line.find(" ms/frame") != std::string::npos;
    Report("per-mod-accounting", ok, detail + " | " + line);
}

// D16: a slow frame inside a room change's grace is the worst frame, but not
// a judged one; the judged worst and the slow judged count say what the
// detector actually weighed.
void WorstJudgedVsOverall()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.roomChange = sim.now + 300.0;
    sim.roomSeenAt = sim.now + 300.0;
    sim.Frame(400.0);
    sim.Frames(8.0, 16.7);
    sim.Frame(300.0);
    sim.Frames(3.0, 16.7);
    sim.Settle();
    const inc::Detector& d = sim.detector;
    const bool ok = d.WorstMs() > 399.0 && d.WorstMs() < 401.0 && !d.WorstWasJudged()
                    && d.WorstJudgedMs() > 299.0 && d.WorstJudgedMs() < 301.0 && d.SlowJudgedFrames() == 1
                    && sim.Count(inc::Kind::hitch) == 1;
    Report("worst-judged-vs-overall", ok, "overall worst " + Num(d.WorstMs()) + " ms judged " + (d.WorstWasJudged() ? "yes" : "no")
           + " | worst judged " + Num(d.WorstJudgedMs()) + " ms | slow judged frames " + std::to_string(d.SlowJudgedFrames())
           + " | " + sim.Describe());
}

// ---- the installer's tag thunk (D8, channel 1) ----------------------------

const char* g_SeenHook = "unset";
inc::Mod g_SeenMod = inc::Mod::Count;

int TripleTarget(int x)
{
    g_SeenHook = inc::g_Accounting.InHookId();
    g_SeenMod = inc::g_Accounting.InMod();
    return x * 3;
}

int OtherTarget(int x) { return x + 1; }

template <int N>
long FillTarget(long x) { return x + N; }

template <size_t... I>
size_t FillTable(std::index_sequence<I...>)
{
    using Thunks = inc::TaggedThunks<long (*)(long)>;
    size_t own = 0;
    ((own += Thunks::Tagged("fill", &FillTarget<static_cast<int>(I)>) == &FillTarget<static_cast<int>(I)> ? 1u : 0u), ...);
    return own;
}

void HookTagThunk()
{
    using Thunks = inc::TaggedThunks<int (*)(int)>;
    const unsigned taggedBefore = inc::g_Accounting.HooksTagged();
    const auto thunk = Thunks::Tagged("harness_triple", &TripleTarget);
    const auto again = Thunks::Tagged("harness_triple_again", &TripleTarget);
    const auto other = Thunks::Tagged("harness_other", &OtherTarget);
    bool ok = thunk != &TripleTarget && again == thunk && other != thunk && Thunks::Tagged() == 2 && Thunks::Untagged() == 0
              && inc::g_Accounting.HooksTagged() == taggedBefore + 2;
    std::string detail = "slots " + std::to_string(Thunks::Tagged());
    // Through the thunk: the target's value, the slot's id inside, none after.
    const int value = thunk(7);
    const bool inside = g_SeenHook && std::string(g_SeenHook) == "harness_triple";
    const bool after = inc::g_Accounting.InHookId() == nullptr;
    ok = ok && value == 21 && inside && after && g_SeenMod == inc::Mod::none && other(1) == 2;
    detail += " | value " + std::to_string(value) + " | inside " + HookName(g_SeenHook) + " | after "
              + HookName(inc::g_Accounting.InHookId());
    // Inside a mod's scope the thunk sets the hook id and leaves the mod alone.
    {
        inc::IncidentScope scope(inc::Mod::gems);
        thunk(1);
        ok = ok && g_SeenMod == inc::Mod::gems && inc::g_Accounting.InMod() == inc::Mod::gems
             && g_SeenHook && std::string(g_SeenHook) == "harness_triple";
    }
    ok = ok && inc::g_Accounting.InMod() == inc::Mod::none && inc::g_Accounting.InHookId() == nullptr;
    detail += " | in a gems scope: mod " + std::string(inc::ModName(g_SeenMod));
    // A full table hands the caller's own pointer back, untagged and counted.
    using Fill = inc::TaggedThunks<long (*)(long)>;
    const unsigned untaggedBefore = inc::g_Accounting.HooksUntagged();
    const size_t own = FillTable(std::make_index_sequence<inc::kHookSlots + 1>{});
    ok = ok && own == 1 && Fill::Tagged() == inc::kHookSlots && Fill::Untagged() == 1
         && inc::g_Accounting.HooksUntagged() == untaggedBefore + 1;
    detail += " | full table: " + std::to_string(Fill::Tagged()) + " tagged, " + std::to_string(Fill::Untagged()) + " untagged";
    Report("hook-tag-thunk", ok, detail);
}

// D16: `incident stat`'s first line is the live marker and says what was judged.
void StatLinePrefix()
{
    inc::StatFacts s;
    s.running = true;
    s.frames = 1200;
    s.baselineMs = 16.7;
    s.worstMs = 400.0;
    s.worstJudged = false;
    s.worstJudgedMs = 300.0;
    s.slowJudged = 1;
    s.window = true;
    s.armed = true;
    s.inHook = "harness_hook";
    s.inMod = inc::Mod::density;
    s.hooksTagged = 42;
    s.hooksUntagged = 10;
    s.writeErrors = 0;
    const std::vector<std::string> lines = inc::StatLines(s);
    bool ok = lines.size() == 4 && lines[0].rfind("incident: frames ", 0) == 0;
    if (ok) {
        for (const char* part : { " | baseline ", " | worst judged ", " | slow judged frames ", " | window ", " | in-hook " })
            ok = ok && lines[0].find(part) != std::string::npos;
        ok = ok && lines[0].find("| window yes") != std::string::npos && lines[0].find("(not judged)") != std::string::npos
             && lines[2].rfind("incident: hooks tagged 42, untagged 10 | report write errors 0", 0) == 0;
    }
    // Not running: one line that says so.
    const std::vector<std::string> idle = inc::StatLines(inc::StatFacts{});
    ok = ok && idle.size() == 1 && idle[0].rfind("incident: monitor not running", 0) == 0;
    std::string detail;
    for (const auto& l : lines) detail += (detail.empty() ? "" : " // ") + l;
    Report("stat-line-prefix", ok, detail);
}

void ScrubUsername()
{
    const std::string profile = "C:\\Users\\Jane Doe";
    const std::string text =
        "exe C:\\Users\\Jane Doe\\AppData\\Local\\x.json\n"
        "json \"C:\\\\Users\\\\Jane Doe\\\\Desktop\\\\Hero Siege\"\n"
        "fwd c:/users/jane doe/Saved\n"
        "other C:\\Users\\Jane Doe2\\x\n"
        "end C:\\Users\\Jane Doe";
    const std::string want =
        "exe %USERPROFILE%\\AppData\\Local\\x.json\n"
        "json \"%USERPROFILE%\\\\Desktop\\\\Hero Siege\"\n"
        "fwd %USERPROFILE%/Saved\n"
        "other C:\\Users\\Jane Doe2\\x\n"
        "end %USERPROFILE%";
    const std::string got = inc::ScrubProfile(text, profile);
    // An empty profile scrubs nothing rather than everything.
    const bool ok = got == want && inc::ScrubProfile(text, "") == text;
    Report("scrub-username", ok, ok ? "every form replaced, a longer name kept" : got);
}

// Runs this harness as a child that loads the probe DLL, arms the marker and
// ends the way the test asks. Returns the child's exit code.
DWORD RunChild(const std::string& how, const std::string& probe, const std::string& marker)
{
    char self[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    std::string cmd = std::string("\"") + self + "\" --child " + how + " \"" + probe + "\" \"" + marker + "\"";
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) return 0xFFFFFFFF;
    DWORD code = 0xFFFFFFFE;
    if (WaitForSingleObject(pi.hProcess, 30000) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
    else TerminateProcess(pi.hProcess, 0xDEAD);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return code;
}

std::string ReadAll(const std::filesystem::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

void ExitCases(const std::string& probe, const std::filesystem::path& work)
{
    std::error_code ec;
    std::filesystem::create_directories(work, ec);
    {
        // The probe has no ExitProcess hook, so the marker comes from the
        // second writer, the DLL's static destructor at detach.
        const auto marker = work / "exit-clean.txt";
        std::filesystem::remove(marker, ec);
        std::filesystem::remove(marker.string() + ".armed", ec);
        const DWORD code = RunChild("exit", probe, marker.string());
        const std::string text = ReadAll(marker);
        const bool armed = std::filesystem::exists(marker.string() + ".armed");
        const bool written = text.find(inc::kCleanShutdownDetachLine) != std::string::npos;
        const bool ok = code == 0 && armed && written && inc::SessionEndedCleanly(text);
        char b[96];
        std::snprintf(b, sizeof(b), "exit 0x%lX, armed %s, marker %s", code, armed ? "yes" : "no", written ? "written" : "missing");
        Report("exit-clean", ok, b);
    }
    {
        const auto marker = work / "exit-terminated.txt";
        std::filesystem::remove(marker, ec);
        std::filesystem::remove(marker.string() + ".armed", ec);
        const DWORD code = RunChild("terminate", probe, marker.string());
        const bool armed = std::filesystem::exists(marker.string() + ".armed");
        const bool written = std::filesystem::exists(marker);
        // The positive control is the `.armed` file: the child got as far as
        // arming the marker, so its absence is the termination's doing.
        const bool ok = code == 0x7E3 && armed && !written;
        char b[96];
        std::snprintf(b, sizeof(b), "exit 0x%lX, armed %s, marker %s", code, armed ? "yes" : "no", written ? "written" : "absent");
        Report("exit-terminated", ok, b);
    }
}

// D14: the exit hook and the destructor both call Write; the first one wins
// and names its route, the second writes nothing.
void MarkerOnce(const std::filesystem::path& work)
{
    std::error_code ec;
    std::filesystem::create_directories(work, ec);
    const auto path = work / "marker-once.txt";
    std::filesystem::remove(path, ec);
    bool first = false, second = false, unarmed = true;
    {
        inc::ShutdownMarker marker;
        unarmed = !marker.Write(inc::ShutdownRoute::exitProcess);   // not armed yet: nothing
        marker.Arm(path.string().c_str());
        first = marker.Write(inc::ShutdownRoute::exitProcess);
        second = marker.Write(inc::ShutdownRoute::detach);
    }   // ...and its destructor, the detach route, writes nothing either
    const std::string text = ReadAll(path);
    const bool ok = unarmed && first && !second && text == std::string(inc::kCleanShutdownLine) + "\r\n"
                    && inc::SessionEndedCleanly(text) && text.find("(detach)") == std::string::npos;
    // A chat line that quotes the marker mid-line is not one.
    const bool quoted = !inc::SessionEndedCleanly(std::string("chat: said ") + inc::kCleanShutdownLine + "\r\n");
    Report("marker-once", ok && quoted, std::string("route exit hook: ") + (first ? "written" : "missing") + ", detach after it: "
           + (second ? "written again" : "nothing") + ", quoted mid-line " + (quoted ? "ignored" : "matched") + " | " + text.substr(0, text.find('\r')));
}

int Child(const std::string& how, const std::string& probe, const std::string& marker)
{
    const HMODULE dll = LoadLibraryA(probe.c_str());
    if (!dll) return 0x7E0;
    using Arm = int (*)(const char*);
    const auto arm = reinterpret_cast<Arm>(GetProcAddress(dll, "ProbeArm"));
    if (!arm || !arm(marker.c_str())) return 0x7E1;
    { std::ofstream f(marker + ".armed"); f << "armed\n"; }
    Sleep(100);   // the probe's monitor thread is running
    if (how == "terminate") TerminateProcess(GetCurrentProcess(), 0x7E3);
    ExitProcess(0);
}

// D9's next-load check: which session was the previous one, and how it ended.
void CrashCheck()
{
    const std::string banner = std::string(inc::kSessionBanner) + " v2.2.0\r\n";
    const std::string running = std::string(inc::kMonitorRunningLine) + " | hitch 250 ms\r\n";
    const std::string clean = std::string(inc::kCleanShutdownLine) + "\r\n";
    const std::string detach = std::string(inc::kCleanShutdownDetachLine) + "\r\n";
    std::string session;
    bool ok = true;
    std::string detail;

    // Two sessions in out.txt, the earlier one never shut down cleanly.
    ok = ok && inc::PreviousSession(banner + running + "line\r\n" + banner + "now\r\n", "", session)
         && inc::SessionRanMonitor(session) && !inc::SessionEndedCleanly(session) && session.find("now") == std::string::npos;
    detail += ok ? "crash seen" : "crash missed";
    // The same, ended cleanly, by either writer.
    const bool cleanOk = inc::PreviousSession(banner + running + clean + banner, "", session) && inc::SessionEndedCleanly(session)
                         && inc::PreviousSession(banner + running + detach + banner, "", session) && inc::SessionEndedCleanly(session);
    ok = ok && cleanOk;
    detail += cleanOk ? ", clean seen" : ", clean missed";
    // out.txt rotated at this load: the previous session is out.prev.txt's last.
    const bool rotated = inc::PreviousSession(banner, "old\r\n" + banner + "a\r\n" + banner + running + "tail\r\n", session)
                         && session.find("tail") != std::string::npos && session.find("a\r\n") == std::string::npos;
    ok = ok && rotated;
    detail += rotated ? ", rotated" : ", rotation missed";
    // A first run has no previous session; one from before the monitor existed is not judged.
    const bool first = !inc::PreviousSession(banner, "", session);
    const bool older = inc::PreviousSession(banner + "old\r\n" + banner, "", session) && !inc::SessionRanMonitor(session);
    ok = ok && first && older;
    detail += first ? ", first run quiet" : ", first run judged";
    detail += older ? ", pre-monitor session skipped" : ", pre-monitor session judged";
    Report("crash-check", ok, detail);
}

void BundleRetention()
{
    std::vector<std::string> names;
    for (int i = 0; i < 12; ++i) {
        char b[48];
        std::snprintf(b, sizeof(b), "202610%02d-120000_%s", i + 1, i % 3 == 0 ? "perf" : (i % 3 == 1 ? "freeze" : "crash"));
        names.push_back(b);
    }
    names.push_back(".pending-20261013-120000_perf");
    names.push_back("notes");
    names.push_back("20261099-1200_perf");
    const std::vector<std::string> drop = inc::ReportsToRemove(names, inc::kKeepReports);
    const bool ok = drop.size() == 2 && drop[0] == "20261001-120000_perf" && drop[1] == "20261002-120000_freeze"
                    && inc::IsReportDirName("20261001-120000_crash") && !inc::IsReportDirName("notes");
    std::string detail;
    for (const auto& d : drop) detail += d + " ";
    Report("bundle-retention", ok, detail);
}

// D16: report.json's `exit` object and the panel's liveness come from files
// the panel writes. The shared fixture is read through the plugin's own
// parsers, for every key ModuleMain.cpp reads.
void ExitJsonFixture(const std::filesystem::path& fixtures)
{
    const std::string exitJson = ReadAll(fixtures / "exit.json");
    const std::string panelJson = ReadAll(fixtures / "panel.json");
    std::string code, module, version;
    double pid = 0.0;
    const bool read = inc::JsonStringField(exitJson, "exit_code", code) && inc::JsonStringField(exitJson, "faulting_module", module)
                      && inc::JsonStringField(panelJson, "version", version) && inc::JsonNumberField(panelJson, "pid", pid);
    const bool ok = !exitJson.empty() && !panelJson.empty() && read && code == "0xC0000005" && module == "KERNELBASE.dll"
                    && version == "2.2.0" && pid == 4242.0;
    Report("exit-json-fixture", ok, "exit_code " + code + ", faulting_module " + module + ", version " + version + ", pid "
           + Num(pid) + " from " + fixtures.string());
}

// The report builders' JSON, written out for the Python side to parse.
void ReportJson(const std::filesystem::path& work)
{
    std::error_code ec;
    std::filesystem::create_directories(work, ec);
    inc::ReportFacts perf;
    perf.kind = inc::Kind::sustained;
    perf.utc = "2026-10-02T12:00:00Z";
    perf.pluginVersion = "2.2.0";
    perf.gameVersion = "1.0.0.0";
    perf.room = "Act \"01\"\\01";
    perf.monsters = 78;
    perf.instances = 6072;
    perf.episode.kind = inc::Kind::sustained;
    perf.episode.baselineMs = 16.7;
    perf.episode.duringMs = 42.0;
    perf.episode.worstMs = 44.0;
    perf.episode.seconds = 2.0;
    perf.episode.cost = { { inc::Mod::density, 3.25, 6.5 }, { inc::Mod::gems, 0.5, 1.0 } };
    perf.lastMinute = perf.episode.cost;
    inc::ReportFacts freeze;
    freeze.kind = inc::Kind::freeze;
    freeze.utc = perf.utc;
    freeze.pluginVersion = "2.2.0";
    freeze.panelVersion = "2.2.0";
    freeze.episode.kind = inc::Kind::freeze;
    freeze.episode.seconds = 3.2;
    freeze.episode.inMod = inc::Mod::mapreveal;   // set by a scope; no tagged hook
    inc::ReportFacts crash;
    crash.kind = inc::Kind::crash;
    crash.utc = perf.utc;
    crash.pluginVersion = "2.2.0";
    crash.episode.kind = inc::Kind::crash;
    crash.previousBanner = std::string(inc::kSessionBanner) + " v2.2.0";
    crash.exitJson = "{\"exit_code\": \"0xC0000005\", \"faulting_module\": \"Hero_Siege.exe\", \"event_probe\": {\"queried\": true, \"records_seen\": 3}}";
    std::string paths;
    const std::pair<const char*, const inc::ReportFacts*> files[] = { { "perf", &perf }, { "freeze", &freeze }, { "crash", &crash } };
    for (const auto& [name, facts] : files) {
        const auto p = work / (std::string(name) + ".json");
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f << inc::ReportJson(*facts);
        paths += (paths.empty() ? "" : ";") + p.string();
    }
    std::string code, module;
    const bool fields = inc::JsonStringField(crash.exitJson, "exit_code", code) && code == "0xC0000005"
                        && inc::JsonStringField(crash.exitJson, "faulting_module", module) && module == "Hero_Siege.exe";
    Report("report-json", fields, paths);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 5 && std::string(argv[1]) == "--child") return Child(argv[2], argv[3], argv[4]);
    if (argc < 4) {
        std::printf("usage: incident_monitor_harness <probe.dll> <work dir> <fixture dir>\n");
        return 2;
    }
    const std::filesystem::path work = argv[2];
    SteadyWithZoneChange();
    SingleHitch();
    Sustained();
    FreezeInHook();
    FreezeNoHook();
    FreezeLoadRoomChange();
    FreezeNeverEnds();
    Unfocused();
    RateLimit();
    PerModAccounting();
    WorstJudgedVsOverall();
    HookTagThunk();
    StatLinePrefix();
    ScrubUsername();
    ExitCases(argv[1], work / "exit");
    MarkerOnce(work / "marker");
    CrashCheck();
    BundleRetention();
    ExitJsonFixture(argv[3]);
    ReportJson(work / "json");
    std::printf("RESULT %s\n", g_Failures ? "FAIL" : "OK");
    return g_Failures ? 1 : 0;
}
