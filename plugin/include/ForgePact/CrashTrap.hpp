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
//     kMaxFirstChanceLines a session, and stages the dump and the two log
//     copies (below); for an MSVC C++ throw it writes nothing and only
//     remembers the thrown type for its thread. A fatal exception raised
//     inside one of ForgePact's own guarded reads (GuardedReadScope, raised
//     around IsBadReadPtr and the frame profiler's __try reads) is ours and
//     handled: it is only counted. The top-level
//     unhandled-exception filter sees only what nothing handled: it appends a
//     `route=unhandled` line (a C++ exception's with its type), has the dump
//     and the two log copies made unless the staged dump was taken for this
//     same exception, then calls the filter it replaced and
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
//     no heap), staged at first chance. The first design took it from the
//     unhandled route alone, because ForgePact raises handled first-chance
//     access violations itself; Live 1 (2026-10-19) then measured a real
//     crash that left only a first-chance line: the process ended after first
//     chance and the top-level filter never ran. So the vectored route stages
//     it for every fatal exception outside a guarded read, at most one every
//     kStageMinIntervalMs and kMaxStagedDumps a session (a refusal keeps its
//     line, is counted, and the first one writes a limit line), and the next
//     load keeps it only for a crash, and only when the record line written
//     beside it names the exception the report folds (IncidentMonitor.hpp's
//     SettleStagedDump). Faults other modules raise and handle themselves
//     stage like a crash; that is what the next-load decision and the limit
//     are for. The unhandled route stays a second trigger, once a session:
//     it takes the dump unless the staged one is for the same exception (same
//     code, address and thread), and it is the only route that dumps a C++
//     exception. A helper thread created at install writes it: MiniDumpWriteDump
//     should not run on the thread that crashed, and that thread may be out of
//     stack. The route waits for the helper at most kDumpWaitMs, so a dump that
//     hangs (the dying thread may hold a lock the dump needs) cannot keep a
//     dead game alive; it waits at all because the process may end as soon as
//     the search goes on. dbghelp.dll is loaded at install from the system
//     directory and MiniDumpWriteDump taken by name: build.bat's link line
//     names user32.lib only. Without it, only the dump is off, and Install's
//     line says so.
//   * The log copies: the game's YYToolkit.log and aurie.log hold one session
//     each and are rewritten at every launch, so at the next load they
//     describe the new session. The helper thread copies their last
//     kLogCopyBytes into the trap's folder with each dump, before it. Every
//     staged file is written under a temporary name and moved over the
//     previous one, so a later staging replaces an earlier one and a
//     half-written file never replaces a whole one.
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
inline constexpr char kDumpLineFile[] = "dumpline.txt";     // the record line the dump on disk was taken for
inline constexpr char kTempSuffix[] = ".tmp";               // a staged file's name until it is whole

// The next load settles the staging by these names (IncidentMonitor.hpp).
constexpr bool SameName(const char* a, const char* b) noexcept
{
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}
static_assert(SameName(kDumpFile, Incident::kTrapDumpFile) && SameName(kDumpLineFile, Incident::kTrapDumpLineFile)
                  && SameName(kYytkLogFile, Incident::kTrapLogFiles[0]) && SameName(kAurieLogFile, Incident::kTrapLogFiles[1]),
              "the staging's names are the next load's");

inline constexpr DWORD kCppExceptionCode = 0xE06D7363;      // an MSVC C++ throw ("msc")
inline constexpr DWORD kHeapCorruption = 0xC0000374;        // STATUS_HEAP_CORRUPTION (ntstatus.h)
inline constexpr long kMaxFirstChanceLines = 32;            // a session; counted on after the cap
inline constexpr DWORD kStageMinIntervalMs = 60000;         // at most one first-chance staging this often...
inline constexpr long kMaxStagedDumps = 4;                  // ...and this many a session
inline constexpr DWORD kDumpWaitMs = 15000;                 // a route waits this long for the dump, at most
inline constexpr DWORD kHelperClaimStepMs = 50;             // the filter waits for a staging in flight in steps of this
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
    char dumpTemp[MAX_PATH];
    char dumpLine[MAX_PATH];
    char dumpLineTemp[MAX_PATH];
    char logSource[2][MAX_PATH];
    char logCopy[2][MAX_PATH];
    char logTemp[2][MAX_PATH];
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
    HANDLE request;                            // a route sets it (auto-reset)...
    HANDLE done;                               // ...and the helper sets this when it finished
    HANDLE helper;
    volatile long helperBusy;                  // 1 from a route's claim until the helper finished
    PEXCEPTION_POINTERS volatile pending;      // what the helper dumps: the exception...
    volatile DWORD pendingThread;
    volatile DWORD pendingCode;
    volatile ULONG_PTR pendingAddress;
    char pendingLine[kLineChars];              // ...and the record line it is taken for
    volatile long pendingLineLength;
    volatile long firstChanceLines;            // written, under the cap
    volatile long firstChanceOver;             // not written: the cap was reached
    volatile long guardedFaults;               // fatal, inside a guarded read of ours: counted only
    volatile long staged;                      // first-chance stagings this session
    volatile long stageRefused;                // fatal exceptions the staging limit refused
    volatile long limitSaid;                   // the limit line is written once
    volatile ULONGLONG lastStageTick;
    volatile long unhandledSeen;
    volatile long dumpSkipped;                 // the filter found its exception's dump already staged
    volatile long dumpWritten;
    volatile long dumpFailed;
    volatile long logsCopied;
    volatile long haveDumped;                  // the dump on disk: taken for this exception
    volatile DWORD dumpedCode;
    volatile ULONG_PTR dumpedAddress;
    volatile DWORD dumpedThread;
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

// ---- ForgePact's own guarded reads ------------------------------------------------
//
// A read ForgePact guards itself (IsBadReadPtr, a __try that reads memory it
// does not own) raises an access violation and handles it. The vectored route
// cannot tell that from a crash by the exception alone, and must not ask the
// loader, so every guarded read in the player build raises this thread's
// depth for the read's duration: a fatal exception raised while it is above
// zero is ours and handled, and is only counted (no line, no staging).
inline thread_local long t_GuardedReads;

struct GuardedReadScope {
    GuardedReadScope() noexcept { ++t_GuardedReads; }
    ~GuardedReadScope() { --t_GuardedReads; }
    GuardedReadScope(const GuardedReadScope&) = delete;
    GuardedReadScope& operator=(const GuardedReadScope&) = delete;
};

inline long GuardedReadDepth() noexcept
{
    return t_GuardedReads;
}

// For a function whose __try may hold no C++ object: the read runs in `read`,
// the scope around it.
template <class Read>
inline auto GuardedRead(Read&& read) noexcept(noexcept(read())) -> decltype(read())
{
    const GuardedReadScope scope;
    return read();
}

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

// The record line, into the caller's fixed buffer (kLineChars), and appended
// to trap.txt. Returns its length, newline included.
inline int WriteRecord(const EXCEPTION_RECORD* record, bool unhandled, char* line, int cap) noexcept
{
    const uintptr_t address = reinterpret_cast<uintptr_t>(record->ExceptionAddress);
    const ModuleRow* module = FindModule(address);
    const char* hookId = Incident::g_Accounting.InHookId();
    const Incident::InModState inMod = Incident::g_Accounting.InModNow();
    char type[kTypeChars];
    type[0] = '\0';
    if (unhandled && record->ExceptionCode == kCppExceptionCode && !ReadCppType(record, type, sizeof(type)))
        CopyText(type, sizeof(type), t_LastCppType);

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
    return pos;
}

// The session's first refused staging says so once in trap.txt, so a report
// whose dump is older than its crash says why. Not an `exception ` line: the
// fold never takes it for a record.
inline void WriteLimitLine() noexcept
{
    char line[kLineChars];
    const int cap = static_cast<int>(sizeof(line));
    int pos = Put(line, 0, cap, "staging limit reached: one dump every ");
    pos = PutDec(line, pos, cap, kStageMinIntervalMs / 1000);
    pos = Put(line, pos, cap, " s and ");
    pos = PutDec(line, pos, cap, static_cast<unsigned long long>(kMaxStagedDumps));
    pos = Put(line, pos, cap, " a session; later fatal exceptions keep their line, without a dump\n");
    AppendRecord(line, pos);
}

// Whether a fatal exception at first chance may be staged now, and if so the
// helper is claimed for it: one staging at a time, at most one every
// kStageMinIntervalMs and kMaxStagedDumps a session. A refusal is counted.
inline bool ClaimStaging() noexcept
{
    State& s = g_State;
    if (!s.helper) return false;
    const ULONGLONG now = GetTickCount64();
    const bool limited = s.staged >= kMaxStagedDumps || (s.staged > 0 && now - s.lastStageTick < kStageMinIntervalMs);
    if (!limited && InterlockedCompareExchange(&s.helperBusy, 1, 0) == 0) {
        InterlockedIncrement(&s.staged);
        s.lastStageTick = now;
        return true;
    }
    InterlockedIncrement(&s.stageRefused);
    if (InterlockedExchange(&s.limitSaid, 1) == 0) WriteLimitLine();
    return false;
}

// The filter's claim on the helper: a staging still in flight (another
// thread's, or one that outran the wait) is waited for, in steps, at most
// kDumpWaitMs in all.
inline bool ClaimHelper() noexcept
{
    if (!g_State.helper) return false;
    for (DWORD waited = 0;; waited += kHelperClaimStepMs) {
        if (InterlockedCompareExchange(&g_State.helperBusy, 1, 0) == 0) return true;
        if (waited >= kDumpWaitMs) return false;
        Sleep(kHelperClaimStepMs);
    }
}

// Whether the dump on disk was taken for this exception on this thread: the
// filter then takes no second one.
inline bool DumpedFor(const EXCEPTION_RECORD* record) noexcept
{
    return g_State.haveDumped && g_State.dumpedCode == record->ExceptionCode
        && g_State.dumpedAddress == reinterpret_cast<ULONG_PTR>(record->ExceptionAddress)
        && g_State.dumpedThread == GetCurrentThreadId();
}

// Hands the exception to the helper thread, which the caller has claimed, and
// waits for it at most kDumpWaitMs, so a hung dump cannot keep a dead game
// alive. The helper copies the logs, writes the dump and then `line`, the
// record line it was taken for.
inline void RequestDump(PEXCEPTION_POINTERS info, const char* line, int length) noexcept
{
    State& s = g_State;
    if (!s.request || !s.done) {
        InterlockedExchange(&s.helperBusy, 0);
        return;
    }
    int n = 0;
    for (; n < length && n < static_cast<int>(sizeof(s.pendingLine)); ++n) s.pendingLine[n] = line[n];
    s.pendingLineLength = n;
    s.pending = info;
    s.pendingThread = GetCurrentThreadId();
    s.pendingCode = info->ExceptionRecord->ExceptionCode;
    s.pendingAddress = reinterpret_cast<ULONG_PTR>(info->ExceptionRecord->ExceptionAddress);
    ResetEvent(s.done);
    SetEvent(s.request);
    WaitForSingleObject(s.done, kDumpWaitMs);
}

// ---- the two routes ----------------------------------------------------------------------

// The vectored route. Always continues the search. A fatal exception outside
// a guarded read is recorded and staged: the dump and the log copies are made
// now, because the process can end after first chance with no later route
// running (Live 1). The faulting thread waits for them, bounded.
inline LONG CALLBACK FirstChance(PEXCEPTION_POINTERS info) noexcept
{
    if (!info || !info->ExceptionRecord || t_Inside) return EXCEPTION_CONTINUE_SEARCH;
    t_Inside = true;
    const EXCEPTION_RECORD* record = info->ExceptionRecord;
    if (record->ExceptionCode == kCppExceptionCode) {
        char type[kTypeChars];
        if (ReadCppType(record, type, sizeof(type))) CopyText(t_LastCppType, sizeof(t_LastCppType), type);
    } else if (IsFatal(record->ExceptionCode)) {
        if (t_GuardedReads > 0) {
            InterlockedIncrement(&g_State.guardedFaults);
        } else if (InterlockedIncrement(&g_State.firstChanceLines) <= kMaxFirstChanceLines) {
            char line[kLineChars];
            const int length = WriteRecord(record, false, line, static_cast<int>(sizeof(line)));
            if (ClaimStaging()) RequestDump(info, line, length);
        } else {
            InterlockedDecrement(&g_State.firstChanceLines);
            InterlockedIncrement(&g_State.firstChanceOver);
        }
    }
    t_Inside = false;
    return EXCEPTION_CONTINUE_SEARCH;
}

// The top-level filter: what nothing else handled. Records it once, has the
// logs copied and the dump written unless the staged dump was taken for this
// same exception, then gives the previous filter its turn and returns that
// one's answer.
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
        char line[kLineChars];
        const int length = WriteRecord(exception, true, line, static_cast<int>(sizeof(line)));
        const bool claimed = ClaimHelper();
        if (DumpedFor(exception)) {
            if (claimed) InterlockedExchange(&g_State.helperBusy, 0);
            InterlockedIncrement(&g_State.dumpSkipped);
            static constexpr char kNote[] = "dump already staged at first chance for this exception; no second one taken\n";
            AppendRecord(kNote, static_cast<int>(sizeof(kNote) - 1));
        } else if (claimed) {
            RequestDump(info, line, length);
        }
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

// The dump, under its temporary name.
inline bool WriteDump() noexcept
{
    if (!g_State.dumpReady.load() || !g_State.writeDump || !g_State.dumpTemp[0]) return false;
    const HANDLE file = CreateFileA(g_State.dumpTemp, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    MINIDUMP_EXCEPTION_INFORMATION exception{};
    exception.ThreadId = g_State.pendingThread;
    exception.ExceptionPointers = g_State.pending;
    exception.ClientPointers = FALSE;
    const BOOL ok = g_State.writeDump(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal,
                                      g_State.pending ? &exception : nullptr, nullptr, nullptr);
    CloseHandle(file);
    if (!ok) DeleteFileA(g_State.dumpTemp);
    return ok != FALSE;
}

// A small file written whole, under its temporary name.
inline bool WriteWhole(const char* path, const char* text, int length) noexcept
{
    if (!path[0] || length <= 0) return false;
    const HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const BOOL ok = WriteFile(file, text, static_cast<DWORD>(length), &written, nullptr);
    CloseHandle(file);
    return ok && written == static_cast<DWORD>(length);
}

// The helper's work for either route: the two log copies, the dump, then the
// record line the dump was taken for, each under its temporary name and then
// moved over the previous one. The old line goes before the new dump moves in
// and the new line only after it, so a dump never sits beside another
// exception's line: at the next load a dump without its own line is not kept.
inline void Stage() noexcept
{
    State& s = g_State;
    for (int i = 0; i < 2; ++i)
        if (CopyLogTail(s.logSource[i], s.logTemp[i]) && MoveFileExA(s.logTemp[i], s.logCopy[i], MOVEFILE_REPLACE_EXISTING))
            InterlockedIncrement(&s.logsCopied);
    if (!s.dumpReady.load()) return;
    if (!WriteDump()) {
        InterlockedIncrement(&s.dumpFailed);
        return;
    }
    DeleteFileA(s.dumpLine);
    InterlockedExchange(&s.haveDumped, 0);
    if (!MoveFileExA(s.dumpTemp, s.dump, MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(s.dumpTemp);
        InterlockedIncrement(&s.dumpFailed);
        return;
    }
    InterlockedIncrement(&s.dumpWritten);
    s.dumpedCode = s.pendingCode;
    s.dumpedAddress = s.pendingAddress;
    s.dumpedThread = s.pendingThread;
    InterlockedExchange(&s.haveDumped, 1);
    if (WriteWhole(s.dumpLineTemp, s.pendingLine, s.pendingLineLength))
        MoveFileExA(s.dumpLineTemp, s.dumpLine, MOVEFILE_REPLACE_EXISTING);
}

// Created at install, never at a crash. Its own faults are neither recorded
// nor staged: t_Inside holds for its whole life.
inline DWORD WINAPI Helper(LPVOID) noexcept
{
    t_Inside = true;
    while (WaitForSingleObject(g_State.request, INFINITE) == WAIT_OBJECT_0) {
        Stage();
        SetEvent(g_State.done);
        InterlockedExchange(&g_State.helperBusy, 0);
    }
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
           + " | staged " + std::to_string(s.staged) + " of " + std::to_string(kMaxStagedDumps)
           + (s.stageRefused ? " (" + std::to_string(s.stageRefused) + " refused by the limit)" : std::string())
           + " | guarded reads " + std::to_string(s.guardedFaults)
           + " | modules " + std::to_string(modules) + " | bp_ipc\\" + kDirName;
}

// First-chance stagings this session (`incident stat`'s `staged <n>`).
inline long StagedCount() noexcept
{
    return g_State.staged;
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
    CopyText(s.dumpTemp, sizeof(s.dumpTemp), ((dir / kDumpFile).string() + kTempSuffix).c_str());
    CopyText(s.dumpLine, sizeof(s.dumpLine), (dir / kDumpLineFile).string().c_str());
    CopyText(s.dumpLineTemp, sizeof(s.dumpLineTemp), ((dir / kDumpLineFile).string() + kTempSuffix).c_str());
    const char* logs[2] = { kYytkLogFile, kAurieLogFile };
    for (int i = 0; i < 2; ++i) {
        CopyText(s.logSource[i], sizeof(s.logSource[i]), (std::filesystem::path(logDir) / logs[i]).string().c_str());
        CopyText(s.logCopy[i], sizeof(s.logCopy[i]), (dir / logs[i]).string().c_str());
        CopyText(s.logTemp[i], sizeof(s.logTemp[i]), ((dir / logs[i]).string() + kTempSuffix).c_str());
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
    s.request = CreateEventW(nullptr, FALSE, FALSE, nullptr);   // one request per signal
    s.done = CreateEventW(nullptr, TRUE, FALSE, nullptr);       // reset by the route that claimed the helper
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
