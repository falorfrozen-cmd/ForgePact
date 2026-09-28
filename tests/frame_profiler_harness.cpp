// Drives the real ForgePact::FrameProfiler against this process's own
// threads. A worker thread plays the game's frame thread: it runs a known call
// chain and calls OnFrame at its frame boundaries, while the profiler samples
// it from another thread. A fake compiled-code table names the chain the way
// the game's YYGMLFuncs table names scripts and object events, so the report
// has to find "Fake_Enemy_obj Step" -> "FakeMiddle" -> "fake_spin()" exactly
// as it would find the game's own code.
//
// Prints one "PASS <label>" / "FAIL <label> ..." line per check, one
// "REPORT <label> <json path>" line per capture for the Python test to read,
// and "RESULT OK" last when nothing failed.

#include <windows.h>

#include <ForgePact/FrameProfiler.hpp>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace fp = ForgePact::FrameProfiler;

static int g_Failures = 0;

static void Result(bool ok, const char* label, const std::string& detail = {})
{
    std::printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", label, detail.empty() ? "" : " ", detail.c_str());
    std::fflush(stdout);
    if (!ok) ++g_Failures;
}

static uint64_t Ms(double ms)
{
    return static_cast<uint64_t>(ms * static_cast<double>(fp::detail::QpcFreq()) / 1000.0);
}

// ---- the known call chain ---------------------------------------------------
// noinline plus work after every call keeps each caller's frame on the stack
// (no tail call turns a frame into a jump).

static volatile uint64_t g_Sink = 0;

extern "C" __declspec(noinline) void FpLeafSpin(uint64_t until)
{
    while (fp::detail::Qpc() < until) g_Sink = g_Sink + 1;
}
extern "C" __declspec(noinline) void FpMiddle(uint64_t until)
{
    FpLeafSpin(until);
    g_Sink = g_Sink ^ 1;
}
extern "C" __declspec(noinline) void FpTop(uint64_t until)
{
    FpMiddle(until);
    g_Sink = g_Sink ^ 2;
}
extern "C" __declspec(noinline) void FpSlowWork(uint64_t until)
{
    FpLeafSpin(until);
    g_Sink = g_Sink ^ 3;
}

// A frame limiter that spins on the clock, like the GameMaker runner's: in
// no table and no built-in list, so its stacks are runtime-only.
extern "C" __declspec(noinline) void FpFrameLimiter(uint64_t until)
{
    while (fp::detail::Qpc() < until) {}
    g_Sink = g_Sink ^ 4;
}

// The fake compiled-code table, laid out like YYGMLFuncs (three pointers a
// row). Typed function pointers make it constant-initialised, so the exe file
// on disk holds the same addresses (relocated) that memory does - which is
// what lets the profiler read a row back after a "mod" swaps its function.
// It is writable for exactly that test. The null rows on both ends are where
// the walk from the anchor must stop.
struct FakeRow {
    const char* name;
    void (*function)(uint64_t);
    const void* variables;
};
static_assert(sizeof(FakeRow) == sizeof(fp::GmlEntry));
static FakeRow g_FakeRows[] = {
    { nullptr, nullptr, nullptr },
    { "gml_Object_Fake_Enemy_obj_Step_0", &FpTop, nullptr },
    { "gml_Script_FakeMiddle", &FpMiddle, nullptr },
    { "gml_Script_FakeSlowWork", &FpSlowWork, nullptr },
    { "gml_Object_Fake_Hud_obj_Draw_64", nullptr, nullptr },
    { nullptr, nullptr, nullptr },
};
static const fp::GmlEntry* const g_FakeTable = reinterpret_cast<const fp::GmlEntry*>(g_FakeRows);

// ---- the stand-in frame thread ----------------------------------------------

enum Mode : int { Chain = 0, Sleeping = 1, HeapChurn = 2, Hitches = 3, SpinWait = 4 };

static std::atomic<int> g_Mode{Chain};
static std::atomic<bool> g_Quit{false};
static std::atomic<bool> g_Ready{false};
static HANDLE g_WorkerHandle = nullptr;
static DWORD g_WorkerId = 0;
static uintptr_t g_WorkerLow = 0, g_WorkerHigh = 0;

static void WorkerMain()
{
    ULONG_PTR lo = 0, hi = 0;
    GetCurrentThreadStackLimits(&lo, &hi);
    g_WorkerLow = lo;
    g_WorkerHigh = hi;
    DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &g_WorkerHandle,
                    THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
    g_WorkerId = GetCurrentThreadId();
    g_Ready = true;
    auto& profiler = fp::Profiler::Instance();
    unsigned frame = 0;
    while (!g_Quit.load()) {
        switch (g_Mode.load()) {
        case Chain:
            FpTop(fp::detail::Qpc() + Ms(16));
            break;
        case Sleeping: {
            const uint64_t until = fp::detail::Qpc() + Ms(16);
            while (fp::detail::Qpc() < until) Sleep(1);
            break;
        }
        case HeapChurn: {
            const uint64_t until = fp::detail::Qpc() + Ms(16);
            while (fp::detail::Qpc() < until) {
                for (int i = 0; i < 64; ++i) {
                    auto* s = new std::string(200, static_cast<char>('a' + (i & 7)));
                    g_Sink = g_Sink + s->size();
                    delete s;
                }
            }
            break;
        }
        case Hitches:
            if ((++frame % 8) == 0) FpSlowWork(fp::detail::Qpc() + Ms(150));
            else FpTop(fp::detail::Qpc() + Ms(16));
            break;
        case SpinWait: {
            // A little real work, then the rest of the frame spun away.
            const uint64_t frameStart = fp::detail::Qpc();
            FpTop(frameStart + Ms(2));
            FpFrameLimiter(frameStart + Ms(16));
            break;
        }
        }
        profiler.OnFrame();
    }
}

// ---- helpers ------------------------------------------------------------------

static std::filesystem::path g_OutDir;

static bool FakeContext(fp::Context& c)
{
    c.room = "Harness_rm";
    c.instances = 321;
    c.monsters = 12;
    return true;
}

static fp::StartParams Params(const std::string& stem, double seconds, unsigned hz)
{
    fp::StartParams p;
    DuplicateHandle(GetCurrentProcess(), g_WorkerHandle, GetCurrentProcess(), &p.frameThread, 0, FALSE,
                    DUPLICATE_SAME_ACCESS);
    p.frameThreadId = g_WorkerId;
    p.stackLow = g_WorkerLow;
    p.stackHigh = g_WorkerHigh;
    p.seconds = seconds;
    p.hz = hz;
    p.gmlAnchor = &g_FakeTable[2];
    p.builtins.push_back({ reinterpret_cast<uintptr_t>(&FpLeafSpin), "fake_spin" });
    p.outDir = g_OutDir;
    p.stem = stem;
    p.toolVersion = "harness";
    p.context = &FakeContext;
    return p;
}

// Waits for the capture to finish and prints its summary. False on timeout,
// which after the watchdog below can only mean the sampler is stuck.
static bool Finish(const char* label, std::vector<std::string>& summary, double timeoutSeconds = 20.0)
{
    auto& profiler = fp::Profiler::Instance();
    const uint64_t deadline = fp::detail::Qpc() + Ms(timeoutSeconds * 1000.0);
    while (profiler.Busy()) {
        if (fp::detail::Qpc() > deadline) return false;
        Sleep(10);
    }
    summary.clear();
    if (!profiler.TakeSummary(summary)) return false;
    for (const auto& l : summary) std::printf("  %s: %s\n", label, l.c_str());
    for (const auto& l : summary) {
        const std::string tag = "frameprof: report ";
        if (l.rfind(tag, 0) == 0) std::printf("REPORT %s %s\n", label, l.substr(tag.size()).c_str());
    }
    std::fflush(stdout);
    return true;
}

static bool AnyContains(const std::vector<std::string>& lines, const std::string& needle)
{
    for (const auto& l : lines) if (l.find(needle) != std::string::npos) return true;
    return false;
}

// A capture that never finishes means the sampler deadlocked against the
// frame thread; there is no recovering from that inside the process.
static DWORD WINAPI Watchdog(LPVOID)
{
    Sleep(150000);
    std::printf("FAIL watchdog the harness did not finish in 150 s (a sampler deadlock?)\n");
    std::fflush(stdout);
    ExitProcess(3);
}

int main(int argc, char** argv)
{
    g_OutDir = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::current_path() / "frameprof-out";
    std::error_code ec;
    std::filesystem::remove_all(g_OutDir, ec);
    std::filesystem::create_directories(g_OutDir, ec);
    CreateThread(nullptr, 0, &Watchdog, nullptr, 0, nullptr);

    std::thread worker(WorkerMain);
    while (!g_Ready.load()) Sleep(1);

    // ---- modules and names, without sampling ----
    fp::ModuleMap modules;
    const bool built = modules.Build(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)));
    Result(built && modules.Game() != nullptr, "modules/build",
           built ? std::to_string(modules.Modules().size()) + " modules" : "no modules");
    {
        const uint64_t key = modules.FunctionKey(reinterpret_cast<uintptr_t>(&FpMiddle) + 3);
        Result(key == reinterpret_cast<uint64_t>(&FpMiddle), "modules/function_key_is_function_start");
        const uint64_t unknown = modules.FunctionKey(0x1000);
        Result((unknown & fp::kTagUnknown) != 0, "modules/unknown_code_is_tagged");
    }
    {
        // A mod swaps a row's function for a hook in its own module: the walk
        // must go past the row, and the row's original address comes back
        // from the exe file on disk.
        g_FakeRows[2].function = reinterpret_cast<void (*)(uint64_t)>(
            GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "SleepEx"));
        fp::Symbolizer swapped;
        const size_t count = swapped.LoadGmlTable(&g_FakeTable[1], modules);
        Result(count == 4, "names/walk_continues_past_a_swapped_row", std::to_string(count) + " entries");
        Result(swapped.GmlSwapped() == 1 && swapped.GmlRestored() == 1, "names/swapped_row_read_back_from_disk",
               std::to_string(swapped.GmlSwapped()) + " swapped, " + std::to_string(swapped.GmlRestored()) + " restored");
        const fp::Symbol mid = swapped.Resolve(modules.FunctionKey(reinterpret_cast<uintptr_t>(&FpMiddle)), modules);
        Result(mid.label == "FakeMiddle", "names/swapped_row_keeps_its_name", mid.label);
        g_FakeRows[2].function = &FpMiddle;
    }
    {
        fp::Symbolizer names;
        const size_t count = names.LoadGmlTable(&g_FakeTable[2], modules);
        Result(count == 4, "names/gml_table_walk_stops_at_both_ends", std::to_string(count) + " entries");
        Result(names.GmlSwapped() == 0, "names/untouched_table_needs_no_disk_read");
        names.AddBuiltins({ { reinterpret_cast<uintptr_t>(&FpLeafSpin), "fake_spin" } });
        const fp::Symbol top = names.Resolve(modules.FunctionKey(reinterpret_cast<uintptr_t>(&FpTop)), modules);
        Result(top.kind == fp::SymKind::GmlEvent && top.label == "Fake_Enemy_obj Step", "names/object_event", top.label);
        const fp::Symbol mid = names.Resolve(modules.FunctionKey(reinterpret_cast<uintptr_t>(&FpMiddle)), modules);
        Result(mid.kind == fp::SymKind::GmlScript && mid.label == "FakeMiddle", "names/script", mid.label);
        const fp::Symbol spin = names.Resolve(modules.FunctionKey(reinterpret_cast<uintptr_t>(&FpLeafSpin)), modules);
        Result(spin.kind == fp::SymKind::Builtin && spin.label == "fake_spin()", "names/builtin", spin.label);
        const auto sleep = reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "SleepEx"));
        const fp::Symbol sys = names.Resolve(modules.FunctionKey(sleep), modules);
        Result(sys.kind == fp::SymKind::Export && sys.name.find("!SleepEx") != std::string::npos, "names/system_export", sys.name);
        const fp::Symbol none = names.Resolve(0x1000 | fp::kTagUnknown, modules);
        Result(none.kind == fp::SymKind::Unknown, "names/unknown_code", none.label);
    }
    {
        fp::SymKind k{};
        Result(fp::GmlLabel("gml_Object_Enemy_Health_Bar_Parent_obj_Draw_64", k) == "Enemy_Health_Bar_Parent_obj Draw GUI",
               "labels/draw_gui");
        Result(fp::GmlLabel("gml_Object_Controller_obj_Step_0", k) == "Controller_obj Step", "labels/step");
        Result(fp::GmlLabel("gml_Object_Controller_obj_Step_1", k) == "Controller_obj Begin Step", "labels/begin_step");
        Result(fp::GmlLabel("gml_Object_Some_obj_Other_12", k) == "Some_obj User Event 2", "labels/user_event");
        Result(fp::GmlLabel("gml_Object_Some_obj_Alarm_2", k) == "Some_obj Alarm 2", "labels/alarm");
        Result(fp::GmlLabel("gml_Script_DrawMinimap", k) == "DrawMinimap" && k == fp::SymKind::GmlScript, "labels/script");
        Result(fp::GmlLabel("gml_RoomCC_Town_01_0_Create", k) == "gml_RoomCC_Town_01_0_Create" && k == fp::SymKind::GmlOther,
               "labels/other_code");
    }
    {
        // Classification, leaf first.
        std::vector<fp::Symbol> syms(6);
        syms[0].module = fp::ModuleKind::System; syms[0].kind = fp::SymKind::Export; syms[0].wait = true;
        syms[1].module = fp::ModuleKind::Graphics; syms[1].kind = fp::SymKind::Unnamed;
        syms[2].module = fp::ModuleKind::Game; syms[2].kind = fp::SymKind::GmlEvent;
        syms[3].module = fp::ModuleKind::Game; syms[3].kind = fp::SymKind::Unnamed;
        syms[4].module = fp::ModuleKind::Mod; syms[4].kind = fp::SymKind::Unnamed;
        syms[5].module = fp::ModuleKind::System; syms[5].kind = fp::SymKind::Export;
        const uint32_t gpuWait[] = { 0, 1, 3, 2 };
        const uint32_t game[] = { 5, 3, 2, 4 };
        const uint32_t idle[] = { 0, 3, 3 };
        const uint32_t mods[] = { 4, 2 };
        const uint32_t hookedGame[] = { 2, 4, 3 };
        const uint32_t runtime[] = { 5, 3 };
        Result(fp::Classify(syms, gpuWait, 4) == fp::Bucket::GpuWait, "classify/gpu_wait");
        Result(fp::Classify(syms, game, 4) == fp::Bucket::Game, "classify/game_code_through_runtime");
        Result(fp::Classify(syms, idle, 3) == fp::Bucket::Idle, "classify/idle");
        Result(fp::Classify(syms, mods, 2) == fp::Bucket::Mods, "classify/mod_leaf");
        Result(fp::Classify(syms, hookedGame, 3) == fp::Bucket::Game, "classify/game_code_under_a_hook");
        Result(fp::Classify(syms, runtime, 2) == fp::Bucket::Runtime, "classify/runtime");
        Result(fp::Classify(syms, nullptr, 0) == fp::Bucket::Unknown, "classify/no_frames");
    }
    {
        // The spinning frame limiter, found from the data.
        std::vector<fp::Symbol> syms(4);
        syms[0].module = fp::ModuleKind::System; syms[0].kind = fp::SymKind::Export;
        syms[0].name = "ntdll.dll!RtlQueryPerformanceCounter";
        syms[1].module = fp::ModuleKind::Game; syms[1].kind = fp::SymKind::Unnamed;   // the limiter
        syms[2].module = fp::ModuleKind::Game; syms[2].kind = fp::SymKind::Unnamed;   // the main loop
        syms[3].module = fp::ModuleKind::Game; syms[3].kind = fp::SymKind::GmlEvent;  // game code
        // Leaf first: a clock read in the limiter, the limiter itself, a clock
        // read under game code.
        const std::vector<uint32_t> flat = { 0, 1, 2, 1, 2, 0, 3, 2 };
        const std::vector<std::pair<uint32_t, uint16_t>> spans = { { 0, 3 }, { 3, 2 }, { 5, 3 } };
        const std::vector<uint64_t> counts = { 60, 30, 10 };
        const uint32_t spin = fp::FindSpinWait(syms, flat, spans, counts, 100);
        Result(spin == 1, "spin/limiter_found", std::to_string(spin));
        Result(fp::IsSpinSample(syms, flat.data() + 0, 3, spin) && fp::IsSpinSample(syms, flat.data() + 3, 2, spin),
               "spin/limiter_and_its_clock_reads_count");
        Result(!fp::IsSpinSample(syms, flat.data() + 5, 3, spin), "spin/clock_read_under_game_code_does_not");
        Result(fp::FindSpinWait(syms, flat, spans, { 0, 0, 10 }, 100) == fp::kNoSymbol,
               "spin/no_runtime_only_clock_reads_no_limiter");
        Result(fp::FindSpinWait(syms, flat, spans, counts, 100000) == fp::kNoSymbol, "spin/under_one_percent_is_ignored");
    }

    auto& profiler = fp::Profiler::Instance();
    std::vector<std::string> summary;

    // ---- a known chain ----
    {
        g_Mode = Chain;
        Sleep(50);
        std::string why;
        const bool started = profiler.Start(Params("chain", 1.0, 1000), why);
        Result(started, "chain/started", why);
        std::string again;
        fp::StartParams second = Params("second", 1.0, 1000);
        HANDLE secondHandle = second.frameThread;
        const bool refused = !profiler.Start(std::move(second), again);
        CloseHandle(secondHandle);
        Result(refused && again.find("already running") != std::string::npos, "start/refuses_while_running", again);
        const bool done = Finish("chain", summary);
        Result(done, "chain/finished");
        Result(AnyContains(summary, "Fake_Enemy_obj Step"), "chain/summary_names_the_event");
        Result(AnyContains(summary, "FakeMiddle"), "chain/summary_names_the_script");
        Result(AnyContains(summary, "fake_spin()"), "chain/summary_names_the_builtin");
        Result(std::filesystem::exists(g_OutDir / "chain.json") && std::filesystem::exists(g_OutDir / "chain.stacks.txt")
                   && std::filesystem::exists(g_OutDir / "chain.txt"),
               "chain/files_written");
        Result(profiler.Status().find("last report") != std::string::npos, "status/idle_names_last_report", profiler.Status());
    }

    // ---- a frame limiter that spins is waiting, not working ----
    {
        g_Mode = SpinWait;
        Sleep(50);
        std::string why;
        Result(profiler.Start(Params("spinning", 1.5, 500), why), "spinning/started", why);
        Result(Finish("spinning", summary), "spinning/finished");
        Result(AnyContains(summary, "the frame limiter spins instead of sleeping"), "spinning/summary_says_so");
    }

    // ---- a sleeping frame thread is idle, not busy ----
    {
        g_Mode = Sleeping;
        Sleep(50);
        std::string why;
        Result(profiler.Start(Params("sleeping", 0.6, 1000), why), "sleeping/started", why);
        Result(Finish("sleeping", summary), "sleeping/finished");
    }

    // ---- no deadlock while the frame thread hammers the heap ----
    {
        g_Mode = HeapChurn;
        Sleep(50);
        std::string why;
        Result(profiler.Start(Params("heap", 1.5, 2000), why), "heap/started", why);
        Result(Finish("heap", summary, 30.0), "heap/finished_without_deadlock");
    }

    // ---- slow frames are attributed to what ran during them ----
    {
        g_Mode = Hitches;
        Sleep(50);
        std::string why;
        Result(profiler.Start(Params("hitches", 2.5, 1000), why), "hitches/started", why);
        Result(Finish("hitches", summary), "hitches/finished");
        Result(AnyContains(summary, "worst frame"), "hitches/summary_names_the_worst_frame");
    }

    // ---- stop ends a capture early ----
    {
        g_Mode = Chain;
        Sleep(50);
        std::string why;
        Result(profiler.Start(Params("stopped", 30.0, 1000), why), "stop/started", why);
        Sleep(300);
        profiler.RequestStop();
        const uint64_t t0 = fp::detail::Qpc();
        const bool done = Finish("stopped", summary);
        const double took = static_cast<double>(fp::detail::Qpc() - t0) * 1000.0 / static_cast<double>(fp::detail::QpcFreq());
        Result(done && took < 5000.0, "stop/ends_early", fp::detail::Fixed(took, 0) + " ms");
    }

    // ---- a frame thread that ends mid-capture ends the capture ----
    {
        HANDLE shortHandle = nullptr;
        DWORD shortId = 0;
        uintptr_t shortLow = 0, shortHigh = 0;
        std::atomic<bool> shortReady{false};
        std::thread shortLived([&] {
            ULONG_PTR lo = 0, hi = 0;
            GetCurrentThreadStackLimits(&lo, &hi);
            shortLow = lo;
            shortHigh = hi;
            DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &shortHandle,
                            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, 0);
            shortId = GetCurrentThreadId();
            shortReady = true;
            FpTop(fp::detail::Qpc() + Ms(200));
        });
        while (!shortReady.load()) Sleep(1);
        fp::StartParams p = Params("ended", 5.0, 1000);
        CloseHandle(p.frameThread);
        p.frameThread = shortHandle;
        p.frameThreadId = shortId;
        p.stackLow = shortLow;
        p.stackHigh = shortHigh;
        std::string why;
        Result(profiler.Start(std::move(p), why), "ended/started", why);
        shortLived.join();
        const uint64_t t0 = fp::detail::Qpc();
        const bool done = Finish("ended", summary);
        const double took = static_cast<double>(fp::detail::Qpc() - t0) * 1000.0 / static_cast<double>(fp::detail::QpcFreq());
        Result(done && took < 4500.0, "ended/finished_early", fp::detail::Fixed(took, 0) + " ms");
        Result(AnyContains(summary, "ended early"), "ended/summary_says_why");
    }

    g_Quit = true;
    worker.join();
    std::printf(g_Failures ? "RESULT FAIL %d\n" : "RESULT OK\n", g_Failures);
    return g_Failures ? 1 : 0;
}
