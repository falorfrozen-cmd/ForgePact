#pragma once

// ForgePact::FrameProfiler - where the game's frame thread spends its time
// (`frameprof`).
//
// A sampling profiler for one thread. A background thread wakes about every
// millisecond, suspends the frame thread just long enough to read its
// registers and copy the live part of its stack, resumes it, and only then
// walks the copy with the x64 unwind tables. Between SuspendThread and
// ResumeThread (CaptureOnce) nothing allocates, logs, locks or calls into the
// game: the frame thread may hold the heap, CRT or loader lock at that moment,
// and a sampler that needed one of them would freeze the game - the stall
// watchdog's rule in ModuleMain.cpp. Walking, naming, counting and the report
// all run on the sampler's own thread, on another core; the game pays only
// for the pauses, and the report measures those too.
//
// It names what it finds from three sources: the game's compiled-code table
// (YYToolkit's YYGMLFuncs, one entry per script and object event, walked from
// any one entry the adapter hands over), the built-in functions the adapter
// could resolve by name, and each system DLL's export table. Everything else
// is "module!0x<rva>".
//
// Game-independent: Win32 and the standard library only, no runtime
// interface. The adapter in ModuleMain.cpp supplies the frame thread, its
// stack range, the table entry, the built-ins and a once-a-second context
// callback; tests/frame_profiler_harness.cpp drives the same class against its
// own threads. Avoids std::min/std::max: ModuleMain.cpp includes <windows.h>
// without NOMINMAX.

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ExitSafeThread.hpp"

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

namespace ForgePact::FrameProfiler {

// One entry of the game's compiled-code table (YYToolkit's YYGMLFuncs).
struct GmlEntry {
    const char* name;
    const void* function;
    const void* variables;
};
static_assert(sizeof(GmlEntry) == 24, "YYGMLFuncs is three pointers");

struct NamedAddress {
    uintptr_t address = 0;
    std::string name;
};

// What the game was doing, read on the frame thread once a second.
struct Context {
    std::string room;
    long long instances = -1;
    long long monsters = -1;
};
using ContextProvider = bool (*)(Context&);

inline constexpr double kMinSeconds = 1.0;
inline constexpr double kMaxSeconds = 600.0;
inline constexpr double kDefaultSeconds = 30.0;
// One sample pauses the frame thread for SuspendThread + GetThreadContext +
// ResumeThread: 60-100 us median on the development PC (measured 2026-09-28,
// most of it GetThreadContext waiting for the suspension to land). 250 Hz
// keeps that under about 2% of the frame thread's time and still gives 7,500
// samples in 30 s; faster rates are for short captures.
inline constexpr unsigned kMinHz = 20;
inline constexpr unsigned kMaxHz = 2000;
inline constexpr unsigned kDefaultHz = 250;
// The same pause grew to ~500 us on a machine busy compiling in the
// background (measured 2026-09-28): the sampler then halves its rate while
// the pauses add up to more than this share of the frame thread's time, and
// doubles it back, up to the requested rate, once they fall well below it.
inline constexpr double kPauseBudget = 0.03;

struct StartParams {
    HANDLE frameThread = nullptr;      // needs suspend + get-context rights; closed when the capture ends
    DWORD frameThreadId = 0;
    uintptr_t stackLow = 0;            // the frame thread's stack, GetCurrentThreadStackLimits
    uintptr_t stackHigh = 0;
    double seconds = kDefaultSeconds;
    unsigned hz = kDefaultHz;
    const GmlEntry* gmlAnchor = nullptr;   // any entry of the table (CScript::m_Functions)
    std::vector<NamedAddress> builtins;    // built-in entry points the adapter could name
    std::filesystem::path outDir;
    std::string stem;                      // files: <stem>.json, <stem>.stacks.txt, <stem>.txt
    std::string toolVersion;
    ContextProvider context = nullptr;
    uintptr_t gameBase = 0;                // the game's exe; 0 = this process's exe
};

inline constexpr size_t kStackCopyBytes = 512u * 1024u;
inline constexpr int kMaxFrames = 160;
inline constexpr double kHitchMs = 50.0;
inline constexpr size_t kTopRows = 40;

// A frame key is the start of the function a frame is in. Two tag bits say
// how it was found: no unwind entry (a leaf, keyed by its own address), or no
// module at all (generated code, where the walk stops).
inline constexpr uint64_t kTagNoUnwind = 1ull << 63;
inline constexpr uint64_t kTagUnknown = 1ull << 62;
inline constexpr uint64_t kTagMask = kTagNoUnwind | kTagUnknown;

namespace detail {

inline uint64_t Qpc() noexcept
{
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return static_cast<uint64_t>(q.QuadPart);
}

inline uint64_t QpcFreq() noexcept
{
    static const uint64_t f = [] {
        LARGE_INTEGER q;
        QueryPerformanceFrequency(&q);
        return static_cast<uint64_t>(q.QuadPart);
    }();
    return f;
}

inline std::string Narrow(const wchar_t* w)
{
    if (!w || !*w) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

inline std::string LowerAscii(std::string s)
{
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

inline bool StartsWith(const std::string& s, const char* prefix)
{
    return s.rfind(prefix, 0) == 0;
}

inline std::string JsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 2);
    for (const unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char b[8];
                snprintf(b, sizeof b, "\\u%04x", c);
                out += b;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

inline std::string Fixed(double v, int digits)
{
    char b[64];
    snprintf(b, sizeof b, "%.*f", digits, v);
    return b;
}

inline double Percent(uint64_t part, uint64_t whole)
{
    return whole ? 100.0 * static_cast<double>(part) / static_cast<double>(whole) : 0.0;
}

// A loaded module's exception and export directories. SEH-guarded: the
// headers belong to a module that could unload between the snapshot and here.
inline bool ReadDirectories(uintptr_t base, const RUNTIME_FUNCTION*& pdata, DWORD& count,
                            DWORD& exportRva, DWORD& exportSize) noexcept
{
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
            return false;
        const IMAGE_DATA_DIRECTORY& ex = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        const IMAGE_DATA_DIRECTORY& ed = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        pdata = ex.VirtualAddress ? reinterpret_cast<const RUNTIME_FUNCTION*>(base + ex.VirtualAddress) : nullptr;
        count = ex.VirtualAddress ? static_cast<DWORD>(ex.Size / sizeof(RUNTIME_FUNCTION)) : 0;
        exportRva = ed.VirtualAddress;
        exportSize = ed.Size;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// A loaded module's address range, sorted by `lo`, for the table walk.
struct CodeRange {
    uintptr_t lo;
    uintptr_t hi;
};

inline bool InRanges(uintptr_t a, const CodeRange* ranges, size_t count) noexcept
{
    size_t lo = 0, hi = count;
    while (lo < hi) {
        const size_t mid = (lo + hi) / 2;
        if (ranges[mid].lo <= a) lo = mid + 1; else hi = mid;
    }
    return lo && a < ranges[lo - 1].hi;
}

// A row's name must be a "gml_" string inside the game image. Its function
// may point anywhere code lives: a mod that hooks a script through the table
// swaps the row's function for its own, in its own DLL (measured in the game
// 2026-09-28: the first walk, which demanded a function inside the image,
// stopped after 680 of the table's 20,951 rows).
inline bool GmlEntryLooksValid(const GmlEntry* e, uintptr_t lo, uintptr_t hi,
                               const CodeRange* modules, size_t moduleCount) noexcept
{
    const uintptr_t p = reinterpret_cast<uintptr_t>(e);
    if (p < lo || p + sizeof(GmlEntry) > hi || (p & 7)) return false;
    const uintptr_t n = reinterpret_cast<uintptr_t>(e->name);
    if (n < lo || n + 5 > hi) return false;
    if (std::memcmp(e->name, "gml_", 4) != 0) return false;
    const uintptr_t f = reinterpret_cast<uintptr_t>(e->function);
    return !f || (f >= lo && f < hi) || InRanges(f, modules, moduleCount);
}

// The table is one contiguous array in the image: walk both ways from the
// anchor while entries still look like entries. SEH-guarded, no objects.
inline size_t FindGmlTable(const GmlEntry* anchor, uintptr_t lo, uintptr_t hi, const CodeRange* modules,
                           size_t moduleCount, const GmlEntry** first) noexcept
{
    __try {
        if (!anchor || !GmlEntryLooksValid(anchor, lo, hi, modules, moduleCount)) return 0;
        const GmlEntry* b = anchor;
        for (size_t guard = 0; guard < 4000000 && GmlEntryLooksValid(b - 1, lo, hi, modules, moduleCount); ++guard) --b;
        const GmlEntry* e = anchor + 1;
        for (size_t guard = 0; guard < 4000000 && GmlEntryLooksValid(e, lo, hi, modules, moduleCount); ++guard) ++e;
        *first = b;
        return static_cast<size_t>(e - b);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// The game image's section table, copied out of its in-memory headers.
struct SectionSpan {
    DWORD va;
    DWORD rawSize;
    DWORD rawOffset;
};

inline size_t ReadSectionTable(uintptr_t base, SectionSpan* out, size_t capacity) noexcept
{
    __try {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
        const IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
        size_t n = nt->FileHeader.NumberOfSections;
        if (n > capacity) n = capacity;
        for (size_t i = 0; i < n; ++i) out[i] = { s[i].VirtualAddress, s[i].SizeOfRawData, s[i].PointerToRawData };
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

inline size_t SafeCopy(void* dst, const void* src, size_t n) noexcept
{
    __try {
        std::memcpy(dst, src, n);
        return n;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

inline bool VirtualUnwindSafe(uintptr_t imageBase, uintptr_t pc, const RUNTIME_FUNCTION* f, CONTEXT& ctx) noexcept
{
    __try {
        PVOID handlerData = nullptr;
        DWORD64 establisher = 0;
        RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, pc, const_cast<PRUNTIME_FUNCTION>(f), &ctx,
                         &handlerData, &establisher, nullptr);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

inline bool ReadU64Safe(const void* p, uint64_t& v) noexcept
{
    __try {
        v = *static_cast<const volatile uint64_t*>(p);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

} // namespace detail

// ---- modules --------------------------------------------------------------

enum class ModuleKind : uint8_t { Game, Graphics, Mod, System, Other };

inline const char* ModuleKindName(ModuleKind k) noexcept
{
    switch (k) {
    case ModuleKind::Game: return "game";
    case ModuleKind::Graphics: return "graphics";
    case ModuleKind::Mod: return "mod";
    case ModuleKind::System: return "system";
    default: return "other";
    }
}

inline ModuleKind ClassifyModule(const std::string& lowerName, const std::string& lowerPath, bool isGame)
{
    if (isGame) return ModuleKind::Game;
    static const char* const kGraphics[] = {
        "d3d11.dll", "dxgi.dll", "d3d12.dll", "d3d12core.dll", "d3d10warp.dll", "d3d9.dll", "dxcore.dll",
        "d3d11on12.dll", "d3dcompiler_47.dll", "opengl32.dll", "vulkan-1.dll", "gameoverlayrenderer64.dll",
    };
    for (const char* g : kGraphics) if (lowerName == g) return ModuleKind::Graphics;
    // Display drivers live in the driver store; the prefixes catch the ones
    // installed elsewhere (NVIDIA, AMD, Intel user-mode drivers).
    if (lowerPath.find("\\driverstore\\") != std::string::npos) return ModuleKind::Graphics;
    static const char* const kGraphicsPrefix[] = {
        "nvwgf2um", "nvldumd", "nvd3dum", "nvoglv", "amdxx", "atidxx", "atiumd", "amdxc", "amdihk",
        "igd10", "igd12", "igdumd", "igdusc", "igc64", "igdgmm", "igxelp",
    };
    for (const char* g : kGraphicsPrefix) if (detail::StartsWith(lowerName, g)) return ModuleKind::Graphics;
    static const char* const kMods[] = {
        "bloodpactplugin", "yytoolkit", "auriecore", "hsafkexpedition", "hsofflinetracker",
    };
    for (const char* m : kMods) if (detail::StartsWith(lowerName, m)) return ModuleKind::Mod;
    if (lowerPath.find("\\mods\\") != std::string::npos) return ModuleKind::Mod;
    if (lowerPath.find("\\windows\\") != std::string::npos) return ModuleKind::System;
    return ModuleKind::Other;
}

struct ModuleInfo {
    uintptr_t base = 0;
    uintptr_t end = 0;
    std::string name;
    std::string path;
    std::wstring widePath;
    ModuleKind kind = ModuleKind::Other;
    const RUNTIME_FUNCTION* pdata = nullptr;
    DWORD pdataCount = 0;
    DWORD exportRva = 0;
    DWORD exportSize = 0;
};

// A snapshot of the process's modules with their unwind tables. Read-only
// after Build, so the walk can use it from any thread without a lock; nothing
// here calls RtlLookupFunctionEntry, which can take loader locks.
class ModuleMap {
public:
    bool Build(uintptr_t gameBase)
    {
        m_Mods.clear();
        m_Game = -1;
        HANDLE snap = INVALID_HANDLE_VALUE;
        for (int attempt = 0; attempt < 5 && snap == INVALID_HANDLE_VALUE; ++attempt) {
            snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
            if (snap == INVALID_HANDLE_VALUE && GetLastError() != ERROR_BAD_LENGTH) break;
        }
        if (snap == INVALID_HANDLE_VALUE) return false;
        MODULEENTRY32W me{};
        me.dwSize = sizeof me;
        for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me)) {
            ModuleInfo m;
            m.base = reinterpret_cast<uintptr_t>(me.modBaseAddr);
            m.end = m.base + me.modBaseSize;
            m.name = detail::Narrow(me.szModule);
            m.path = detail::Narrow(me.szExePath);
            m.widePath = me.szExePath;
            detail::ReadDirectories(m.base, m.pdata, m.pdataCount, m.exportRva, m.exportSize);
            m.kind = ClassifyModule(detail::LowerAscii(m.name), detail::LowerAscii(m.path), m.base == gameBase);
            m_Mods.push_back(std::move(m));
        }
        CloseHandle(snap);
        std::sort(m_Mods.begin(), m_Mods.end(),
                  [](const ModuleInfo& a, const ModuleInfo& b) { return a.base < b.base; });
        for (size_t i = 0; i < m_Mods.size(); ++i)
            if (m_Mods[i].base == gameBase) m_Game = static_cast<int>(i);
        return !m_Mods.empty();
    }

    const ModuleInfo* Find(uintptr_t a) const noexcept
    {
        size_t lo = 0, hi = m_Mods.size();
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            if (m_Mods[mid].base <= a) lo = mid + 1; else hi = mid;
        }
        if (!lo) return nullptr;
        const ModuleInfo& m = m_Mods[lo - 1];
        return a < m.end ? &m : nullptr;
    }

    // The unwind entry covering `a`, by binary search in the module's own
    // .pdata (sorted by BeginAddress). Null for a leaf function.
    static const RUNTIME_FUNCTION* Lookup(const ModuleInfo& m, uintptr_t a) noexcept
    {
        if (!m.pdata || !m.pdataCount || a < m.base || a >= m.end) return nullptr;
        const DWORD rva = static_cast<DWORD>(a - m.base);
        size_t lo = 0, hi = m.pdataCount;
        while (lo < hi) {
            const size_t mid = (lo + hi) / 2;
            if (m.pdata[mid].BeginAddress <= rva) lo = mid + 1; else hi = mid;
        }
        if (!lo) return nullptr;
        const RUNTIME_FUNCTION* f = &m.pdata[lo - 1];
        return rva < f->EndAddress ? f : nullptr;
    }

    // A chained entry covers a fragment of a function; its chain leads to the
    // entry for the function's start.
    static const RUNTIME_FUNCTION* Primary(const ModuleInfo& m, const RUNTIME_FUNCTION* f) noexcept
    {
        for (int guard = 0; f && guard < 32; ++guard) {
            const DWORD ui = f->UnwindInfoAddress;
            if (ui & 1) {
                const uintptr_t next = m.base + (ui & ~1u);
                if (next < m.base || next + sizeof(RUNTIME_FUNCTION) > m.end) return f;
                f = reinterpret_cast<const RUNTIME_FUNCTION*>(next);
                continue;
            }
            const uintptr_t info = m.base + ui;
            if (info < m.base || info + 4 > m.end) return f;
            const auto* bytes = reinterpret_cast<const uint8_t*>(info);
            if (!((bytes[0] >> 3) & UNW_FLAG_CHAININFO)) return f;
            const size_t codes = (static_cast<size_t>(bytes[2]) + 1) & ~static_cast<size_t>(1);
            const uintptr_t chained = info + 4 + codes * 2;
            if (chained + sizeof(RUNTIME_FUNCTION) > m.end) return f;
            f = reinterpret_cast<const RUNTIME_FUNCTION*>(chained);
        }
        return f;
    }

    uint64_t FunctionKey(uintptr_t pc) const noexcept
    {
        const ModuleInfo* m = Find(pc);
        if (!m) return static_cast<uint64_t>(pc) | kTagUnknown;
        const RUNTIME_FUNCTION* f = Lookup(*m, pc);
        if (!f) return static_cast<uint64_t>(pc) | kTagNoUnwind;
        f = Primary(*m, f);
        return static_cast<uint64_t>(m->base + f->BeginAddress);
    }

    const std::vector<ModuleInfo>& Modules() const noexcept { return m_Mods; }
    const ModuleInfo* Game() const noexcept { return m_Game >= 0 ? &m_Mods[static_cast<size_t>(m_Game)] : nullptr; }

private:
    std::vector<ModuleInfo> m_Mods;
    int m_Game = -1;
};

// ---- capture and walk -----------------------------------------------------

enum class CaptureResult : uint8_t { Ok, SuspendFailed, ContextFailed, StackOutside };

// The only code that runs while the frame thread is suspended. Win32 calls,
// one QueryPerformanceCounter pair and a guarded memcpy into a buffer the
// caller allocated beforehand - no heap, no lock, no logging, no game call,
// and no exception can leave it between SuspendThread and ResumeThread.
inline CaptureResult CaptureOnce(HANDLE thread, uintptr_t stackLow, uintptr_t stackHigh, uint8_t* buffer,
                                 size_t capacity, CONTEXT& ctx, size_t& copied, bool& truncated,
                                 uint64_t& pausedTicks, uint64_t& atQpc) noexcept
{
    copied = 0;
    truncated = false;
    pausedTicks = 0;
    const uint64_t t0 = detail::Qpc();
    atQpc = t0;
    if (SuspendThread(thread) == static_cast<DWORD>(-1)) return CaptureResult::SuspendFailed;
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    CaptureResult result = GetThreadContext(thread, &ctx) ? CaptureResult::Ok : CaptureResult::ContextFailed;
    if (result == CaptureResult::Ok) {
        const uintptr_t sp = static_cast<uintptr_t>(ctx.Rsp);
        if (sp < stackLow || sp >= stackHigh) {
            result = CaptureResult::StackOutside;
        } else {
            size_t n = stackHigh - sp;
            if (n > capacity) { n = capacity; truncated = true; }
            copied = detail::SafeCopy(buffer, reinterpret_cast<const void*>(sp), n);
        }
    }
    ResumeThread(thread);
    pausedTicks = detail::Qpc() - t0;
    return result;
}

enum class WalkEnd : uint8_t { Complete, UnknownCode, Fault, DepthCap, OffStack, Count };

inline const char* WalkEndName(WalkEnd e) noexcept
{
    switch (e) {
    case WalkEnd::Complete: return "complete";
    case WalkEnd::UnknownCode: return "unknown_code";
    case WalkEnd::Fault: return "unwind_fault";
    case WalkEnd::DepthCap: return "depth_cap";
    case WalkEnd::OffStack: return "off_stack";
    default: return "?";
    }
}

// Walks the copied stack after the frame thread has been resumed. Pointers
// into the original stack range - in the copy and in the registers - are
// moved into the copy first, so the unwinder reads the snapshot, never the
// live stack the frame thread has already changed. Returns the frames, leaf
// first. No heap use; SEH guards the unwinder against a bad snapshot.
inline int WalkCopy(CONTEXT& ctx, uint8_t* copy, size_t copied, const ModuleMap& modules,
                    uint64_t* out, int maxFrames, WalkEnd& end) noexcept
{
    end = WalkEnd::Complete;
    int n = 0;
    if (!copied) {
        if (ctx.Rip && maxFrames > 0) out[n++] = ctx.Rip;
        end = WalkEnd::OffStack;
        return n;
    }
    const uint64_t origLo = ctx.Rsp;
    const uint64_t origHi = origLo + copied;
    const uint64_t copyLo = reinterpret_cast<uint64_t>(copy);
    const uint64_t copyHi = copyLo + copied;
    auto relocate = [&](DWORD64& v) {
        if (v >= origLo && v < origHi) v = v - origLo + copyLo;
    };
    auto* words = reinterpret_cast<uint64_t*>(copy);
    const size_t count = copied / sizeof(uint64_t);
    for (size_t i = 0; i < count; ++i)
        if (words[i] >= origLo && words[i] < origHi) words[i] = words[i] - origLo + copyLo;
    relocate(ctx.Rsp);
    relocate(ctx.Rbp);
    relocate(ctx.Rbx);
    relocate(ctx.Rsi);
    relocate(ctx.Rdi);
    relocate(ctx.R12);
    relocate(ctx.R13);
    relocate(ctx.R14);
    relocate(ctx.R15);
    relocate(ctx.Rax);
    relocate(ctx.Rcx);
    relocate(ctx.Rdx);
    relocate(ctx.R8);
    relocate(ctx.R9);
    relocate(ctx.R10);
    relocate(ctx.R11);

    for (;;) {
        if (n >= maxFrames) { end = WalkEnd::DepthCap; break; }
        const uintptr_t pc = static_cast<uintptr_t>(ctx.Rip);
        if (!pc) break;
        out[n++] = pc;
        const ModuleInfo* m = modules.Find(pc);
        if (!m) { end = WalkEnd::UnknownCode; break; }
        const RUNTIME_FUNCTION* f = ModuleMap::Lookup(*m, pc);
        if (f) {
            if (!detail::VirtualUnwindSafe(m->base, pc, f, ctx)) { end = WalkEnd::Fault; break; }
        } else {
            // A leaf function: the return address is at the top of the stack.
            if (ctx.Rsp < copyLo || ctx.Rsp + 8 > copyHi) { end = WalkEnd::OffStack; break; }
            uint64_t ret = 0;
            if (!detail::ReadU64Safe(reinterpret_cast<const void*>(ctx.Rsp), ret)) { end = WalkEnd::Fault; break; }
            ctx.Rip = ret;
            ctx.Rsp += 8;
        }
        if (ctx.Rsp < copyLo || ctx.Rsp > copyHi) { end = WalkEnd::OffStack; break; }
        if (ctx.Rsp == copyHi) {
            // The outermost frame of the copy: its caller, if any, is the thread start.
            if (ctx.Rip && n < maxFrames && modules.Find(static_cast<uintptr_t>(ctx.Rip)))
                out[n++] = static_cast<uintptr_t>(ctx.Rip);
            break;
        }
    }
    return n;
}

// ---- names ----------------------------------------------------------------

enum class SymKind : uint8_t { GmlScript, GmlEvent, GmlOther, Builtin, Export, Unnamed, Unknown };

inline bool IsGml(SymKind k) noexcept
{
    return k == SymKind::GmlScript || k == SymKind::GmlEvent || k == SymKind::GmlOther;
}

struct Symbol {
    std::string name;
    std::string label;
    SymKind kind = SymKind::Unknown;
    ModuleKind module = ModuleKind::Other;
    std::string moduleName;
    bool wait = false;   // a system call that blocks: sleep, wait, yield
};

// "Draw_64" -> "Draw GUI" and friends, from GameMaker's event numbering.
inline std::string GmlEventName(const std::string& type, const std::string& arg)
{
    const int n = std::atoi(arg.c_str());
    if (type == "Create") return "Create";
    if (type == "Destroy") return "Destroy";
    if (type == "CleanUp") return "Clean Up";
    if (type == "PreCreate") return "Pre Create";
    if (type == "Alarm") return "Alarm " + arg;
    if (type == "Step") return n == 1 ? "Begin Step" : n == 2 ? "End Step" : "Step";
    if (type == "Draw") {
        switch (n) {
        case 0: return "Draw";
        case 64: return "Draw GUI";
        case 65: return "Window Resize";
        case 72: return "Draw Begin";
        case 73: return "Draw End";
        case 74: return "Draw GUI Begin";
        case 75: return "Draw GUI End";
        case 76: return "Pre-Draw";
        case 77: return "Post-Draw";
        default: return "Draw " + arg;
        }
    }
    if (type == "Other") {
        if (n >= 10 && n <= 25) return "User Event " + std::to_string(n - 10);
        switch (n) {
        case 0: return "Outside Room";
        case 1: return "Intersect Boundary";
        case 2: return "Game Start";
        case 3: return "Game End";
        case 4: return "Room Start";
        case 5: return "Room End";
        case 7: return "Animation End";
        case 8: return "Path Ended";
        case 60: return "Async Image Loaded";
        case 62: return "Async HTTP";
        case 63: return "Async Dialog";
        case 68: return "Async Networking";
        case 69: return "Async Steam";
        case 72: return "Async Save/Load";
        case 75: return "Async System";
        default: return "Other " + arg;
        }
    }
    if (type == "Collision") return "Collision with " + arg;
    return type + " " + arg;
}

inline std::string GmlLabel(const std::string& name, SymKind& kind)
{
    if (detail::StartsWith(name, "gml_Script_")) { kind = SymKind::GmlScript; return name.substr(11); }
    if (detail::StartsWith(name, "gml_GlobalScript_")) { kind = SymKind::GmlScript; return name.substr(17) + " (script file)"; }
    if (detail::StartsWith(name, "gml_Object_")) {
        kind = SymKind::GmlEvent;
        const std::string rest = name.substr(11);
        static const char* const kTypes[] = {
            "Create", "Destroy", "Alarm", "Step", "Collision", "Keyboard", "Mouse", "Other", "Draw",
            "KeyPress", "KeyRelease", "Trigger", "CleanUp", "Gesture", "PreCreate",
        };
        size_t best = std::string::npos;
        std::string bestType;
        for (const char* t : kTypes) {
            const std::string needle = std::string("_") + t + "_";
            const size_t p = rest.rfind(needle);
            if (p != std::string::npos && (best == std::string::npos || p > best)) { best = p; bestType = t; }
        }
        if (best == std::string::npos || best == 0) return rest;
        const std::string arg = rest.substr(best + bestType.size() + 2);
        return rest.substr(0, best) + " " + GmlEventName(bestType, arg);
    }
    kind = SymKind::GmlOther;
    return name;
}

inline bool IsWaitExport(const std::string& exportName)
{
    return exportName.find("Wait") != std::string::npos || exportName.find("Delay") != std::string::npos
        || exportName.find("YieldExecution") != std::string::npos
        || exportName.find("RemoveIoCompletion") != std::string::npos;
}

class Symbolizer {
public:
    // Walks the compiled-code table from one entry. Returns how many rows it
    // found (0 when the anchor does not look like one). A row whose function
    // a mod swapped for its own hook no longer says where the game's function
    // is, so the row is read again from the exe file on disk, where it still
    // holds the original address.
    size_t LoadGmlTable(const GmlEntry* anchor, const ModuleMap& modules)
    {
        const ModuleInfo* game = modules.Game();
        if (!anchor || !game) return 0;
        std::vector<detail::CodeRange> ranges;
        ranges.reserve(modules.Modules().size());
        for (const ModuleInfo& m : modules.Modules()) ranges.push_back({ m.base, m.end });
        const GmlEntry* first = nullptr;
        const size_t count = detail::FindGmlTable(anchor, game->base, game->end, ranges.data(), ranges.size(), &first);
        std::vector<size_t> swapped;
        for (size_t i = 0; i < count; ++i) {
            const uintptr_t f = reinterpret_cast<uintptr_t>(first[i].function);
            if (!f) continue;
            if (f >= game->base && f < game->end) m_Gml.emplace(f, first[i].name);
            else swapped.push_back(i);
        }
        m_GmlSwapped = swapped.size();
        if (!swapped.empty()) {
            std::vector<uintptr_t> original;
            if (ReadOriginalFunctions(*game, first, count, original)) {
                for (const size_t i : swapped) {
                    if (!original[i]) continue;
                    m_Gml.emplace(original[i], first[i].name);
                    ++m_GmlRestored;
                }
            }
        }
        return count;
    }

    size_t GmlSwapped() const noexcept { return m_GmlSwapped; }
    size_t GmlRestored() const noexcept { return m_GmlRestored; }

    void AddBuiltins(const std::vector<NamedAddress>& builtins)
    {
        for (const auto& b : builtins)
            if (b.address) m_Builtins.emplace(b.address, b.name);
    }

    size_t GmlCount() const noexcept { return m_Gml.size(); }
    size_t BuiltinCount() const noexcept { return m_Builtins.size(); }

    Symbol Resolve(uint64_t key, const ModuleMap& modules)
    {
        Symbol s;
        const uintptr_t addr = static_cast<uintptr_t>(key & ~kTagMask);
        const ModuleInfo* m = modules.Find(addr);
        if (m) { s.module = m->kind; s.moduleName = m->name; }
        if (!(key & kTagMask)) {
            const auto g = m_Gml.find(addr);
            if (g != m_Gml.end()) {
                s.name = g->second;
                s.label = GmlLabel(s.name, s.kind);
                return s;
            }
            const auto b = m_Builtins.find(addr);
            if (b != m_Builtins.end()) {
                s.name = b->second;
                s.label = s.name + "()";
                s.kind = SymKind::Builtin;
                return s;
            }
        }
        if (!m) {
            s.kind = SymKind::Unknown;
            s.name = s.label = "unknown code";
            return s;
        }
        const DWORD rva = static_cast<DWORD>(addr - m->base);
        const auto& exports = ExportsOf(*m);
        if (!exports.empty()) {
            auto it = std::upper_bound(exports.begin(), exports.end(), rva,
                                       [](DWORD v, const std::pair<DWORD, const char*>& e) { return v < e.first; });
            if (it != exports.begin()) {
                --it;
                // An exact export for a function with unwind data; a syscall
                // stub (no unwind data) is named by the export just before it.
                const DWORD distance = rva - it->first;
                if (distance == 0 || ((key & kTagNoUnwind) && distance < 64)) {
                    s.name = m->name + "!" + it->second;
                    s.kind = SymKind::Export;
                    s.wait = (m->kind == ModuleKind::System) && IsWaitExport(it->second);
                }
            }
        }
        if (s.name.empty()) {
            char b[32];
            snprintf(b, sizeof b, "!0x%X", static_cast<unsigned>(rva));
            s.name = m->name + b;
            s.kind = SymKind::Unnamed;
        }
        s.label = s.name;
        return s;
    }

private:
    // Each row's function as the exe file on disk stores it, moved to where
    // the image is loaded. Only the table's own bytes are read.
    static bool ReadOriginalFunctions(const ModuleInfo& game, const GmlEntry* first, size_t count,
                                      std::vector<uintptr_t>& out)
    {
        detail::SectionSpan sections[128];
        const size_t sectionCount = detail::ReadSectionTable(game.base, sections, 128);
        const uint64_t rva = reinterpret_cast<uintptr_t>(first) - game.base;
        const uint64_t bytes = static_cast<uint64_t>(count) * sizeof(GmlEntry);
        uint64_t fileOffset = 0;
        bool mapped = false;
        for (size_t i = 0; i < sectionCount; ++i) {
            if (rva >= sections[i].va && rva + bytes <= static_cast<uint64_t>(sections[i].va) + sections[i].rawSize) {
                fileOffset = sections[i].rawOffset + (rva - sections[i].va);
                mapped = true;
                break;
            }
        }
        if (!mapped || game.widePath.empty() || bytes > 64ull * 1024 * 1024) return false;
        HANDLE file = CreateFileW(game.widePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        auto readAt = [file](uint64_t offset, void* dst, DWORD size) {
            LARGE_INTEGER at;
            at.QuadPart = static_cast<LONGLONG>(offset);
            DWORD got = 0;
            return SetFilePointerEx(file, at, nullptr, FILE_BEGIN) && ReadFile(file, dst, size, &got, nullptr) && got == size;
        };
        bool ok = false;
        std::vector<uint8_t> header(4096);
        std::vector<uint8_t> table(static_cast<size_t>(bytes));
        if (readAt(0, header.data(), static_cast<DWORD>(header.size()))) {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(header.data());
            if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0
                && static_cast<size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) <= header.size()) {
                const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(header.data() + dos->e_lfanew);
                const uint64_t diskBase = nt->OptionalHeader.ImageBase;
                if (nt->Signature == IMAGE_NT_SIGNATURE && readAt(fileOffset, table.data(), static_cast<DWORD>(bytes))) {
                    out.assign(count, 0);
                    for (size_t i = 0; i < count; ++i) {
                        uint64_t fn = 0;
                        std::memcpy(&fn, table.data() + i * sizeof(GmlEntry) + offsetof(GmlEntry, function), sizeof fn);
                        if (!fn || fn < diskBase) continue;
                        const uintptr_t loaded = static_cast<uintptr_t>(fn - diskBase) + game.base;
                        if (loaded >= game.base && loaded < game.end) out[i] = loaded;
                    }
                    ok = true;
                }
            }
        }
        CloseHandle(file);
        return ok;
    }

    const std::vector<std::pair<DWORD, const char*>>& ExportsOf(const ModuleInfo& m)
    {
        auto found = m_Exports.find(m.base);
        if (found != m_Exports.end()) return found->second;
        auto& list = m_Exports[m.base];
        if (!m.exportRva || m.exportSize < sizeof(IMAGE_EXPORT_DIRECTORY)) return list;
        if (m.base + m.exportRva + sizeof(IMAGE_EXPORT_DIRECTORY) > m.end) return list;
        const auto* dir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(m.base + m.exportRva);
        const uintptr_t functions = m.base + dir->AddressOfFunctions;
        const uintptr_t names = m.base + dir->AddressOfNames;
        const uintptr_t ordinals = m.base + dir->AddressOfNameOrdinals;
        if (functions + static_cast<uintptr_t>(dir->NumberOfFunctions) * 4 > m.end
            || names + static_cast<uintptr_t>(dir->NumberOfNames) * 4 > m.end
            || ordinals + static_cast<uintptr_t>(dir->NumberOfNames) * 2 > m.end)
            return list;
        const auto* fn = reinterpret_cast<const DWORD*>(functions);
        const auto* nm = reinterpret_cast<const DWORD*>(names);
        const auto* od = reinterpret_cast<const WORD*>(ordinals);
        list.reserve(dir->NumberOfNames);
        for (DWORD i = 0; i < dir->NumberOfNames; ++i) {
            const WORD ordinal = od[i];
            if (ordinal >= dir->NumberOfFunctions) continue;
            const DWORD fr = fn[ordinal];
            if (fr >= m.exportRva && fr < m.exportRva + m.exportSize) continue;   // a forwarder
            if (m.base + nm[i] >= m.end) continue;
            list.emplace_back(fr, reinterpret_cast<const char*>(m.base + nm[i]));
        }
        std::sort(list.begin(), list.end());
        return list;
    }

    std::unordered_map<uintptr_t, const char*> m_Gml;
    std::unordered_map<uintptr_t, std::string> m_Builtins;
    std::unordered_map<uintptr_t, std::vector<std::pair<DWORD, const char*>>> m_Exports;
    size_t m_GmlSwapped = 0;
    size_t m_GmlRestored = 0;
};

// ---- what a sample was doing ----------------------------------------------

enum class Bucket : uint8_t { Game, GameWait, Graphics, GpuWait, Mods, Runtime, Idle, Spin, Unknown, Count };

inline const char* BucketName(Bucket b) noexcept
{
    switch (b) {
    case Bucket::Game: return "game code";
    case Bucket::GameWait: return "game code waiting (files, locks)";
    case Bucket::Graphics: return "graphics driver";
    case Bucket::GpuWait: return "waiting for the GPU / display";
    case Bucket::Mods: return "mods (plugins)";
    case Bucket::Runtime: return "GameMaker runtime";
    case Bucket::Idle: return "idle (frame limiter, sleeping)";
    case Bucket::Spin: return "idle (frame limiter, spinning)";
    default: return "unknown";
    }
}

inline const char* BucketKey(Bucket b) noexcept
{
    switch (b) {
    case Bucket::Game: return "game";
    case Bucket::GameWait: return "game_wait";
    case Bucket::Graphics: return "graphics";
    case Bucket::GpuWait: return "gpu_wait";
    case Bucket::Mods: return "mods";
    case Bucket::Runtime: return "runtime";
    case Bucket::Idle: return "idle";
    case Bucket::Spin: return "spin";
    default: return "unknown";
    }
}

inline bool IsClockRead(const Symbol& s) noexcept
{
    if (s.kind != SymKind::Export) return false;
    const size_t bang = s.name.find('!');
    if (bang == std::string::npos) return false;
    const std::string fn = s.name.substr(bang + 1);
    return fn == "RtlQueryPerformanceCounter" || fn == "QueryPerformanceCounter";
}

// No game code, graphics driver or mod anywhere on the stack.
inline bool RuntimeOnly(const std::vector<Symbol>& syms, const uint32_t* ids, int n)
{
    for (int i = 0; i < n; ++i) {
        const Symbol& s = syms[ids[i]];
        if (IsGml(s.kind) || s.module == ModuleKind::Graphics || s.module == ModuleKind::Mod) return false;
    }
    return true;
}

inline constexpr uint32_t kNoSymbol = 0xFFFFFFFFu;

// The GameMaker runner can pace frames by spinning on the clock instead of
// sleeping: in town the frame thread was 96% busy at 60 fps, 38% of its
// samples in one runtime function or the clock read it made (measured
// 2026-09-28). That function is found from the data: the runtime-only
// stacks whose innermost frame is a clock read, grouped by the frame that
// called it; one caller holding at least half of them, and at least 1% of
// all samples, is taken as the frame limiter. kNoSymbol when there is none.
inline uint32_t FindSpinWait(const std::vector<Symbol>& syms, const std::vector<uint32_t>& flat,
                             const std::vector<std::pair<uint32_t, uint16_t>>& stacks,
                             const std::vector<uint64_t>& counts, uint64_t total)
{
    std::unordered_map<uint32_t, uint64_t> callers;
    uint64_t clockReads = 0;
    for (size_t s = 0; s < stacks.size(); ++s) {
        const uint32_t* ids = flat.data() + stacks[s].first;
        const int n = stacks[s].second;
        if (n < 2 || !IsClockRead(syms[ids[0]]) || !RuntimeOnly(syms, ids, n)) continue;
        callers[ids[1]] += counts[s];
        clockReads += counts[s];
    }
    uint32_t best = kNoSymbol;
    uint64_t bestCount = 0;
    for (const auto& c : callers) {
        if (c.second > bestCount || (c.second == bestCount && c.first < best)) { best = c.first; bestCount = c.second; }
    }
    if (best == kNoSymbol || bestCount * 2 < clockReads || bestCount * 100 < total) return kNoSymbol;
    return best;
}

// A runtime-only stack spent in the frame limiter: the spin function itself
// at the leaf, or a clock read it made.
inline bool IsSpinSample(const std::vector<Symbol>& syms, const uint32_t* ids, int n, uint32_t spin)
{
    if (spin == kNoSymbol || n < 1 || !RuntimeOnly(syms, ids, n)) return false;
    if (ids[0] == spin) return true;
    return n >= 2 && IsClockRead(syms[ids[0]]) && ids[1] == spin;
}

// Working time: the frame thread was running code, not blocked.
inline bool IsWorking(Bucket b) noexcept
{
    return b == Bucket::Game || b == Bucket::Graphics || b == Bucket::Mods || b == Bucket::Runtime;
}

// From the leaf outwards, the first frame that says what kind of work this
// is decides: a graphics module, game code, or a mod. Runtime and system
// frames are passed through. A blocking system call at the leaf turns the
// decision into its waiting form.
inline Bucket Classify(const std::vector<Symbol>& syms, const uint32_t* ids, int n)
{
    if (n <= 0) return Bucket::Unknown;
    const bool wait = syms[ids[0]].wait;
    for (int i = 0; i < n; ++i) {
        const Symbol& s = syms[ids[i]];
        if (s.module == ModuleKind::Graphics) return wait ? Bucket::GpuWait : Bucket::Graphics;
        if (IsGml(s.kind)) return wait ? Bucket::GameWait : Bucket::Game;
        if (s.module == ModuleKind::Mod) return Bucket::Mods;
    }
    return wait ? Bucket::Idle : Bucket::Runtime;
}

// ---- the profiler ---------------------------------------------------------

class Profiler {
public:
    // Heap-held and never freed: a process-lifetime object has nothing to run
    // at ExitProcess (see ExitSafeThread.hpp for why that matters here).
    static Profiler& Instance()
    {
        static Profiler* profiler = new Profiler();
        return *profiler;
    }

    // Frame thread. Takes ownership of params.frameThread only when it
    // returns true; on false the caller still owns (and closes) it.
    bool Start(StartParams params, std::string& why)
    {
        if (m_State.load() != State::Idle) { why = "a capture is already running"; return false; }
        if (!m_Thread.JoinFor(std::chrono::milliseconds(0))) { why = "the previous capture is still finishing"; return false; }
        if (!params.frameThread) { why = "no handle to the frame thread"; return false; }
        if (!params.stackLow || params.stackHigh <= params.stackLow) { why = "the frame thread's stack is unknown"; return false; }
        if (!(params.seconds >= kMinSeconds)) params.seconds = kMinSeconds;
        if (params.seconds > kMaxSeconds) params.seconds = kMaxSeconds;
        if (params.hz < kMinHz) params.hz = kMinHz;
        if (params.hz > kMaxHz) params.hz = kMaxHz;
        if (params.stem.empty()) params.stem = "frameprof";

        m_Params = std::move(params);
        const size_t frameCapacity = static_cast<size_t>(m_Params.seconds * 500.0) + 64;
        m_FrameQpc.assign(frameCapacity, 0);
        m_FrameCount.store(0);
        m_Contexts.assign(static_cast<size_t>(m_Params.seconds) + 8, ContextSample{});
        m_ContextCount.store(0);
        m_SampleCount.store(0);
        m_Stop.store(false);
        m_StartQpc = detail::Qpc();
        m_NextContext = 0;
        m_State.store(State::Running);
        m_Recording.store(true, std::memory_order_release);
        if (!m_Thread.Start(&Profiler::ThreadEntry)) {
            m_Recording.store(false);
            m_State.store(State::Idle);
            m_Params.frameThread = nullptr;   // still the caller's to close
            why = "the sampling thread could not be started";
            return false;
        }
        return true;
    }

    void RequestStop() noexcept { m_Stop.store(true); }

    bool Busy() const noexcept { return m_State.load() != State::Idle; }

    // Frame thread, every frame. One atomic load while no capture runs.
    void OnFrame() noexcept
    {
        if (!m_Recording.load(std::memory_order_acquire)) return;
        const uint64_t now = detail::Qpc();
        const size_t i = m_FrameCount.load(std::memory_order_relaxed);
        if (i < m_FrameQpc.size()) {
            m_FrameQpc[i] = now;
            m_FrameCount.store(i + 1, std::memory_order_release);
        }
        if (m_Params.context && now >= m_NextContext) {
            m_NextContext = now + detail::QpcFreq();
            const size_t c = m_ContextCount.load(std::memory_order_relaxed);
            if (c < m_Contexts.size()) {
                try {
                    Context x;
                    if (m_Params.context(x)) {
                        m_Contexts[c].qpc = now;
                        m_Contexts[c].context = std::move(x);
                        m_ContextCount.store(c + 1, std::memory_order_release);
                    }
                } catch (...) {}
            }
        }
    }

    // Frame thread: the summary of a capture that has just finished, once.
    bool TakeSummary(std::vector<std::string>& lines)
    {
        if (!m_SummaryReady.load(std::memory_order_acquire)) return false;
        std::lock_guard<std::mutex> lock(m_SummaryLock);
        lines.swap(m_Summary);
        m_Summary.clear();
        m_SummaryReady.store(false);
        return true;
    }

    std::string Status() const
    {
        std::ostringstream s;
        const State st = m_State.load();
        if (st == State::Running) {
            const double elapsed = static_cast<double>(detail::Qpc() - m_StartQpc) / static_cast<double>(detail::QpcFreq());
            s << "sampling the frame thread: " << detail::Fixed(elapsed, 1) << " of "
              << detail::Fixed(m_Params.seconds, 0) << " s, " << m_SampleCount.load() << " samples, "
              << m_FrameCount.load() << " frames (`frameprof stop` ends it early)";
        } else if (st == State::Reporting) {
            s << "writing the report";
        } else {
            std::lock_guard<std::mutex> lock(m_SummaryLock);
            s << "idle";
            if (!m_LastReport.empty()) s << "; last report " << m_LastReport;
            if (!m_LastFailure.empty()) s << "; last capture ended early: " << m_LastFailure;
        }
        return s.str();
    }

private:
    enum class State : int { Idle, Running, Reporting };

    struct ContextSample {
        uint64_t qpc = 0;
        Context context;
    };
    struct StackRec {
        uint32_t offset = 0;
        uint16_t depth = 0;
        uint64_t count = 0;
    };
    struct SampleRec {
        uint64_t qpc = 0;
        uint32_t stack = 0;
    };
    struct ThreadCpu {
        DWORD id = 0;
        uint64_t cpu = 0;   // 100 ns units, user + kernel
        uint64_t at = 0;    // QPC when this thread's times were read
        std::string name;
    };

    Profiler() = default;

    static void ThreadEntry() noexcept { Instance().Run(); }

    static std::vector<ThreadCpu> SnapshotThreads()
    {
        std::vector<ThreadCpu> out;
        using GetDescription = HRESULT(WINAPI*)(HANDLE, PWSTR*);
        static const auto getDescription = reinterpret_cast<GetDescription>(
            GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "GetThreadDescription"));
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap == INVALID_HANDLE_VALUE) return out;
        THREADENTRY32 te{};
        te.dwSize = sizeof te;
        const DWORD pid = GetCurrentProcessId();
        for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
            if (te.th32OwnerProcessID != pid) continue;
            HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
            if (!h) continue;
            FILETIME created{}, exited{}, kernel{}, user{};
            if (GetThreadTimes(h, &created, &exited, &kernel, &user)) {
                ThreadCpu t;
                t.at = detail::Qpc();
                t.id = te.th32ThreadID;
                t.cpu = ((static_cast<uint64_t>(kernel.dwHighDateTime) << 32) | kernel.dwLowDateTime)
                      + ((static_cast<uint64_t>(user.dwHighDateTime) << 32) | user.dwLowDateTime);
                if (getDescription) {
                    PWSTR desc = nullptr;
                    if (SUCCEEDED(getDescription(h, &desc)) && desc) {
                        t.name = detail::Narrow(desc);
                        LocalFree(desc);
                    }
                }
                out.push_back(std::move(t));
            }
            CloseHandle(h);
        }
        CloseHandle(snap);
        return out;
    }

    uint32_t InternStack(const uint64_t* keys, int n)
    {
        uint64_t h = 1469598103934665603ull;
        for (int i = 0; i < n; ++i) { h ^= keys[i]; h *= 1099511628211ull; }
        h ^= static_cast<uint64_t>(n);
        auto& candidates = m_StackByHash[h];
        for (const uint32_t id : candidates) {
            const StackRec& r = m_Stacks[id];
            if (r.depth == n && std::equal(keys, keys + n, m_Flat.begin() + r.offset)) {
                ++m_Stacks[id].count;
                return id;
            }
        }
        StackRec r;
        r.offset = static_cast<uint32_t>(m_Flat.size());
        r.depth = static_cast<uint16_t>(n);
        r.count = 1;
        m_Flat.insert(m_Flat.end(), keys, keys + n);
        m_Stacks.push_back(r);
        const uint32_t id = static_cast<uint32_t>(m_Stacks.size() - 1);
        candidates.push_back(id);
        return id;
    }

    void ResetCaptureData()
    {
        m_Flat.clear();
        m_Stacks.clear();
        m_StackByHash.clear();
        m_Samples.clear();
        m_Missed = 0;
        m_StackOutside = 0;
        m_Truncated = 0;
        m_PausedTicks = 0;
        m_PausedMaxTicks = 0;
        m_RateCuts = 0;
        m_LowestHz = 0;
        for (auto& w : m_WalkEnds) w = 0;
    }

    void Run() noexcept
    {
        std::string failure;
        ModuleMap modules;
        Symbolizer names;
        std::vector<ThreadCpu> cpuBefore, cpuAfter;
        uint64_t loopStart = 0, loopEnd = 0;
        unsigned timerMode = 0;
        try {
            ResetCaptureData();
            using SetDescription = HRESULT(WINAPI*)(HANDLE, PCWSTR);
            if (const auto setDescription = reinterpret_cast<SetDescription>(
                    GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "SetThreadDescription")))
                setDescription(GetCurrentThread(), L"ForgePact frame profiler");
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

            const uintptr_t gameBase = m_Params.gameBase ? m_Params.gameBase
                                                         : reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
            if (!modules.Build(gameBase)) throw std::runtime_error("the module list could not be read");
            m_GmlEntries = names.LoadGmlTable(m_Params.gmlAnchor, modules);
            names.AddBuiltins(m_Params.builtins);

            const size_t expected = static_cast<size_t>(m_Params.seconds * m_Params.hz) + 1024;
            m_Samples.reserve(expected);
            std::vector<uint8_t> buffer(kStackCopyBytes + 64);
            uint8_t* copy = reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(buffer.data()) + 15) & ~static_cast<uintptr_t>(15));

            HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
            timerMode = timer ? 2 : 0;
            if (!timer) { timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS); timerMode = timer ? 1 : 0; }
            const uint64_t freq = detail::QpcFreq();
            unsigned hz = m_Params.hz;
            uint64_t step = freq / hz;
            m_LowestHz = hz;

            cpuBefore = SnapshotThreads();
            loopStart = detail::Qpc();
            uint64_t budgetStart = loopStart, budgetPaused = 0;
            // The requested length is sampled in full: it counts from here,
            // after the module list and the names are ready.
            const uint64_t deadline = loopStart + static_cast<uint64_t>(m_Params.seconds * static_cast<double>(freq));
            uint64_t next = loopStart;
            uint64_t keys[kMaxFrames];
            int consecutiveFailures = 0;
            while (!m_Stop.load(std::memory_order_relaxed)) {
                // A fixed schedule, so the time a sample takes does not lower
                // the rate; after a stall it restarts from now rather than
                // firing the missed ticks back to back.
                next += step;
                uint64_t now = detail::Qpc();
                if (next >= deadline || now >= deadline) break;
                if (now > next + 4 * step) next = now;
                if (next > now) {
                    const LONGLONG wait100ns = static_cast<LONGLONG>((next - now) * 10000000ull / freq);
                    if (timer && wait100ns > 0) {
                        LARGE_INTEGER due;
                        due.QuadPart = -wait100ns;
                        if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) WaitForSingleObject(timer, 100);
                        else Sleep(1);
                    } else if (wait100ns > 0) {
                        Sleep(1);
                    }
                }
                CONTEXT ctx{};
                size_t copied = 0;
                bool truncated = false;
                uint64_t paused = 0, at = 0;
                const CaptureResult r = CaptureOnce(m_Params.frameThread, m_Params.stackLow, m_Params.stackHigh,
                                                    copy, kStackCopyBytes, ctx, copied, truncated, paused, at);
                if (r == CaptureResult::SuspendFailed) {
                    failure = "the frame thread could not be paused (it may have ended)";
                    break;
                }
                if (r == CaptureResult::ContextFailed) {
                    ++m_Missed;
                    if (++consecutiveFailures > 250) { failure = "the frame thread's registers could not be read"; break; }
                    continue;
                }
                consecutiveFailures = 0;
                m_PausedTicks += paused;
                if (paused > m_PausedMaxTicks) m_PausedMaxTicks = paused;
                budgetPaused += paused;
                if (at - budgetStart >= freq / 2) {
                    const double share = static_cast<double>(budgetPaused) / static_cast<double>(at - budgetStart);
                    if (share > kPauseBudget && hz > kMinHz) {
                        hz = hz / 2 < kMinHz ? kMinHz : hz / 2;
                        ++m_RateCuts;
                    } else if (share < kPauseBudget / 4 && hz < m_Params.hz) {
                        hz = hz * 2 > m_Params.hz ? m_Params.hz : hz * 2;
                    }
                    step = freq / hz;
                    if (hz < m_LowestHz) m_LowestHz = hz;
                    budgetStart = at;
                    budgetPaused = 0;
                }
                if (r == CaptureResult::StackOutside) ++m_StackOutside;
                if (truncated) ++m_Truncated;
                WalkEnd end = WalkEnd::Complete;
                const int n = WalkCopy(ctx, copy, copied, modules, keys, kMaxFrames, end);
                ++m_WalkEnds[static_cast<size_t>(end)];
                for (int i = 0; i < n; ++i) keys[i] = modules.FunctionKey(static_cast<uintptr_t>(keys[i]));
                SampleRec rec;
                rec.qpc = at;
                rec.stack = InternStack(keys, n);
                m_Samples.push_back(rec);
                m_SampleCount.fetch_add(1, std::memory_order_relaxed);
            }
            loopEnd = detail::Qpc();
            if (timer) CloseHandle(timer);
            cpuAfter = SnapshotThreads();
        } catch (const std::exception& e) {
            failure = std::string("internal error: ") + e.what();
        } catch (...) {
            failure = "internal error";
        }
        if (!loopEnd) loopEnd = detail::Qpc();
        if (!loopStart) loopStart = loopEnd;
        m_Recording.store(false, std::memory_order_release);
        m_State.store(State::Reporting);

        std::vector<std::string> summary;
        std::string reportPath;
        try {
            summary = BuildReport(modules, names, cpuBefore, cpuAfter, loopStart, loopEnd, timerMode, failure, reportPath);
        } catch (const std::exception& e) {
            summary = { std::string("frameprof: the report could not be written: ") + e.what() };
        } catch (...) {
            summary = { "frameprof: the report could not be written" };
        }
        if (m_Params.frameThread) {
            CloseHandle(m_Params.frameThread);
            m_Params.frameThread = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(m_SummaryLock);
            m_Summary = std::move(summary);
            m_LastReport = reportPath;
            m_LastFailure = failure;
        }
        m_SummaryReady.store(true, std::memory_order_release);
        m_State.store(State::Idle);
    }

    struct Tally {
        std::unordered_map<std::string, uint64_t> counts;
        void Add(const std::string& label, uint64_t n) { counts[label] += n; }
        std::vector<std::pair<std::string, uint64_t>> Top(size_t rows) const
        {
            std::vector<std::pair<std::string, uint64_t>> v(counts.begin(), counts.end());
            std::sort(v.begin(), v.end(), [](const auto& a, const auto& b) {
                return a.second != b.second ? a.second > b.second : a.first < b.first;
            });
            if (v.size() > rows) v.resize(rows);
            return v;
        }
    };

    static std::string TopText(const std::vector<std::pair<std::string, uint64_t>>& rows, uint64_t whole, size_t count)
    {
        std::string s;
        for (size_t i = 0; i < rows.size() && i < count; ++i) {
            if (i) s += " | ";
            s += rows[i].first + " " + detail::Fixed(detail::Percent(rows[i].second, whole), 1) + "%";
        }
        return s.empty() ? "(none)" : s;
    }

    static void WriteRows(std::ostringstream& j, const char* key, const std::vector<std::pair<std::string, uint64_t>>& rows,
                          uint64_t all, uint64_t working)
    {
        j << ",\"" << key << "\":[";
        for (size_t i = 0; i < rows.size(); ++i) {
            if (i) j << ',';
            j << "{\"name\":\"" << detail::JsonEscape(rows[i].first) << "\",\"samples\":" << rows[i].second
              << ",\"percent\":" << detail::Fixed(detail::Percent(rows[i].second, all), 2)
              << ",\"percentOfWorking\":" << detail::Fixed(detail::Percent(rows[i].second, working), 2) << '}';
        }
        j << ']';
    }

    std::vector<std::string> BuildReport(const ModuleMap& modules, Symbolizer& names,
                                         const std::vector<ThreadCpu>& cpuBefore, const std::vector<ThreadCpu>& cpuAfter,
                                         uint64_t loopStart, uint64_t loopEnd, unsigned timerMode,
                                         const std::string& failure, std::string& reportPath)
    {
        const uint64_t freq = detail::QpcFreq();
        const double wall = static_cast<double>(loopEnd - loopStart) / static_cast<double>(freq);

        // Name every distinct function once.
        std::unordered_map<uint64_t, uint32_t> symIndex;
        std::vector<Symbol> syms;
        std::vector<uint32_t> symFlat(m_Flat.size());
        for (size_t i = 0; i < m_Flat.size(); ++i) {
            const auto found = symIndex.find(m_Flat[i]);
            if (found != symIndex.end()) { symFlat[i] = found->second; continue; }
            const uint32_t id = static_cast<uint32_t>(syms.size());
            syms.push_back(names.Resolve(m_Flat[i], modules));
            symIndex.emplace(m_Flat[i], id);
            symFlat[i] = id;
        }

        // A frame limiter that spins on the clock, if the runner has one.
        std::vector<std::pair<uint32_t, uint16_t>> spans;
        std::vector<uint64_t> spanCounts;
        spans.reserve(m_Stacks.size());
        spanCounts.reserve(m_Stacks.size());
        for (const StackRec& r : m_Stacks) {
            spans.emplace_back(r.offset, r.depth);
            spanCounts.push_back(r.count);
        }
        const uint32_t spin = FindSpinWait(syms, symFlat, spans, spanCounts, m_Samples.size());

        // Per stack: its bucket and what it adds to each table.
        std::vector<Bucket> stackBucket(m_Stacks.size(), Bucket::Unknown);
        uint64_t bucketSamples[static_cast<size_t>(Bucket::Count)] = {};
        Tally gmlTotal, gmlSelf, events, builtins, leaves, leafModules;
        uint64_t total = 0;
        std::vector<std::string> seen;
        for (size_t s = 0; s < m_Stacks.size(); ++s) {
            const StackRec& r = m_Stacks[s];
            const uint32_t* ids = symFlat.data() + r.offset;
            const int n = r.depth;
            const Bucket b = IsSpinSample(syms, ids, n, spin) ? Bucket::Spin : Classify(syms, ids, n);
            stackBucket[s] = b;
            bucketSamples[static_cast<size_t>(b)] += r.count;
            total += r.count;
            if (!n) continue;
            leaves.Add(syms[ids[0]].label, r.count);
            leafModules.Add(syms[ids[0]].moduleName.empty() ? "unknown code" : syms[ids[0]].moduleName, r.count);
            bool self = false;
            const Symbol* outerEvent = nullptr;
            seen.clear();
            for (int i = 0; i < n; ++i) {
                const Symbol& sym = syms[ids[i]];
                if (IsGml(sym.kind)) {
                    if (!self) { gmlSelf.Add(sym.label, r.count); self = true; }
                    if (sym.kind == SymKind::GmlEvent) outerEvent = &sym;
                    if (std::find(seen.begin(), seen.end(), sym.label) == seen.end()) {
                        seen.push_back(sym.label);
                        gmlTotal.Add(sym.label, r.count);
                    }
                } else if (sym.kind == SymKind::Builtin) {
                    const std::string key = "\x01" + sym.label;
                    if (std::find(seen.begin(), seen.end(), key) == seen.end()) {
                        seen.push_back(key);
                        builtins.Add(sym.label, r.count);
                    }
                }
            }
            if (outerEvent) events.Add(outerEvent->label, r.count);
        }
        uint64_t working = 0, waiting = 0;
        for (size_t b = 0; b < static_cast<size_t>(Bucket::Count); ++b) {
            if (IsWorking(static_cast<Bucket>(b))) working += bucketSamples[b];
            else if (static_cast<Bucket>(b) != Bucket::Unknown) waiting += bucketSamples[b];
        }

        // Frames: the ones that ended inside the sampled window, so the frame
        // numbers and the samples describe the same stretch of play.
        std::vector<uint64_t> fq;
        {
            const size_t recorded = m_FrameCount.load(std::memory_order_acquire);
            fq.reserve(recorded);
            for (size_t i = 0; i < recorded; ++i)
                if (m_FrameQpc[i] >= loopStart && m_FrameQpc[i] <= loopEnd) fq.push_back(m_FrameQpc[i]);
        }
        const size_t frameCount = fq.size();
        std::vector<double> frameMs;
        frameMs.reserve(frameCount);
        for (size_t i = 1; i < frameCount; ++i)
            frameMs.push_back(static_cast<double>(fq[i] - fq[i - 1]) * 1000.0 / static_cast<double>(freq));
        std::vector<double> sorted = frameMs;
        std::sort(sorted.begin(), sorted.end());
        auto pct = [&](double p) {
            if (sorted.empty()) return 0.0;
            size_t idx = static_cast<size_t>(p / 100.0 * static_cast<double>(sorted.size() - 1) + 0.5);
            if (idx >= sorted.size()) idx = sorted.size() - 1;
            return sorted[idx];
        };
        const double frameSpan = frameCount > 1
            ? static_cast<double>(fq[frameCount - 1] - fq[0]) / static_cast<double>(freq) : 0.0;
        const double fps = frameSpan > 0 ? static_cast<double>(frameCount - 1) / frameSpan : 0.0;
        double meanMs = 0;
        for (double v : frameMs) meanMs += v;
        if (!frameMs.empty()) meanMs /= static_cast<double>(frameMs.size());
        size_t over33 = 0, over50 = 0, over100 = 0, over250 = 0;
        for (double v : frameMs) {
            if (v > 33.4) ++over33;
            if (v > 50.0) ++over50;
            if (v > 100.0) ++over100;
            if (v > 250.0) ++over250;
        }

        // The slowest frames, each with what ran during it.
        std::vector<size_t> slow;
        for (size_t i = 0; i < frameMs.size(); ++i) if (frameMs[i] > kHitchMs) slow.push_back(i);
        std::sort(slow.begin(), slow.end(), [&](size_t a, size_t b) { return frameMs[a] > frameMs[b]; });
        if (slow.size() > 10) slow.resize(10);
        struct Hitch {
            double at = 0, ms = 0;
            uint64_t samples = 0;
            std::string room;
            std::vector<std::pair<std::string, uint64_t>> gml, ev, bk;
        };
        std::vector<Hitch> hitches;
        const size_t contextCount = m_ContextCount.load(std::memory_order_acquire);
        auto roomAt = [&](uint64_t qpc) {
            std::string room;
            for (size_t c = 0; c < contextCount; ++c) {
                if (m_Contexts[c].qpc > qpc) break;
                room = m_Contexts[c].context.room;
            }
            return room;
        };
        for (const size_t fi : slow) {
            const uint64_t from = fq[fi], to = fq[fi + 1];
            Hitch h;
            h.at = static_cast<double>(to - loopStart) / static_cast<double>(freq);
            h.ms = frameMs[fi];
            h.room = roomAt(to);
            Tally g, e, bk;
            auto it = std::lower_bound(m_Samples.begin(), m_Samples.end(), from,
                                       [](const SampleRec& s, uint64_t v) { return s.qpc < v; });
            for (; it != m_Samples.end() && it->qpc <= to; ++it) {
                ++h.samples;
                const StackRec& r = m_Stacks[it->stack];
                const uint32_t* ids = symFlat.data() + r.offset;
                bk.Add(BucketName(stackBucket[it->stack]), 1);
                seen.clear();
                const Symbol* outerEvent = nullptr;
                for (int i = 0; i < r.depth; ++i) {
                    const Symbol& sym = syms[ids[i]];
                    if (!IsGml(sym.kind)) continue;
                    if (sym.kind == SymKind::GmlEvent) outerEvent = &sym;
                    if (std::find(seen.begin(), seen.end(), sym.label) == seen.end()) {
                        seen.push_back(sym.label);
                        g.Add(sym.label, 1);
                    }
                }
                if (outerEvent) e.Add(outerEvent->label, 1);
            }
            h.gml = g.Top(6);
            h.ev = e.Top(4);
            h.bk = bk.Top(4);
            hitches.push_back(std::move(h));
        }

        // One row per second.
        struct Second {
            uint64_t frames = 0, samples = 0, workingSamples = 0;
            double sumMs = 0, maxMs = 0;
            bool hasContext = false;
            Context context;
        };
        const size_t seconds = static_cast<size_t>(wall) + 2;
        std::vector<Second> timeline(seconds);
        auto secondOf = [&](uint64_t qpc) -> size_t {
            if (qpc < loopStart) return 0;
            const size_t s = static_cast<size_t>((qpc - loopStart) / freq);
            return s < timeline.size() ? s : timeline.size() - 1;
        };
        for (size_t i = 0; i < frameMs.size(); ++i) {
            Second& sec = timeline[secondOf(fq[i + 1])];
            ++sec.frames;
            sec.sumMs += frameMs[i];
            if (frameMs[i] > sec.maxMs) sec.maxMs = frameMs[i];
        }
        for (const SampleRec& s : m_Samples) {
            Second& sec = timeline[secondOf(s.qpc)];
            ++sec.samples;
            if (IsWorking(stackBucket[s.stack])) ++sec.workingSamples;
        }
        for (size_t c = 0; c < contextCount; ++c) {
            Second& sec = timeline[secondOf(m_Contexts[c].qpc)];
            sec.hasContext = true;
            sec.context = m_Contexts[c].context;
        }
        while (!timeline.empty() && !timeline.back().samples && !timeline.back().frames) timeline.pop_back();

        // CPU per thread over the capture.
        struct ThreadRow {
            DWORD id = 0;
            double percentOfCore = 0;
            std::string name;
            bool frame = false, profiler = false;
        };
        // Each thread's own two readings bound its window, so a slow thread
        // list cannot stretch or shrink it; a thread born during the capture
        // counts from the sampling start. Windows charges thread time in clock
        // ticks, so short captures read a few percent off.
        std::vector<ThreadRow> threads;
        const DWORD self = GetCurrentThreadId();
        double otherThreads = 0, allThreads = 0, frameThread = 0, profilerThread = 0;
        for (const ThreadCpu& a : cpuAfter) {
            const ThreadCpu* before = nullptr;
            for (const ThreadCpu& b : cpuBefore) if (b.id == a.id) { before = &b; break; }
            const uint64_t since = before ? before->at : loopStart;
            const uint64_t used = before ? (a.cpu >= before->cpu ? a.cpu - before->cpu : 0) : a.cpu;
            const double window = a.at > since ? static_cast<double>(a.at - since) / static_cast<double>(freq) : 0.0;
            ThreadRow row;
            row.id = a.id;
            row.name = a.name;
            row.percentOfCore = window > 0.25 ? 100.0 * (static_cast<double>(used) / 1e7) / window : 0.0;
            row.frame = a.id == m_Params.frameThreadId;
            row.profiler = a.id == self;
            allThreads += row.percentOfCore;
            if (row.frame) frameThread = row.percentOfCore;
            else if (row.profiler) profilerThread = row.percentOfCore;
            else otherThreads += row.percentOfCore;
            threads.push_back(std::move(row));
        }
        std::sort(threads.begin(), threads.end(), [](const ThreadRow& a, const ThreadRow& b) { return a.percentOfCore > b.percentOfCore; });
        if (threads.size() > 24) threads.resize(24);

        const double pausedUs = m_Samples.empty() ? 0.0
            : static_cast<double>(m_PausedTicks) * 1e6 / static_cast<double>(freq) / static_cast<double>(m_Samples.size());
        const double pausedMaxUs = static_cast<double>(m_PausedMaxTicks) * 1e6 / static_cast<double>(freq);
        const double pausedShare = wall > 0 ? 100.0 * static_cast<double>(m_PausedTicks) / static_cast<double>(freq) / wall : 0.0;

        const auto gmlTotalTop = gmlTotal.Top(kTopRows);
        const auto gmlSelfTop = gmlSelf.Top(kTopRows);
        const auto eventsTop = events.Top(kTopRows);
        const auto builtinsTop = builtins.Top(kTopRows);
        const auto leavesTop = leaves.Top(kTopRows);
        const auto modulesTop = leafModules.Top(kTopRows);

        // Files.
        std::error_code ec;
        std::filesystem::create_directories(m_Params.outDir, ec);
        const std::filesystem::path jsonPath = m_Params.outDir / (m_Params.stem + ".json");
        const std::filesystem::path stacksPath = m_Params.outDir / (m_Params.stem + ".stacks.txt");
        const std::filesystem::path textPath = m_Params.outDir / (m_Params.stem + ".txt");

        SYSTEMTIME lt{};
        GetLocalTime(&lt);
        char stamp[40];
        snprintf(stamp, sizeof stamp, "%04u-%02u-%02uT%02u:%02u:%02u", lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond);

        std::ostringstream j;
        j << "{\"schema\":\"forgepact-frameprof/1\"";
        j << ",\"tool\":\"ForgePact " << detail::JsonEscape(m_Params.toolVersion) << "\"";
        j << ",\"finished\":\"" << stamp << "\"";
        const ModuleInfo* game = modules.Game();
        j << ",\"game\":{\"module\":\"" << detail::JsonEscape(game ? game->name : "") << "\",\"bytes\":" << (game ? game->end - game->base : 0)
          << ",\"gmlEntries\":" << m_GmlEntries << ",\"gmlNamed\":" << names.GmlCount()
          << ",\"gmlSwappedByMods\":" << names.GmlSwapped() << ",\"gmlRestoredFromDisk\":" << names.GmlRestored()
          << ",\"builtinsNamed\":" << names.BuiltinCount() << '}';
        j << ",\"capture\":{\"requestedSeconds\":" << detail::Fixed(m_Params.seconds, 1)
          << ",\"seconds\":" << detail::Fixed(wall, 3) << ",\"hz\":" << m_Params.hz
          << ",\"hzLowest\":" << m_LowestHz << ",\"rateCuts\":" << m_RateCuts
          << ",\"pauseBudgetPercent\":" << detail::Fixed(kPauseBudget * 100.0, 1)
          << ",\"samples\":" << m_Samples.size() << ",\"effectiveHz\":" << detail::Fixed(wall > 0 ? static_cast<double>(m_Samples.size()) / wall : 0.0, 1)
          << ",\"missed\":" << m_Missed << ",\"stackOutside\":" << m_StackOutside << ",\"truncatedStacks\":" << m_Truncated
          << ",\"pauseUsAvg\":" << detail::Fixed(pausedUs, 2) << ",\"pauseUsMax\":" << detail::Fixed(pausedMaxUs, 1)
          << ",\"pausePercentOfTime\":" << detail::Fixed(pausedShare, 3)
          << ",\"timer\":\"" << (timerMode == 2 ? "high-resolution" : timerMode == 1 ? "standard" : "sleep") << "\""
          << ",\"uniqueStacks\":" << m_Stacks.size() << ",\"walkEnds\":{";
        for (size_t w = 0; w < static_cast<size_t>(WalkEnd::Count); ++w) {
            if (w) j << ',';
            j << '"' << WalkEndName(static_cast<WalkEnd>(w)) << "\":" << m_WalkEnds[w];
        }
        j << "},\"endedEarly\":\"" << detail::JsonEscape(failure) << "\"}";
        j << ",\"frames\":{\"count\":" << frameCount << ",\"fps\":" << detail::Fixed(fps, 2)
          << ",\"msMean\":" << detail::Fixed(meanMs, 2) << ",\"msP50\":" << detail::Fixed(pct(50), 2)
          << ",\"msP90\":" << detail::Fixed(pct(90), 2) << ",\"msP95\":" << detail::Fixed(pct(95), 2)
          << ",\"msP99\":" << detail::Fixed(pct(99), 2) << ",\"msMax\":" << detail::Fixed(sorted.empty() ? 0.0 : sorted.back(), 2)
          << ",\"over33ms\":" << over33 << ",\"over50ms\":" << over50 << ",\"over100ms\":" << over100
          << ",\"over250ms\":" << over250 << '}';
        j << ",\"time\":{\"workingPercent\":" << detail::Fixed(detail::Percent(working, total), 2)
          << ",\"waitingPercent\":" << detail::Fixed(detail::Percent(waiting, total), 2) << '}';
        const uint64_t spinSamples = bucketSamples[static_cast<size_t>(Bucket::Spin)];
        if (spin != kNoSymbol) {
            j << ",\"spinWait\":{\"function\":\"" << detail::JsonEscape(syms[spin].label) << "\",\"samples\":" << spinSamples
              << ",\"percent\":" << detail::Fixed(detail::Percent(spinSamples, total), 2) << '}';
        } else {
            j << ",\"spinWait\":null";
        }
        j << ",\"buckets\":[";
        for (size_t b = 0; b < static_cast<size_t>(Bucket::Count); ++b) {
            if (b) j << ',';
            j << "{\"key\":\"" << BucketKey(static_cast<Bucket>(b)) << "\",\"name\":\"" << BucketName(static_cast<Bucket>(b))
              << "\",\"samples\":" << bucketSamples[b] << ",\"percent\":" << detail::Fixed(detail::Percent(bucketSamples[b], total), 2) << '}';
        }
        j << ']';
        WriteRows(j, "gmlTotal", gmlTotalTop, total, working);
        WriteRows(j, "gmlSelf", gmlSelfTop, total, working);
        WriteRows(j, "events", eventsTop, total, working);
        WriteRows(j, "builtins", builtinsTop, total, working);
        WriteRows(j, "leafFunctions", leavesTop, total, working);
        WriteRows(j, "leafModules", modulesTop, total, working);
        j << ",\"hitches\":[";
        for (size_t i = 0; i < hitches.size(); ++i) {
            const Hitch& h = hitches[i];
            if (i) j << ',';
            j << "{\"atSeconds\":" << detail::Fixed(h.at, 3) << ",\"ms\":" << detail::Fixed(h.ms, 2)
              << ",\"room\":\"" << detail::JsonEscape(h.room) << "\",\"samples\":" << h.samples;
            auto rows = [&](const char* key, const std::vector<std::pair<std::string, uint64_t>>& v) {
                j << ",\"" << key << "\":[";
                for (size_t k = 0; k < v.size(); ++k) {
                    if (k) j << ',';
                    j << "{\"name\":\"" << detail::JsonEscape(v[k].first) << "\",\"samples\":" << v[k].second << '}';
                }
                j << ']';
            };
            rows("gmlTotal", h.gml);
            rows("events", h.ev);
            rows("buckets", h.bk);
            j << '}';
        }
        j << ']';
        j << ",\"timeline\":[";
        for (size_t s = 0; s < timeline.size(); ++s) {
            const Second& sec = timeline[s];
            if (s) j << ',';
            j << "{\"second\":" << s << ",\"frames\":" << sec.frames
              << ",\"msMean\":" << detail::Fixed(sec.frames ? sec.sumMs / static_cast<double>(sec.frames) : 0.0, 2)
              << ",\"msMax\":" << detail::Fixed(sec.maxMs, 2) << ",\"samples\":" << sec.samples
              << ",\"workingPercent\":" << detail::Fixed(detail::Percent(sec.workingSamples, sec.samples), 1);
            if (sec.hasContext) {
                j << ",\"room\":\"" << detail::JsonEscape(sec.context.room) << "\",\"instances\":" << sec.context.instances
                  << ",\"monsters\":" << sec.context.monsters;
            }
            j << '}';
        }
        j << ']';
        j << ",\"threads\":{\"frameThreadPercentOfCore\":" << detail::Fixed(frameThread, 1)
          << ",\"otherThreadsPercentOfCore\":" << detail::Fixed(otherThreads, 1)
          << ",\"profilerThreadPercentOfCore\":" << detail::Fixed(profilerThread, 1)
          << ",\"allThreadsPercentOfCore\":" << detail::Fixed(allThreads, 1) << ",\"top\":[";
        for (size_t i = 0; i < threads.size(); ++i) {
            if (i) j << ',';
            j << "{\"id\":" << threads[i].id << ",\"name\":\"" << detail::JsonEscape(threads[i].name)
              << "\",\"percentOfCore\":" << detail::Fixed(threads[i].percentOfCore, 1)
              << ",\"frameThread\":" << (threads[i].frame ? "true" : "false")
              << ",\"profiler\":" << (threads[i].profiler ? "true" : "false") << '}';
        }
        j << "]}";
        j << ",\"files\":{\"stacks\":\"" << detail::JsonEscape(stacksPath.filename().string()) << "\",\"text\":\""
          << detail::JsonEscape(textPath.filename().string()) << "\"}";
        j << "}\n";
        {
            std::ofstream f(jsonPath, std::ios::binary | std::ios::trunc);
            f << j.str();
        }
        {
            // Collapsed stacks, outermost first: the format flame-graph tools read.
            std::ofstream f(stacksPath, std::ios::binary | std::ios::trunc);
            std::string line;
            for (const StackRec& r : m_Stacks) {
                line.clear();
                const uint32_t* ids = symFlat.data() + r.offset;
                for (int i = r.depth - 1; i >= 0; --i) {
                    std::string label = syms[ids[i]].label;
                    for (char& c : label) if (c == ';' || c == '\n' || c == '\r') c = ',';
                    if (!line.empty()) line += ';';
                    line += label;
                }
                if (line.empty()) line = "(no frames)";
                f << line << ' ' << r.count << '\n';
            }
        }

        // The lines the frame thread prints to out.txt, also kept as <stem>.txt.
        std::vector<std::string> lines;
        {
            std::string head = "frameprof: " + detail::Fixed(wall, 1) + " s, " + std::to_string(m_Samples.size()) + " samples";
            if (!failure.empty()) head += " (ended early: " + failure + ")";
            lines.push_back(head);
        }
        lines.push_back("frameprof: " + std::to_string(frameCount) + " frames, " + detail::Fixed(fps, 1) + " fps; frame time median "
                        + detail::Fixed(pct(50), 1) + " ms, 95% under " + detail::Fixed(pct(95), 1) + " ms, worst "
                        + detail::Fixed(sorted.empty() ? 0.0 : sorted.back(), 1) + " ms; " + std::to_string(over50)
                        + " frames over 50 ms");
        lines.push_back("frameprof: frame thread working " + detail::Fixed(detail::Percent(working, total), 0) + "%, waiting "
                        + detail::Fixed(detail::Percent(waiting, total), 0) + "%; CPU: frame thread "
                        + detail::Fixed(frameThread, 0) + "% of one core, all other game threads "
                        + detail::Fixed(otherThreads, 0) + "% of one core combined");
        {
            std::string b = "frameprof: time split -";
            for (size_t k = 0; k < static_cast<size_t>(Bucket::Count); ++k) {
                if (!bucketSamples[k]) continue;
                b += " " + std::string(BucketName(static_cast<Bucket>(k))) + " "
                   + detail::Fixed(detail::Percent(bucketSamples[k], total), 1) + "%;";
            }
            lines.push_back(b);
        }
        if (spin != kNoSymbol) {
            lines.push_back("frameprof: the frame limiter spins instead of sleeping - "
                            + detail::Fixed(detail::Percent(spinSamples, total), 1) + "% of the frame thread's time, in "
                            + syms[spin].label + " (counted as waiting)");
        }
        lines.push_back("frameprof: heaviest events (with everything they call): " + TopText(eventsTop, total, 6));
        lines.push_back("frameprof: heaviest game code (with everything it calls): " + TopText(gmlTotalTop, total, 8));
        lines.push_back("frameprof: heaviest built-ins: " + TopText(builtinsTop, total, 6));
        if (!hitches.empty()) {
            // What most of the slow frame was (a GPU wait looks nothing like a
            // slow script), then the game code that ran in it.
            const Hitch& h = hitches.front();
            const std::string mostly = h.bk.empty() ? std::string()
                : "mostly " + h.bk.front().first + " (" + std::to_string(h.bk.front().second) + " of "
                  + std::to_string(h.samples) + " samples); ";
            lines.push_back("frameprof: worst frame " + detail::Fixed(h.ms, 0) + " ms at " + detail::Fixed(h.at, 1) + " s"
                            + (h.room.empty() ? std::string() : " in " + h.room) + ": " + mostly + "game code: "
                            + TopText(h.gml, h.samples ? h.samples : 1, 4));
        }
        lines.push_back("frameprof: each sample paused the game at most " + detail::Fixed(pausedUs, 1) + " us (at most "
                        + detail::Fixed(pausedShare, 2) + "% of the time"
                        + (m_RateCuts ? "; sampling slowed to " + std::to_string(m_LowestHz)
                                            + " a second at times to keep that under "
                                            + detail::Fixed(kPauseBudget * 100.0, 0) + "%"
                                      : std::string())
                        + "); names: " + std::to_string(names.GmlCount()) + " game functions ("
                        + std::to_string(names.GmlRestored()) + " of " + std::to_string(names.GmlSwapped())
                        + " rows a mod had swapped read from the exe), " + std::to_string(names.BuiltinCount())
                        + " built-ins");
        reportPath = jsonPath.string();
        lines.push_back("frameprof: report " + reportPath);
        {
            std::ofstream f(textPath, std::ios::binary | std::ios::trunc);
            for (const auto& l : lines) f << l << "\r\n";
        }
        return lines;
    }

    // Settings of the current or last capture. Written only by Start, on the
    // frame thread, while no sampler thread runs.
    StartParams m_Params;
    ExitSafeThread m_Thread;
    std::atomic<State> m_State{State::Idle};
    std::atomic<bool> m_Recording{false};
    std::atomic<bool> m_Stop{false};
    std::atomic<bool> m_SummaryReady{false};
    uint64_t m_StartQpc = 0;

    // Frame thread writes, sampler thread reads after m_Recording is cleared.
    std::vector<uint64_t> m_FrameQpc;
    std::atomic<size_t> m_FrameCount{0};
    std::vector<ContextSample> m_Contexts;
    std::atomic<size_t> m_ContextCount{0};
    uint64_t m_NextContext = 0;

    // Sampler thread only.
    std::vector<uint64_t> m_Flat;
    std::vector<StackRec> m_Stacks;
    std::unordered_map<uint64_t, std::vector<uint32_t>> m_StackByHash;
    std::vector<SampleRec> m_Samples;
    std::atomic<uint64_t> m_SampleCount{0};
    uint64_t m_Missed = 0, m_StackOutside = 0, m_Truncated = 0;
    uint64_t m_PausedTicks = 0, m_PausedMaxTicks = 0;
    uint64_t m_RateCuts = 0;
    unsigned m_LowestHz = 0;
    uint64_t m_WalkEnds[static_cast<size_t>(WalkEnd::Count)] = {};
    size_t m_GmlEntries = 0;

    mutable std::mutex m_SummaryLock;
    std::vector<std::string> m_Summary;
    std::string m_LastReport;
    std::string m_LastFailure;
};

} // namespace ForgePact::FrameProfiler
