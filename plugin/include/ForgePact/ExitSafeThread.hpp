#pragma once

// A background thread owned by a module global, with no destructor.
//
// Hero Siege ends through ExitProcess. Windows terminates every other thread
// first and only then runs each DLL's DLL_PROCESS_DETACH, where the CRT
// destroys this DLL's static objects. A std::thread whose thread was
// terminated that way still reports joinable(), and destroying a joinable
// std::thread calls std::terminate: the exit ends in ucrtbase!abort
// (0xc0000409, fast-fail 7) with a Windows Error Reporting dump. Nothing can
// join the thread before that: Aurie runs no module code at process exit
// (AurieCore's DllMain returns at once when the process is ending), and this
// module cannot have a DllMain of its own, because Aurie's shared.hpp defines
// it.
//
// So the std::thread lives on the heap, and the holder frees it only after a
// successful join. The holder is trivially destructible, so the CRT has no
// destructor to run for it at exit, and the OS reclaims the rest.
//
// Not synchronised: one thread owns the holder and makes every Start and
// JoinFor call. For coop that is the game's frame thread: FrameCallback runs
// both the IPC commands and the research build's coop.ini auto-start.
//
// HS-Offline-Tracker's producer aborted the game's exit this way; its fix
// (HS-Offline-Tracker PR #8, aurie-producer/include/hsot_aurie/
// exit_safe_thread.h) is the same shape.
// Game-independent: tests/coop_thread_exit_probe.cpp compiles this header
// whole.

#include <windows.h>

#include <chrono>
#include <thread>
#include <type_traits>
#include <utility>

namespace ForgePact {

class ExitSafeThread final {
public:
    // Starts `routine` on a new thread. False when this holder still owns a
    // thread (JoinFor has not freed it yet) or the thread could not be
    // created.
    [[nodiscard]] bool Start(void (*routine)() noexcept) noexcept
    {
        if (thread_) return false;
        try {
            thread_ = new std::thread(routine);
            return true;
        } catch (...) {
            return false;
        }
    }

    // Waits up to `timeout` for the thread, which the caller has already told
    // to stop, then joins and frees it. True when no thread is left (joined
    // here, or none was started). False when it is still running: it is left
    // alone, because a joinable std::thread must never be destroyed, and the
    // caller must not start another thread that shares its state until a
    // later call returns true. A zero timeout only asks whether it has ended.
    [[nodiscard]] bool JoinFor(std::chrono::milliseconds timeout) noexcept
    {
        if (!thread_) return true;
        const long long ms = timeout.count() < 0 ? 0 : timeout.count();
        if (WaitForSingleObject(thread_->native_handle(), static_cast<DWORD>(ms)) != WAIT_OBJECT_0)
            return false;
        try {
            thread_->join();
        } catch (...) {
            return false;
        }
        delete std::exchange(thread_, nullptr);
        return true;
    }

private:
    std::thread* thread_ = nullptr;
};

static_assert(std::is_trivially_destructible_v<ExitSafeThread>,
    "a module global must not end a thread in its destructor; see ExitSafeThread");

} // namespace ForgePact
