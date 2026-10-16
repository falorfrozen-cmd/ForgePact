// How the tracker's journal writer meets the game's exit. Each case runs in a
// child process of its own, because what is under test is how that process
// ends: tests/test_tracker_mod_exit_behavior.py starts one per case, reads its
// exit code and then reads the session file the probe wrote.
//
//   tracker_mod_exit_harness --case <exit|control> <probe.dll> <directory>
//
//   exit:    the probe appends one batch, buffers more, and the process ends
//            through ExitProcess, as the game does. Expected: exit 0, and the
//            file holds only the whole lines that batch carried.
//   control: the same, with the probe's static std::thread left running.
//            Expected: the harness's abort code, 0x7E2, which proves this
//            harness sees an exit that aborts.
//
// No case ends through Windows Error Reporting: an abort or an unhandled
// exception ends the process with an exit code of its own, so a failing case
// leaves no dump in %LOCALAPPDATA%\CrashDumps (tests/coop_thread_exit_harness.cpp
// is the model). No terminate handler is installed: std::terminate takes its
// default route to abort(), which the SIGABRT handler turns into 0x7E2.

#include <windows.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

namespace {

constexpr UINT kExitClean = 0;
constexpr UINT kExitAbort = 0x7E2;
constexpr UINT kExitException = 0x7E3;
constexpr UINT kExitCheckFailed = 0x7E4;

using WriteFunction = int (*)(const char*, int) noexcept;
using BufferedFunction = long long (*)() noexcept;
using ControlFunction = int (*)() noexcept;
using AbortFunction = void* (*)() noexcept;

[[noreturn]] void End(const UINT code) noexcept
{
    TerminateProcess(GetCurrentProcess(), code);
    for (;;) Sleep(INFINITE);
}

[[noreturn]] void OnAbort(int)
{
    End(kExitAbort);
}

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS*)
{
    End(kExitException);
}

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            std::fprintf(stderr, "check failed, line %d: %s\n", __LINE__, #condition);     \
            std::fflush(stderr);                                                           \
            End(kExitCheckFailed);                                                         \
        }                                                                                  \
    } while (false)

void ArmChild()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    static_cast<void>(std::signal(SIGABRT, &OnAbort));
    SetUnhandledExceptionFilter(&OnUnhandledException);
}

[[noreturn]] void RunCase(const std::wstring_view name, const wchar_t* probePath, const wchar_t* directory)
{
    ArmChild();
    const HMODULE module = LoadLibraryW(probePath);
    CHECK(module != nullptr);
    const auto write = reinterpret_cast<WriteFunction>(GetProcAddress(module, "ProbeWrite"));
    const auto buffered = reinterpret_cast<BufferedFunction>(GetProcAddress(module, "ProbeBuffered"));
    const auto control = reinterpret_cast<ControlFunction>(GetProcAddress(module, "ProbeStartControlThread"));
    const auto abortFunction = reinterpret_cast<AbortFunction>(GetProcAddress(module, "ProbeAbortFunction"));
    CHECK(write && buffered && control && abortFunction);
    // The instrument: the probe's abort() must be this process's own, or
    // OnAbort would never see it.
    CHECK(abortFunction() == reinterpret_cast<void*>(&std::abort));

    char narrow[MAX_PATH] = {};
    CHECK(WideCharToMultiByte(CP_ACP, 0, directory, -1, narrow, MAX_PATH, nullptr, nullptr) > 0);
    const int appended = write(narrow, 25);
    CHECK(appended > 0);
    CHECK(buffered() > 0);
    if (name == L"control") CHECK(control() == 1);
    else CHECK(name == L"exit");
    std::printf("appended=%d\n", appended);
    std::fflush(stdout);
    // How the game ends: Windows ends every other thread, then runs the
    // probe's static destructors.
    ExitProcess(kExitClean);
}

} // namespace

int wmain(const int argumentCount, wchar_t* arguments[])
{
    if (argumentCount == 5 && std::wstring_view(arguments[1]) == L"--case")
        RunCase(arguments[2], arguments[3], arguments[4]);
    std::fprintf(stderr, "usage: tracker_mod_exit_harness --case <exit|control> <probe.dll> <directory>\n");
    return 2;
}
