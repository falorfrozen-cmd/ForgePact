// The coop receive thread's owner in a DLL of its own, loaded by
// tests/coop_thread_exit_harness.cpp. No game, Aurie or YYToolkit is involved:
// what is under test is how a DLL global that owns a thread meets ExitProcess.
//
// ProbeStart and ProbeStop follow CoopStart and CoopStop in ModuleMain.cpp,
// with the thread owned in one of three shapes:
//
//   kShapeCurrent:   a ForgePact::ExitSafeThread with static storage, compiled
//                    from plugin/include, as ModuleMain.cpp now declares
//                    g_CoopRecvThread;
//   kShapeStalled:   the same, with a receive loop that takes 3 s to return
//                    once its socket is closed, so a stop has to give up;
//   kShapeBeforeFix: a std::thread with static storage, as ModuleMain.cpp
//                    declared g_CoopRecvThread before ExitSafeThread.
//
// The receive loop has CoopRecvLoop's shape: a blocking recvfrom on a UDP
// socket, counting what arrives, ending when the stop closes the socket. The
// socket is bound to the loopback address rather than INADDR_ANY, as
// coopstart's is, so a test run never meets the firewall.

#include <winsock2.h>
#include <ws2tcpip.h>

#include <ForgePact/ExitSafeThread.hpp>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <thread>

namespace {

constexpr int kShapeCurrent = 0;
constexpr int kShapeStalled = 1;
constexpr int kShapeBeforeFix = 2;

// ProbeStart's refusals; a started receive thread returns its port instead.
constexpr int kStartRefusedRunning = -1;    // CoopStart: already running
constexpr int kStartRefusedPrevious = -2;   // CoopStart: the previous thread has not ended
constexpr int kStartSocketFailed = -3;
constexpr int kStartThreadFailed = -4;

std::atomic<bool> g_Run{ false };
std::atomic<SOCKET> g_Sock{ INVALID_SOCKET };
std::atomic<bool> g_Stall{ false };
std::atomic<long> g_Received{ 0 };

ForgePact::ExitSafeThread g_RecvThread;   // kShapeCurrent, kShapeStalled
std::thread g_BeforeFixRecvThread;        // kShapeBeforeFix

void RecvLoop() noexcept
{
    while (g_Run.load()) {
        char packet[64];
        sockaddr_in from{};
        int fromLength = sizeof(from);
        const int r = recvfrom(g_Sock.load(), packet, sizeof(packet), 0,
            reinterpret_cast<sockaddr*>(&from), &fromLength);
        if (r > 0) {
            g_Received.fetch_add(1);
        } else if (r == SOCKET_ERROR) {
            const int e = WSAGetLastError();
            if (e == WSAEINTR || e == WSAENOTSOCK || e == WSAEBADF) break;
            if (!g_Run.load()) break;
            Sleep(2);
        }
    }
    if (g_Stall.load()) Sleep(3000);
}

void CloseSocket() noexcept
{
    const SOCKET s = g_Sock.exchange(INVALID_SOCKET);
    if (s != INVALID_SOCKET) closesocket(s);
}

} // namespace

// CoopStart. Returns the loopback port the receive thread listens on, or one of
// the refusals above.
extern "C" __declspec(dllexport) int ProbeStart(const int shape) noexcept
{
    if (g_Run.load()) return kStartRefusedRunning;
    if (shape != kShapeBeforeFix && !g_RecvThread.JoinFor(std::chrono::milliseconds(0)))
        return kStartRefusedPrevious;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return kStartSocketFailed;
    const SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return kStartSocketFailed;
    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int length = sizeof(local);
    if (bind(s, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) == SOCKET_ERROR
        || getsockname(s, reinterpret_cast<sockaddr*>(&local), &length) == SOCKET_ERROR) {
        closesocket(s);
        return kStartSocketFailed;
    }
    g_Sock.store(s);
    g_Stall.store(shape == kShapeStalled);
    g_Run.store(true);
    bool started = false;
    if (shape == kShapeBeforeFix) {
        try {
            g_BeforeFixRecvThread = std::thread(&RecvLoop);
            started = true;
        } catch (...) {
        }
    } else {
        started = g_RecvThread.Start(&RecvLoop);
    }
    if (!started) {
        g_Run.store(false);
        CloseSocket();
        return kStartThreadFailed;
    }
    return ntohs(local.sin_port);
}

// Datagrams the receive thread has counted.
extern "C" __declspec(dllexport) long ProbeReceived() noexcept
{
    return g_Received.load();
}

// CoopStop, for the ExitSafeThread shapes: 1 when the receive thread was
// joined, 0 when it did not end within `timeoutMs` and is still owned.
extern "C" __declspec(dllexport) int ProbeStop(const unsigned timeoutMs) noexcept
{
    g_Run.store(false);
    CloseSocket();
    return g_RecvThread.JoinFor(std::chrono::milliseconds(timeoutMs)) ? 1 : 0;
}

// The abort() this DLL calls. The harness compares it with its own before
// anything can abort: only a CRT shared with the harness (/MD, like the plugin)
// reaches the SIGABRT handler that ends the process quietly, and a CRT of its
// own would hand the abort to Windows Error Reporting instead.
extern "C" __declspec(dllexport) void* ProbeAbortFunction() noexcept
{
    return reinterpret_cast<void*>(&std::abort);
}
