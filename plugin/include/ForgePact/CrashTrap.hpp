#pragma once

// ForgePact::CrashTrap - what the game was doing when it crashed (issue #223).
//
// The incident monitor finds a crash only at the next load, from a missing
// clean-shutdown line, and until now a crash report said "unknown" for every
// question that mattered. The trap writes the answer at the crash, from
// inside the dying process, into bp_ipc\crashtrap\, and the next load folds it
// into the crash report (IncidentMonitor.hpp's ParseTrapRecord):
//
//   * Two routes. A first-registered vectored handler sees every exception
//     first. For a fatal code (an access violation, a stack overflow, heap
//     corruption and the rest of IsFatal's set) it appends one line, at most
//     kMaxFirstChanceLines a session; for an MSVC C++ throw it writes nothing
//     and only remembers the thrown type for its thread. The top-level
//     unhandled-exception filter sees only what nothing handled: it appends a
//     `route=unhandled` line (a C++ exception's with its type), has the dump
//     and the two log copies made, then calls the filter it replaced and
//     returns that one's answer. Whoever sets the top-level filter last owns
//     it, so whether ours still does is measured (CheckUnhandled, from the
//     status line and the monitor thread): a filter set over ours is named in
//     trap.txt and the status line, and ours is re-armed on top of it, a
//     bounded number of times. Neither route handles or resumes anything:
//     the vectored handler always continues the search, so the game dies
//     exactly as before and Windows Error Reporting still sees it.
//   * The line is `exception 0x<code> at <module>+0x<offset> thread=<game|
//     other> tid=<n> in-hook=<id|none> in-mod=<mod> game-original=<yes|no>
//     cpp=<type|none> route=<first-chance|unhandled>`. The tags are the
//     incident monitor's: which ForgePact hook and which mod the thread was
//     in. Module names come from a list made outside the handler (at install,
//     and refreshed by the monitor thread); the handler never asks the loader,
//     whose lock a dying thread may hold.
//   * Inside either route: Win32 calls and fixed buffers only, no allocation,
//     no stream, no lock of ours, nothing from the game's runtime. The file is
//     opened, appended and closed for each line, so the line is on disk before
//     the search goes on.
//   * The dump: MiniDumpNormal only (the threads' stacks and the module list,
//     no heap), once a session, from the unhandled route only. ForgePact
//     itself raises first-chance access violations that are handled
//     (IsBadReadPtr, the frame profiler's __try), so a dump at first chance
//     would dump sessions that never crashed. A helper thread created at
//     install writes it: MiniDumpWriteDump should not run on the thread that
//     crashed, and that thread may be out of stack. The filter waits for the
//     helper at most kDumpWaitMs, so a dump that hangs (the dying thread may
//     hold a lock the dump needs) cannot keep a dead game alive. dbghelp.dll
//     is loaded at install from the system directory and MiniDumpWriteDump
//     taken by name: build.bat's link line names user32.lib only. Without it,
//     only the dump is off, and Install's line says so.
//   * The log copies: the game's YYToolkit.log and aurie.log hold one session
//     each and are rewritten at every launch, so at the next load they
//     describe the new session. The helper thread copies their last
//     kLogCopyBytes into the trap's folder at the crash, before the dump.
//   * Rotation: at install, before anything is written, bp_ipc\crashtrap\
//     becomes bp_ipc\crashtrap.prev\ (an older .prev is removed) and a fresh
//     folder starts with a session line. The next-load check reads .prev.
//
// Game-independent: Win32 and the standard library, and the incident
// monitor's accounting for the two tags. tests/crash_trap_harness.cpp
// compiles it whole. Avoids the two-argument min/max of the standard library:
// ModuleMain.cpp includes <windows.h> without NOMINMAX.

#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>

#include "IncidentMonitor.hpp"

namespace ForgePact::CrashTrap {

static_assert(sizeof(void*) == 8, "the C++ throw information is read with the x64 layout");

// ---- names and limits ----------------------------------------------------------

inline constexpr char kDirName[] = "crashtrap";             // under bp_ipc\, this session's
inline constexpr char kPrevDirName[] = "crashtrap.prev";    // the previous session's, read at the next load
inline constexpr char kRecordFile[] = "trap.txt";
inline constexpr char kDumpFile[] = "crash.dmp";
inline constexpr char kYytkLogFile[] = "YYToolkit.log";     // the source's name, and its copy's
inline constexpr char kAurieLogFile[] = "aurie.log";

inline constexpr DWORD kCppExceptionCode = 0xE06D7363;      // an MSVC C++ throw ("msc")
inline constexpr DWORD kHeapCorruption = 0xC0000374;        // STATUS_HEAP_CORRUPTION (ntstatus.h)
inline constexpr long kMaxFirstChanceLines = 32;            // a session; counted on after the cap
inline constexpr DWORD kDumpWaitMs = 15000;                 // the filter waits this long for the dump, at most
inline constexpr DWORD kLogCopyBytes = 1024 * 1024;         // the tail of each log the helper copies
inline constexpr DWORD kModuleRefreshMs = 10000;            // how often the monitor thread re-lists the modules
inline constexpr int kMaxModules = 512;
inline constexpr size_t kModuleNameChars = 64;
inline constexpr size_t kTypeChars = 128;                   // a C++ type's decorated name, cut to this
inline constexpr size_t kLineChars = 512;
inline constexpr long kMaxFilterChain = 8;                  // the filter ours replaced at install, and each one re-armed over
inline constexpr size_t kFilterNameChars = kModuleNameChars + 32;   // "<module>+0x<offset>" or "unlisted 0x<address>"

// ---- state: plain data and atomics, zero at load, nothing to destroy -----------

struct ModuleRow {
    uintptr_t base;
    uintptr_t end;
    char name[kModuleNameChars];
};

struct ModuleTable {
    ModuleRow rows[kMaxModules];
    int count;
};

using MiniDumpWriteDumpFn = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                          PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

struct State {
    char record[MAX_PATH];
    char dump[MAX_PATH];
    char logSource[2][MAX_PATH];
    char logCopy[2][MAX_PATH];
    char dumpWhy[96];                          // why the dump is off; empty when it is ready
    std::atomic<bool> installed;
    std::atomic<bool> firstChanceArmed;
    std::atomic<bool> unhandledArmed;
    std::atomic<bool> dumpReady;
    std::atomic<DWORD> gameThread;
    PVOID vectored;
    LPTOP_LEVEL_EXCEPTION_FILTER previous;     // the filter ours replaced, called after ours
    LPTOP_LEVEL_EXCEPTION_FILTER chain[kMaxFilterChain];   // [0] the one replaced at install, then each re-armed over
    volatile long chainCount;
    std::atomic<bool> checking;                // one CheckUnhandled at a time
    std::atomic<bool> filterLeft;              // found replaced with the chain full: the other filter stays on top
    volatile long filterRearmed;               // times ours was found replaced and put back on top
    char replacedBy[kFilterNameChars];         // the filter found on top instead of ours, last time
    MiniDumpWriteDumpFn writeDump;
    HANDLE request;                            // the filter sets it...
    HANDLE done;                               // ...and the helper sets this when it finished
    HANDLE helper;
    PEXCEPTION_POINTERS volatile pending;
    volatile DWORD pendingThread;
    volatile long firstChanceLines;            // written, under the cap
    volatile long firstChanceOver;             // not written: the cap was reached
    volatile long unhandledSeen;
    volatile long dumpWritten;
    volatile long dumpFailed;
    volatile long logsCopied;
    ModuleTable tables[2];
    std::atomic<int> active;                   // which table the handler reads
};

inline constinit State g_State{};
static_assert(std::is_trivially_destructible_v<State>, "nothing may run for the trap at exit");

// This thread's last C++ throw, and whether this thread is inside a route (an
// exception raised inside one, such as the guarded read below, is not
// recorded again).
inline thread_local char t_LastCppType[kTypeChars];
inline thread_local bool t_Inside;

// The exception the filter last ran for on this thread, and how many times it
// was handed back to ours for it (see Unhandled).
inline thread_local const EXCEPTION_RECORD* t_FilterRecord;
inline thread_local long t_FilterDepth;

// ---- fixed-buffer text, for the routes -------------------------------------------

inline int Put(char* buf, int pos, int cap, const char* text, size_t limit = static_cast<size_t>(-1)) noexcept
{
    for (size_t i = 0; text && i < limit && text[i] && pos < cap - 1; ++i) buf[pos++] = text[i];
    return pos;
}

inline int PutHex(char* buf, int pos, int cap, unsigned long long value, int minDigits) noexcept
{
    char digits[16];
    int n = 0;
    do { digits[n++] = "0123456789ABCDEF"[value & 0xF]; value >>= 4; } while (value && n < 16);
    while (n < minDigits && n < 16) digits[n++] = '0';
    while (n > 0 && pos < cap - 1) buf[pos++] = digits[--n];
    return pos;
}

inline int PutDec(char* buf, int pos, int cap, unsigned long long value) noexcept
{
    char digits[20];
    int n = 0;
    do { digits[n++] = static_cast<char>('0' + value % 10); value /= 10; } while (value && n < 20);
    while (n > 0 && pos < cap - 1) buf[pos++] = digits[--n];
    return pos;
}

inline void CopyText(char* to, size_t cap, const char* from) noexcept
{
    size_t i = 0;
    for (; from && i + 1 < cap && from[i]; ++i) to[i] = from[i];
    to[i] = '\0';
}

// ---- what a route reads -------------------------------------------------------------

// The fatal codes: the research build's crash instrument logs the same set,
// heap corruption included, plus an in-page error (a mapped file that failed).
inline bool IsFatal(DWORD code) noexcept
{
    return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_STACK_OVERFLOW
        || code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_PRIV_INSTRUCTION
        || code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_ARRAY_BOUNDS_EXCEEDED
        || code == EXCEPTION_IN_PAGE_ERROR || code == kHeapCorruption;
}

// The module an address is in, from the list made outside the handler.
inline const ModuleRow* FindModule(uintptr_t address) noexcept
{
    const ModuleTable& table = g_State.tables[g_State.active.load(std::memory_order_acquire) & 1];
    const int count = table.count < kMaxModules ? table.count : kMaxModules;
    for (int i = 0; i < count; ++i)
        if (address >= table.rows[i].base && address < table.rows[i].end) return &table.rows[i];
    return nullptr;
}

// The MSVC x64 throw information, as the compiler lays it out: every field
// after the first is an offset from the throwing module's image base, which
// the exception record carries as its fourth parameter.
struct ThrowInfo64 {
    uint32_t attributes;
    int32_t unwind;
    int32_t forwardCompat;
    int32_t catchableTypes;    // -> CatchableTypes64
};
struct CatchableTypes64 {
    int32_t count;
    int32_t types[1];          // -> CatchableType64, the most derived type first
};
struct CatchableType64 {
    uint32_t properties;
    int32_t typeDescriptor;    // -> the type descriptor, whose name starts 16 bytes in
};
inline constexpr size_t kTypeNameOffset = 2 * sizeof(void*);

// A C++ throw's decorated type name (".?AVruntime_error@std@@"), read under
// __try: the pointers belong to another module and may be anything. No C++
// object lives in this function, as __try requires.
inline bool ReadCppType(const EXCEPTION_RECORD* record, char* out, size_t cap) noexcept
{
    if (!record || record->ExceptionCode != kCppExceptionCode || record->NumberParameters < 4 || cap < 2) return false;
    const ULONG_PTR magic = record->ExceptionInformation[0];
    if (magic != 0x19930520 && magic != 0x19930521 && magic != 0x19930522) return false;
    const ULONG_PTR throwInfo = record->ExceptionInformation[2];
    const ULONG_PTR image = record->ExceptionInformation[3];
    if (!throwInfo || !image) return false;   // a bare `throw;` raises none: the thread's last type stands
    bool ok = false;
    __try {
        const auto* info = reinterpret_cast<const ThrowInfo64*>(throwInfo);
        const auto* types = reinterpret_cast<const CatchableTypes64*>(image + info->catchableTypes);
        if (types->count > 0) {
            const auto* type = reinterpret_cast<const CatchableType64*>(image + types->types[0]);
            const char* name = reinterpret_cast<const char*>(image + type->typeDescriptor + kTypeNameOffset);
            size_t i = 0;
            for (; i + 1 < cap && name[i]; ++i) out[i] = name[i];
            out[i] = '\0';
            ok = i > 0;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        ok = false;
    }
    return ok;
}

// Appends one line, open-append-close, so it is on disk before this returns.
inline bool AppendRecord(const char* text, int len) noexcept
{
    if (len <= 0 || !g_State.record[0]) return false;
    const HANDLE file = CreateFileA(g_State.record, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(len), &written, nullptr);
    CloseHandle(file);
    return true;
}

// The record line, from a fixed buffer.
inline void WriteRecord(const EXCEPTION_RECORD* record, bool unhandled) noexcept
{
    const uintptr_t address = reinterpret_cast<uintptr_t>(record->ExceptionAddress);
    const ModuleRow* module = FindModule(address);
    const char* hookId = Incident::g_Accounting.InHookId();
    const Incident::InModState inMod = Incident::g_Accounting.InModNow();
    char type[kTypeChars];
    type[0] = '\0';
    if (unhandled && record->ExceptionCode == kCppExceptionCode && !ReadCppType(record, type, sizeof(type)))
        CopyText(type, sizeof(type), t_LastCppType);

    char line[kLineChars];
    const int cap = static_cast<int>(sizeof(line));
    int pos = Put(line, 0, cap, "exception 0x");
    pos = PutHex(line, pos, cap, record->ExceptionCode, 8);
    pos = Put(line, pos, cap, " at ");
    pos = Put(line, pos, cap, module ? module->name : "unlisted", kModuleNameChars);
    pos = Put(line, pos, cap, "+0x");
    pos = PutHex(line, pos, cap, module ? address - module->base : address, 1);
    pos = Put(line, pos, cap, " thread=");
    pos = Put(line, pos, cap, GetCurrentThreadId() == g_State.gameThread.load(std::memory_order_relaxed) ? "game" : "other");
    pos = Put(line, pos, cap, " tid=");
    pos = PutDec(line, pos, cap, GetCurrentThreadId());
    pos = Put(line, pos, cap, " in-hook=");
    pos = Put(line, pos, cap, hookId && *hookId ? hookId : "none", Incident::kHookIdChars);
    pos = Put(line, pos, cap, " in-mod=");
    pos = Put(line, pos, cap, Incident::ModName(inMod.mod));
    pos = Put(line, pos, cap, inMod.gameOriginal ? " game-original=yes" : " game-original=no");
    pos = Put(line, pos, cap, " cpp=");
    pos = Put(line, pos, cap, type[0] ? type : "none");
    pos = Put(line, pos, cap, unhandled ? " route=unhandled" : " route=first-chance");
    line[pos++] = '\n';
    AppendRecord(line, pos);
}

// ---- the two routes ----------------------------------------------------------------------

// The vectored route. Always continues the search.
inline LONG CALLBACK FirstChance(PEXCEPTION_POINTERS info) noexcept
{
    if (!info || !info->ExceptionRecord || t_Inside) return EXCEPTION_CONTINUE_SEARCH;
    t_Inside = true;
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    if (record->ExceptionCode == kCppExceptionCode) {
        char type[kTypeChars];
        if (ReadCppType(record, type, sizeof(type))) CopyText(t_LastCppType, sizeof(t_LastCppType), type);
    } else if (IsFatal(record->ExceptionCode)) {
        if (InterlockedIncrement(&g_State.firstChanceLines) <= kMaxFirstChanceLines) {
            WriteRecord(record, false);
        } else {
            InterlockedDecrement(&g_State.firstChanceLines);
            InterlockedIncrement(&g_State.firstChanceOver);
        }
    }
    t_Inside = false;
    return EXCEPTION_CONTINUE_SEARCH;
}

// Hands the crash to the helper thread and waits for it, at most kDumpWaitMs.
inline void RequestDump(PEXCEPTION_POINTERS info) noexcept
{
    if (!g_State.helper || !g_State.request || !g_State.done) return;
    g_State.pending = info;
    g_State.pendingThread = GetCurrentThreadId();
    SetEvent(g_State.request);
    WaitForSingleObject(g_State.done, kDumpWaitMs);
}

// The top-level filter: what nothing else handled. Records it once, has the
// logs copied and the dump written, then gives the previous filter its turn
// and returns that one's answer.
//
// Ours can be handed the same exception again: when CheckUnhandled re-armed
// ours over a filter that had kept ours as the one it calls next. Each such
// return goes one step down the chain, to the filter ours was on top of
// before, and past the bottom the search goes on, so the chain never loops.
inline LONG WINAPI Unhandled(PEXCEPTION_POINTERS info) noexcept
{
    const EXCEPTION_RECORD* const exception = info ? info->ExceptionRecord : nullptr;
    const long depth = exception && t_FilterRecord == exception ? t_FilterDepth + 1 : 0;
    t_FilterRecord = exception;
    t_FilterDepth = depth;
    if (depth == 0 && exception && InterlockedExchange(&g_State.unhandledSeen, 1) == 0) {
        t_Inside = true;
        WriteRecord(info->ExceptionRecord, true);
        RequestDump(info);
        t_Inside = false;
    }
    const long at = g_State.chainCount - 1 - depth;
    const LPTOP_LEVEL_EXCEPTION_FILTER previous = depth == 0 ? g_State.previous
                                                : at >= 0 && at < kMaxFilterChain ? g_State.chain[at] : nullptr;
    if (previous) return previous(info);
    return EXCEPTION_CONTINUE_SEARCH;
}

// ---- the helper thread: the log copies and the dump ------------------------------------

// The last kLogCopyBytes of a log another module may hold open for writing.
inline bool CopyLogTail(const char* from, const char* to) noexcept
{
    if (!from[0] || !to[0]) return false;
    const HANDLE source = CreateFileA(from, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (source == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    if (GetFileSizeEx(source, &size) && size.QuadPart > static_cast<LONGLONG>(kLogCopyBytes)) {
        LARGE_INTEGER start{};
        start.QuadPart = size.QuadPart - kLogCopyBytes;
        SetFilePointerEx(source, start, nullptr, FILE_BEGIN);
    }
    const HANDLE target = CreateFileA(to, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (target == INVALID_HANDLE_VALUE) {
        CloseHandle(source);
        return false;
    }
    static char buffer[64 * 1024];   // the helper's only: one copy runs at a time
    DWORD total = 0, got = 0;
    while (total < kLogCopyBytes && ReadFile(source, buffer, sizeof(buffer), &got, nullptr) && got) {
        DWORD put = 0;
        if (!WriteFile(target, buffer, got, &put, nullptr) || put != got) break;
        total += got;
    }
    CloseHandle(target);
    CloseHandle(source);
    return true;
}

inline bool WriteDump() noexcept
{
    if (!g_State.dumpReady.load() || !g_State.writeDump || !g_State.dump[0]) return false;
    const HANDLE file = CreateFileA(g_State.dump, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    MINIDUMP_EXCEPTION_INFORMATION exception{};
    exception.ThreadId = g_State.pendingThread;
    exception.ExceptionPointers = g_State.pending;
    exception.ClientPointers = FALSE;
    const BOOL ok = g_State.writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
                                      g_State.pending ? &exception : nullptr, nullptr, nullptr);
    CloseHandle(file);
    if (!ok) DeleteFileA(g_State.dump);
    return ok != FALSE;
}

inline DWORD WINAPI Helper(LPVOID) noexcept
{
    if (WaitForSingleObject(g_State.request, INFINITE) != WAIT_OBJECT_0) return 0;
    for (int i = 0; i < 2; ++i)
        if (CopyLogTail(g_State.logSource[i], g_State.logCopy[i])) InterlockedIncrement(&g_State.logsCopied);
    if (g_State.dumpReady.load()) InterlockedIncrement(WriteDump() ? &g_State.dumpWritten : &g_State.dumpFailed);
    SetEvent(g_State.done);
    return 0;
}

// ---- outside the routes: install, the module list, the game thread ---------------------

// "<module>+0x<offset>" for a filter's address, "unlisted 0x<address>" when
// no listed module holds it, "none" for no filter at all.
inline void DescribeFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter, char* out, size_t cap) noexcept
{
    const int limit = static_cast<int>(cap);
    int pos = 0;
    const uintptr_t address = reinterpret_cast<uintptr_t>(filter);
    const ModuleRow* module = filter ? FindModule(address) : nullptr;
    if (!filter) {
        pos = Put(out, pos, limit, "none");
    } else if (module) {
        pos = Put(out, pos, limit, module->name);
        pos = Put(out, pos, limit, "+0x");
        pos = PutHex(out, pos, limit, address - module->base, 1);
    } else {
        pos = Put(out, pos, limit, "unlisted 0x");
        pos = PutHex(out, pos, limit, address, 1);
    }
    out[pos] = '\0';
}

// Whether ours is still the process's top-level filter, measured, not
// remembered. That filter belongs to the process, and whoever sets it last
// owns it: the exe's C runtime or the runner may set its own after Install,
// and the MSVC runtime's own filter ends a C++ exception without calling any
// other. Then the unhandled route never runs, no dump is written and no log
// copied, and nothing else would say so.
//
// Setting ours hands back the filter in place. When that was another one (or
// none), ours stays on top and calls that one after its own work, as it does
// the one Install found; the replacement is counted, named and written to
// trap.txt as a `filter ` line, so a crash folder says it too (the report's
// parser reads `exception ` lines only). Once ours was re-armed
// kMaxFilterChain - 1 times, the other filter is put back and left on top, and
// the status line says that instead. One check at a time; the status line and
// the monitor thread's module refresh call it.
inline void CheckUnhandled() noexcept
{
    State& s = g_State;
    if (!s.unhandledArmed.load() || s.checking.exchange(true)) return;
    const LPTOP_LEVEL_EXCEPTION_FILTER found = SetUnhandledExceptionFilter(&Unhandled);
    if (found == &Unhandled) {
        s.filterLeft.store(false);
        s.checking.store(false);
        return;
    }
    char who[kFilterNameChars];
    DescribeFilter(found, who, sizeof(who));
    char line[kLineChars];
    const int cap = static_cast<int>(sizeof(line));
    int pos = Put(line, 0, cap, "filter replaced by ");
    pos = Put(line, pos, cap, who);
    const long count = s.chainCount;
    if (count < kMaxFilterChain) {
        s.chain[count] = found;
        InterlockedExchange(&s.chainCount, count + 1);
        s.previous = found;
        const long times = InterlockedIncrement(&s.filterRearmed);
        s.filterLeft.store(false);
        CopyText(s.replacedBy, sizeof(s.replacedBy), who);
        pos = Put(line, pos, cap, ", ours re-armed over it (");
        pos = PutDec(line, pos, cap, static_cast<unsigned long long>(times));
        pos = Put(line, pos, cap, ")\n");
        AppendRecord(line, pos);
    } else {
        SetUnhandledExceptionFilter(found);
        const bool changed = !s.filterLeft.exchange(true) || std::strcmp(s.replacedBy, who) != 0;
        CopyText(s.replacedBy, sizeof(s.replacedBy), who);
        if (changed) {
            pos = Put(line, pos, cap, ", left on top: ours was re-armed ");
            pos = PutDec(line, pos, cap, static_cast<unsigned long long>(s.filterRearmed));
            pos = Put(line, pos, cap, " times already\n");
            AppendRecord(line, pos);
        }
    }
    s.checking.store(false);
}

// Re-lists the process's modules into the table the handler is not reading,
// then switches it over, and then checks that ours is still the top-level
// filter (a module listed just now can be named). One writer: Install, then
// the monitor thread.
inline void RefreshModules() noexcept
{
    HMODULE modules[kMaxModules] = {};
    DWORD needed = 0;
    const HANDLE process = GetCurrentProcess();
    if (!K32EnumProcessModules(process, modules, sizeof(modules), &needed)) {
        CheckUnhandled();
        return;
    }
    const int next = (g_State.active.load(std::memory_order_relaxed) & 1) ^ 1;
    ModuleTable& table = g_State.tables[next];
    int listed = 0;
    const int count = static_cast<int>(needed / sizeof(HMODULE)) < kMaxModules ? static_cast<int>(needed / sizeof(HMODULE))
                                                                                : kMaxModules;
    for (int i = 0; i < count; ++i) {
        MODULEINFO info{};
        char path[MAX_PATH] = {};
        if (!modules[i] || !K32GetModuleInformation(process, modules[i], &info, sizeof(info))
            || !GetModuleFileNameA(modules[i], path, MAX_PATH)) continue;
        const char* slash = std::strrchr(path, '\\');
        ModuleRow& row = table.rows[listed++];
        row.base = reinterpret_cast<uintptr_t>(info.lpBaseOfDll);
        row.end = row.base + info.SizeOfImage;
        CopyText(row.name, sizeof(row.name), slash ? slash + 1 : path);
    }
    table.count = listed;
    g_State.active.store(next, std::memory_order_release);
    CheckUnhandled();
}

// The frame thread, every frame: `thread=game` means this one.
inline void NoteGameThread(DWORD threadId) noexcept
{
    g_State.gameThread.store(threadId, std::memory_order_relaxed);
}

// This thread's last thrown C++ type, decorated, or "unknown". Never null.
inline const char* LastCppType() noexcept
{
    return t_LastCppType[0] ? t_LastCppType : "unknown";
}

// The previous session's folder out of the way, this one's made fresh.
inline bool Rotate(const std::filesystem::path& dir, const std::filesystem::path& prev)
{
    std::error_code ec;
    std::filesystem::remove_all(prev, ec);
    if (std::filesystem::exists(dir, ec)) {
        std::filesystem::rename(dir, prev, ec);
        if (ec) std::filesystem::remove_all(dir, ec);   // a session's leftovers never mix with the next
    }
    std::error_code made;
    std::filesystem::create_directories(dir, made);
    return !made && std::filesystem::is_directory(dir, made);
}

// The unhandled part says what CheckUnhandled just measured: ours on top, or
// which filter is on top instead.
inline std::string StatusLine()
{
    CheckUnhandled();
    const State& s = g_State;
    std::string unhandled;
    if (!s.unhandledArmed.load()) {
        unhandled = "unhandled not armed";
    } else if (s.filterLeft.load()) {
        unhandled = std::string("unhandled replaced by ") + s.replacedBy + ", left on top (ours re-armed "
                    + std::to_string(s.filterRearmed) + " times already)";
    } else {
        unhandled = "unhandled armed";
        unhandled += s.filterRearmed ? " (top-level, re-armed " + std::to_string(s.filterRearmed) + "x, last over "
                                           + s.replacedBy + ")"
                                     : std::string(" (top-level)");
    }
    std::string dump;
    if (s.dumpWritten) dump = "dump written";
    else if (s.dumpFailed) dump = "dump failed";
    else if (s.dumpReady.load()) dump = "dump ready";
    else dump = std::string("dump off - ") + (s.dumpWhy[0] ? s.dumpWhy : "not installed");
    const int modules = s.tables[s.active.load() & 1].count;
    return std::string("incident: trap ") + (s.firstChanceArmed.load() ? "first-chance armed" : "first-chance not armed")
           + " | " + unhandled + " | " + dump + " | lines "
           + std::to_string(s.firstChanceLines) + " of " + std::to_string(kMaxFirstChanceLines)
           + (s.firstChanceOver ? " (" + std::to_string(s.firstChanceOver) + " after the cap)" : std::string())
           + " | modules " + std::to_string(modules) + " | bp_ipc\\" + kDirName;
}

// Once a process: rotates bp_ipc\crashtrap\, starts its record, lists the
// modules, loads dbghelp.dll from the system directory, starts the helper and
// arms both routes. `ipcDir` is bp_ipc, `logDir` the folder holding
// YYToolkit.log and aurie.log (the game's). Returns its line for out.txt.
inline std::string Install(const std::string& ipcDir, const std::string& logDir)
{
    if (g_State.installed.exchange(true)) return StatusLine();
    State& s = g_State;
    const std::filesystem::path dir = std::filesystem::path(ipcDir) / kDirName;
    const bool rotated = Rotate(dir, std::filesystem::path(ipcDir) / kPrevDirName);
    CopyText(s.record, sizeof(s.record), (dir / kRecordFile).string().c_str());
    CopyText(s.dump, sizeof(s.dump), (dir / kDumpFile).string().c_str());
    const char* logs[2] = { kYytkLogFile, kAurieLogFile };
    for (int i = 0; i < 2; ++i) {
        CopyText(s.logSource[i], sizeof(s.logSource[i]), (std::filesystem::path(logDir) / logs[i]).string().c_str());
        CopyText(s.logCopy[i], sizeof(s.logCopy[i]), (dir / logs[i]).string().c_str());
    }
    {
        SYSTEMTIME t{};
        GetSystemTime(&t);
        char head[96];
        const int n = std::snprintf(head, sizeof(head), "session %04u-%02u-%02uT%02u:%02u:%02uZ pid=%lu\n", t.wYear, t.wMonth,
                                    t.wDay, t.wHour, t.wMinute, t.wSecond, static_cast<unsigned long>(GetCurrentProcessId()));
        const HANDLE file = CreateFileA(s.record, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
                                        FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            if (n > 0) WriteFile(file, head, static_cast<DWORD>(n), &written, nullptr);
            CloseHandle(file);
        }
    }
    RefreshModules();

    const HMODULE dbghelp = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dbghelp) {
        std::snprintf(s.dumpWhy, sizeof(s.dumpWhy), "dbghelp.dll not loaded from system32, error %lu",
                      static_cast<unsigned long>(GetLastError()));
    } else {
        s.writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
        if (!s.writeDump) CopyText(s.dumpWhy, sizeof(s.dumpWhy), "MiniDumpWriteDump not found in dbghelp.dll");
    }
    s.request = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    s.done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (s.request && s.done) s.helper = CreateThread(nullptr, 0, &Helper, nullptr, 0, nullptr);
    if (!s.helper) {
        std::snprintf(s.dumpWhy, sizeof(s.dumpWhy), "the helper thread could not start, error %lu",
                      static_cast<unsigned long>(GetLastError()));
        s.writeDump = nullptr;
    }
    s.dumpReady.store(s.writeDump != nullptr);

    s.vectored = AddVectoredExceptionHandler(1, &FirstChance);
    s.firstChanceArmed.store(s.vectored != nullptr);
    s.previous = SetUnhandledExceptionFilter(&Unhandled);
    s.chain[0] = s.previous;
    s.chainCount = 1;
    s.unhandledArmed.store(true);   // armed now; whether it stays on top is CheckUnhandled's to measure
    std::string line = StatusLine();
    if (!rotated) line += " | the folder could not be made, nothing will be recorded";
    return line;
}

} // namespace ForgePact::CrashTrap
