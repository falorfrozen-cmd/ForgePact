// A small DLL that tests/test_release_pdb_symbols.py compiles three times with
// build.bat's own compile options: plain, under the CL / _LINK_ values the
// release workflow's compile step sets (forgepact-release.yml, issue #76), and
// once with optimisation off as the negative control. No game, Aurie or
// YYToolkit is involved: what is under test is whether asking cl and link for
// a PDB changes the code they emit.
//
// It carries the shapes whose code generation could plausibly move when debug
// information is on, each touching read-only or writable data so the
// relocations into that data are part of what the test compares: string
// tables, a switch dense enough for a jump table, a virtual call, an
// exception that crosses a frame, the standard containers, a Win32 import, and
// a namespace-scope object with a destructor (the shape of the incident
// monitor's clean-shutdown marker).

#include <windows.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const char* const kNames[] = {"density", "mapreveal", "drops", "autoprospect", "hudlabels", "farsleep"};

volatile std::uint32_t g_Calls = 0;

struct Scorer {
    virtual ~Scorer() = default;
    virtual std::uint32_t Score(std::uint32_t value) const = 0;
};

struct Doubler final : Scorer {
    std::uint32_t Score(std::uint32_t value) const override { return value * 2u + 1u; }
};

struct Tripler final : Scorer {
    std::uint32_t Score(std::uint32_t value) const override { return value * 3u + g_Calls; }
};

std::uint32_t Pick(std::uint32_t kind, std::uint32_t value) {
    switch (kind) {
    case 0: return value + 11u;
    case 1: return value ^ 0x5Au;
    case 2: return value * 7u;
    case 3: return value >> 2;
    case 4: return value - 3u;
    case 5: return value | 0x100u;
    case 6: return value & 0xFFu;
    case 7: return value * value;
    default: return 0u;
    }
}

std::string Describe(std::uint32_t kind) {
    if (kind >= sizeof(kNames) / sizeof(kNames[0])) {
        throw std::out_of_range("no such mod");
    }
    return std::string("mod ") + kNames[kind];
}

struct ExitMarker {
    ~ExitMarker() {
        char line[32] = "probe done\r\n";
        line[0] = static_cast<char>('p' + (g_Calls & 1u));
        DWORD written = 0;
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        if (out != nullptr && out != INVALID_HANDLE_VALUE) {
            WriteFile(out, line, static_cast<DWORD>(lstrlenA(line)), &written, nullptr);
        }
    }
};

ExitMarker g_ExitMarker;

}  // namespace

extern "C" __declspec(dllexport) std::uint32_t ProbeRun(std::uint32_t seed) {
    ++g_Calls;
    Doubler doubler;
    Tripler tripler;
    const Scorer* scorers[] = {&doubler, &tripler};
    std::vector<std::uint32_t> values;
    for (std::uint32_t i = 0; i < 16u; ++i) {
        values.push_back(scorers[(seed + i) & 1u]->Score(Pick((seed + i) % 9u, seed ^ i)));
    }
    std::uint32_t total = static_cast<std::uint32_t>(GetTickCount());
    for (std::uint32_t value : values) {
        try {
            total += static_cast<std::uint32_t>(Describe(value % 8u).size());
        } catch (const std::out_of_range&) {
            total ^= value;
        }
    }
    return total;
}
