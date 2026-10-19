// The tracker's journal writer in a DLL of its own, loaded by
// tests/tracker_mod_exit_harness.cpp. No game, Aurie or YYToolkit is
// involved: what is under test is how a DLL that holds the tracker's session
// file open, with lines still buffered, meets ExitProcess.
//
// It holds the core and the writer the way ModuleMain.cpp's tracker adapter
// does: the core on the heap, never deleted, and the writer
// (ForgePact::TrackerJournalFile, compiled from plugin/include) as a plain
// global with no destructor. Nothing here runs at process exit.
//
// ProbeWrite opens the session file in the directory it is given, appends one
// flush's worth of records, then buffers more that are never appended: what
// the game's exit finds when it comes between two flush frames.
//
// ProbeStartControlThread is the positive control's shape: a std::thread with
// static storage, still running at ExitProcess, whose destructor finds it
// joinable and calls std::terminate. The harness must see that exit as an
// abort, or its clean result for the writer would prove nothing.

#include <ForgePact/TrackerMod.hpp>

#include <cstdlib>
#include <thread>

namespace {

ForgePact::TrackerMod* g_Core = nullptr;     // ModuleMain.cpp's shape: new, never deleted
ForgePact::TrackerJournalFile g_File;       // trivially destructible
std::thread g_ControlThread;                // the positive control only

} // namespace

// Opens `<dir>\tracker\session-<pid>-<ms>.ndjson`, appends one batch, and
// leaves `pending` more records buffered. Returns the lines appended, or -1
// when the file could not be opened or written.
extern "C" __declspec(dllexport) int ProbeWrite(const char* dir, int pending) noexcept
{
    try {
        g_Core = new ForgePact::TrackerMod();
        g_Core->SetIdentity(GetCurrentProcessId(), "forgepact-exit-probe");
        g_Core->SetEnabled(true);
        const unsigned long long t0 = 1700000000000ULL;
        g_Core->BeginFrame(t0);
        if (!g_File.Open(dir, GetCurrentProcessId(), t0)) return -1;
        g_Core->RecordBridgeStatus(true, "", "gold=both xp=both kills=both drops=both");
        g_Core->NoteRoom("Act_01_01");
        g_Core->RecordProgress("Sorak");
        g_Core->CountGold(100);
        for (int f = 1; f < (int)ForgePact::TrackerMod::kFlushFrames; ++f) g_Core->BeginFrame(t0 + 11000ULL);
        if (!g_Core->EndFrame()) return -1;
        const std::string batch = g_Core->TakeBatch();
        const bool ok = g_File.Append(batch);
        g_Core->NoteAppend(ok);
        if (!ok) return -1;
        // Buffered, and never appended: the exit comes first.
        for (int i = 0; i < pending; ++i) g_Core->NoteRoom(i % 2 ? "Act_01_02" : "Act_01_03");
        g_Core->CountKill(7);
        return (int)g_Core->Stats().lines;
    } catch (...) {
        return -1;
    }
}

// Buffered bytes not yet appended, so the harness can check there are some.
extern "C" __declspec(dllexport) long long ProbeBuffered() noexcept
{
    return g_Core ? (long long)g_Core->Buffered() : -1;
}

extern "C" __declspec(dllexport) int ProbeStartControlThread() noexcept
{
    try {
        g_ControlThread = std::thread([] { Sleep(INFINITE); });
        return 1;
    } catch (...) {
        return 0;
    }
}

// The abort() this DLL calls; the harness checks it shares the probe's CRT
// (/MD, like the plugin), as tests/coop_thread_exit_harness.cpp does.
extern "C" __declspec(dllexport) void* ProbeAbortFunction() noexcept
{
    return reinterpret_cast<void*>(&std::abort);
}
