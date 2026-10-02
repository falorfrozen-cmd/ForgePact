// The incident monitor (ForgePact issue #76) without a game.
//
// Compiles plugin/include/ForgePact/IncidentMonitor.hpp whole and drives it
// the way ModuleMain.cpp's adapter does: frames arrive on a clock, the monitor
// wakes every kWakeMs, and each wake hands the Detector what it can see (the
// newest frame's time, focus, the last room change, the in-hook tag). Here the
// clock is simulated, so ten seconds of 60 fps take no time; the per-mod
// accounting and the clean-shutdown marker run for real.
//
// Usage:  incident_monitor_harness.exe <incident_shutdown_probe.dll> <work dir>
//         incident_monitor_harness.exe --child <exit|terminate> <probe.dll> <marker path>
//
// Each scenario prints one line, `<name> | <pass|fail> | <detail>`, and the
// process exits 0 only when every scenario passed.
// tests/test_incident_monitor_behavior.py compiles and runs it.

#include <ForgePact/IncidentMonitor.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
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
                 + Num(e.baselineMs) + " in-hook " + inc::ModName(e.inHook) + "]";
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

// The in-hook tag comes from a real scope on the real accounting: the freeze
// report must name the innermost ForgePact code the frame thread was in.
void FreezeInHook()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    {
        inc::IncidentScope scope(inc::Mod::mapreveal);
        sim.in.inHook = inc::g_Accounting.InHook();
        sim.Stall(4000.0);
    }
    sim.in.inHook = inc::g_Accounting.InHook();
    const bool restored = sim.in.inHook == inc::Mod::none;
    sim.Frames(3.0, 16.7);
    sim.Settle();
    double ended = 0.0;
    const bool sawEnd = sim.detector.TakeFreezeEnded(ended);
    bool ok = restored && sim.episodes.size() == 1 && sim.Count(inc::Kind::freeze) == 1 && sawEnd && ended >= 4000.0;
    std::string line;
    if (ok) {
        line = inc::FreezeLine(sim.episodes[0]);
        ok = sim.episodes[0].inHook == inc::Mod::mapreveal
             && line.rfind("FREEZE 3 s without a frame | in-hook mapreveal", 0) == 0;
    }
    Report("freeze-4s-in-hook", ok, sim.Describe() + " | " + line + " | ended after " + Num(ended) + " ms");
}

void FreezeNoHook()
{
    Sim sim;
    sim.Frames(5.0, 16.7);
    sim.in.inHook = inc::g_Accounting.InHook();
    sim.Stall(4000.0);
    sim.Frames(3.0, 16.7);
    sim.Settle();
    bool ok = sim.episodes.size() == 1 && sim.Count(inc::Kind::freeze) == 1;
    std::string line;
    if (ok) {
        line = inc::FreezeLine(sim.episodes[0]);
        ok = sim.episodes[0].inHook == inc::Mod::none && line.find("| in-hook none") != std::string::npos;
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
        const auto marker = work / "exit-clean.txt";
        std::filesystem::remove(marker, ec);
        std::filesystem::remove(marker.string() + ".armed", ec);
        const DWORD code = RunChild("exit", probe, marker.string());
        const std::string text = ReadAll(marker);
        const bool armed = std::filesystem::exists(marker.string() + ".armed");
        const bool ok = code == 0 && armed && text.find(inc::kCleanShutdownLine) != std::string::npos;
        char b[96];
        std::snprintf(b, sizeof(b), "exit 0x%lX, armed %s, marker %s", code, armed ? "yes" : "no",
                      text.find(inc::kCleanShutdownLine) != std::string::npos ? "written" : "missing");
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
    std::string session;
    bool ok = true;
    std::string detail;

    // Two sessions in out.txt, the earlier one never shut down cleanly.
    ok = ok && inc::PreviousSession(banner + running + "line\r\n" + banner + "now\r\n", "", session)
         && inc::SessionRanMonitor(session) && !inc::SessionEndedCleanly(session) && session.find("now") == std::string::npos;
    detail += ok ? "crash seen" : "crash missed";
    // The same, ended cleanly.
    const bool cleanOk = inc::PreviousSession(banner + running + clean + banner, "", session) && inc::SessionEndedCleanly(session);
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
    freeze.episode.inHook = inc::Mod::mapreveal;
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
    if (argc < 3) {
        std::printf("usage: incident_monitor_harness <probe.dll> <work dir>\n");
        return 2;
    }
    const std::filesystem::path work = argv[2];
    SteadyWithZoneChange();
    SingleHitch();
    Sustained();
    FreezeInHook();
    FreezeNoHook();
    Unfocused();
    RateLimit();
    PerModAccounting();
    ScrubUsername();
    ExitCases(argv[1], work / "exit");
    CrashCheck();
    BundleRetention();
    ReportJson(work / "json");
    std::printf("RESULT %s\n", g_Failures ? "FAIL" : "OK");
    return g_Failures ? 1 : 0;
}
