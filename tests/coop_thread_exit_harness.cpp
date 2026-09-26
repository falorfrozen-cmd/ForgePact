// How the coop receive thread's owner meets the game's exit. Each case runs in
// a child process of its own, because what is under test is how that process
// ends: tests/test_coop_thread_exit_behavior.py starts one per case and reads
// its exit code. tests/coop_thread_exit_probe.cpp describes the shapes.
//
//   coop_thread_exit_harness --case <name> <probe.dll>
//
// No case ends through Windows Error Reporting. ArmChild ends an abort, a
// std::terminate or an unhandled exception with an exit code of its own, so a
// failing case leaves no dump in %LOCALAPPDATA%\CrashDumps: Windows keeps only
// the newest ten there, and they are evidence.

#include <winsock2.h>
#include <ws2tcpip.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string_view>

namespace {

constexpr UINT kExitClean = 0;
constexpr UINT kExitTerminate = 0x7E1;
constexpr UINT kExitAbort = 0x7E2;
constexpr UINT kExitException = 0x7E3;
constexpr UINT kExitCheckFailed = 0x7E4;

// coop_thread_exit_probe.cpp's shapes and one of its refusals.
constexpr int kShapeCurrent = 0;
constexpr int kShapeStalled = 1;
constexpr int kShapeBeforeFix = 2;
constexpr int kStartRefusedPrevious = -2;

using StartFunction = int (*)(int) noexcept;
using ReceivedFunction = long (*)() noexcept;
using StopFunction = int (*)(unsigned) noexcept;
using AbortFunction = void* (*)() noexcept;

[[noreturn]] void End(const UINT code) noexcept
{
    TerminateProcess(GetCurrentProcess(), code);
    for (;;) Sleep(INFINITE);
}

[[noreturn]] void OnTerminate()
{
    End(kExitTerminate);
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

// The old shape's std::terminate goes on to abort(). With a SIGABRT handler
// installed, abort() raises it before anything else; with _CALL_REPORTFAULT
// cleared, it would not fast-fail into Windows Error Reporting even if the
// handler returned. This process and the probe share one CRT (/MD), so both
// settings reach the probe's abort too; Load proves that before any case runs.
void ArmChild()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    std::set_terminate(&OnTerminate);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    static_cast<void>(std::signal(SIGABRT, &OnAbort));
    SetUnhandledExceptionFilter(&OnUnhandledException);
}

struct Probe {
    StartFunction start;
    ReceivedFunction received;
    StopFunction stop;
};

Probe Load(const wchar_t* path)
{
    const HMODULE module = LoadLibraryW(path);
    CHECK(module != nullptr);
    Probe probe{};
    probe.start = reinterpret_cast<StartFunction>(GetProcAddress(module, "ProbeStart"));
    probe.received = reinterpret_cast<ReceivedFunction>(GetProcAddress(module, "ProbeReceived"));
    probe.stop = reinterpret_cast<StopFunction>(GetProcAddress(module, "ProbeStop"));
    const auto abortFunction = reinterpret_cast<AbortFunction>(GetProcAddress(module, "ProbeAbortFunction"));
    CHECK(probe.start && probe.received && probe.stop && abortFunction);
    // The instrument: the probe's abort() must be this process's own, or
    // OnAbort would never see it and an abort would reach Windows Error
    // Reporting after all.
    CHECK(abortFunction() == reinterpret_cast<void*>(&std::abort));
    return probe;
}

// Sends datagrams to the probe's port until its receive thread has counted one
// more, so the case goes on with that thread running and waiting in recvfrom.
void SendAndWait(const Probe& probe, const int port)
{
    const long before = probe.received();
    const SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK(s != INVALID_SOCKET);
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    to.sin_port = htons(static_cast<u_short>(port));
    const char packet[] = "HSC1";
    for (int attempt = 0; attempt < 250 && probe.received() == before; ++attempt) {
        sendto(s, packet, sizeof(packet), 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
        Sleep(20);
    }
    closesocket(s);
    CHECK(probe.received() > before);
}

[[noreturn]] void RunCase(const std::wstring_view name, const wchar_t* probePath)
{
    ArmChild();
    WSADATA wsa;
    CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    const Probe probe = Load(probePath);

    if (name == L"exit-before-fix" || name == L"exit") {
        const int port = probe.start(name == L"exit" ? kShapeCurrent : kShapeBeforeFix);
        CHECK(port > 0);
        SendAndWait(probe, port);
        // How the game ends. Windows terminates the receive thread, still in
        // recvfrom, and then runs the probe's static destructors.
        ExitProcess(kExitClean);
    }

    if (name == L"stop") {
        // coopstart, coopstop, then both again: the holder is free after a stop.
        for (int round = 0; round < 2; ++round) {
            const int port = probe.start(kShapeCurrent);
            CHECK(port > 0);
            SendAndWait(probe, port);
            CHECK(probe.stop(2000) == 1);
        }
        ExitProcess(kExitClean);
    }

    if (name == L"stop-stalled" || name == L"stop-stalled-retry") {
        const int port = probe.start(kShapeStalled);
        CHECK(port > 0);
        SendAndWait(probe, port);
        // The receive loop leaves recvfrom but takes 3 s to return, so this
        // stop gives up and keeps the thread,
        CHECK(probe.stop(200) == 0);
        // and a coopstart refuses while that thread still runs.
        CHECK(probe.start(kShapeCurrent) == kStartRefusedPrevious);
        if (name == L"stop-stalled") {
            // The game exits with the thread still running.
            ExitProcess(kExitClean);
        }
        // A second coopstop waits the thread out; then coopstart works again.
        CHECK(probe.stop(5000) == 1);
        const int again = probe.start(kShapeCurrent);
        CHECK(again > 0);
        SendAndWait(probe, again);
        CHECK(probe.stop(2000) == 1);
        ExitProcess(kExitClean);
    }

    std::fprintf(stderr, "unknown case %.*ls\n", static_cast<int>(name.size()), name.data());
    std::fflush(stderr);
    End(kExitCheckFailed);
}

} // namespace

int wmain(const int argumentCount, wchar_t* arguments[])
{
    if (argumentCount == 4 && std::wstring_view(arguments[1]) == L"--case")
        RunCase(arguments[2], arguments[3]);
    std::fprintf(stderr, "usage: coop_thread_exit_harness --case <name> <probe.dll>\n");
    return 2;
}
