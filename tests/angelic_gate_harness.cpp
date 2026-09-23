// Behaviour harness for the Angelic gate finder (#69).
//
// test_angelic_gate_behavior.py injects the production ScriptCode,
// GameScriptCode, AngelicScriptCodeReady, CaptureAngelicScriptCode and
// FindAngelicGate at PRODUCTION_GATE. Everything they touch is modelled here:
// a fake game image holding DropItem (with the guarded call to
// DropItemAngelicChance) and DropItemAngelicChance itself, a fake plugin
// image standing in for ForgePact's own hook bodies, and the two
// script-table entries HookOneScript swaps when DropManager (`dropmult`) or
// `angelicwatch` hooks those scripts.
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using PVOID = void*;
using HMODULE = void*;
using AurieStatus = int;
static bool AurieSuccess(AurieStatus status) { return status == 0; }

// Both images are int3-filled and equally large, so a scan that wanders into
// the plugin reads ForgePact's bytes and finds nothing - which is what the
// finder did after `dropmult`, before #69.
static unsigned char gameImage[0x50000];
static unsigned char pluginImage[0x50000];
static HMODULE const kGameModule = reinterpret_cast<HMODULE>(0x140000000ull);
static HMODULE GetModuleHandleA(const char*) { return kGameModule; }
static bool AddrIsExecutableInModule(HMODULE mod, const void* addr)
{
    const auto* p = static_cast<const unsigned char*>(addr);
    return mod == kGameModule && p >= gameImage && p < gameImage + sizeof(gameImage);
}

struct FakeFunctions { void* m_ScriptFunction; };
struct CScript { FakeFunctions* m_Functions; };
static FakeFunctions dropFunctions, chanceFunctions;
static CScript dropScript{ &dropFunctions }, chanceScript{ &chanceFunctions };
struct Runner {
    AurieStatus GetNamedRoutinePointer(const char* name, PVOID* out)
    {
        const std::string n(name);
        *out = n == "gml_Script_DropItem" ? static_cast<PVOID>(&dropScript)
             : n == "gml_Script_DropItemAngelicChance" ? static_cast<PVOID>(&chanceScript)
             : nullptr;
        return *out ? 0 : 1;
    }
} runner;
static Runner* g_Yytk = &runner;

static std::vector<std::string> logs;
static void Out(const std::string& line) { logs.push_back(line); }

static unsigned char* g_AngelicGate = nullptr;
static unsigned char* g_DropItemCode = nullptr;
static unsigned char* g_AngelicChanceCode = nullptr;

// PRODUCTION_GATE

// DropItem at +0x1000, its call to DropItemAngelicChance 0x20000 bytes in, and
// the `test al,al; je rel32` that skips that call 0x80 bytes before it,
// landing 0x10 bytes after it - the shape FindAngelicGate looks for.
static unsigned char* const kDropItem = gameImage + 0x1000;
static unsigned char* const kChance = gameImage + 0x40000;
static unsigned char* const kCall = kDropItem + 0x20000;
static unsigned char* const kGateJe = kCall - 0x80 + 2;             // the 0F 84 of that je
static unsigned char* const kHookDropItem = pluginImage + 0x100;   // DropManager's Hook_DropItem
static unsigned char* const kHookChance = pluginImage + 0x200;     // HookAngelicChance

static void WriteRel32(unsigned char* at, const unsigned char* from, const unsigned char* to)
{
    const int rel = static_cast<int>(to - from);
    std::memcpy(at, &rel, sizeof(rel));
}

static void Reset(int gates = 1)
{
    std::memset(gameImage, 0xCC, sizeof(gameImage));
    std::memset(pluginImage, 0xCC, sizeof(pluginImage));
    kCall[0] = 0xE8;
    WriteRel32(kCall + 1, kCall + 5, kChance);
    for (int g = 0; g < gates; ++g) {
        unsigned char* test = kCall - 0x80 - g * 0x20;   // 84 C0 0F 84 rel32
        test[0] = 0x84; test[1] = 0xC0; test[2] = 0x0F; test[3] = 0x84;
        WriteRel32(test + 4, test + 8, kCall + 0x10);
    }
    dropFunctions.m_ScriptFunction = kDropItem;        // the game's own entries
    chanceFunctions.m_ScriptFunction = kChance;
    g_AngelicGate = g_DropItemCode = g_AngelicChanceCode = nullptr;
    logs.clear();
}

static bool Logged(const char* text)
{
    for (const auto& line : logs)
        if (line.find(text) != std::string::npos) return true;
    return false;
}

int main()
{
    int failures = 0;
    auto check = [&](bool ok, const char* name) {
        std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
        failures += !ok;
    };

    Reset(); CaptureAngelicScriptCode();
    check(FindAngelicGate() == kGateJe && Logged("gate found at DropItem+0x"),
          "startup/the gate is found while the table still holds the game's code");

    // #69: the first `dropmult` installs DropManager, whose HookOneScript
    // swaps DropItem's table entry for ForgePact's Hook_DropItem.
    Reset(); CaptureAngelicScriptCode();
    dropFunctions.m_ScriptFunction = kHookDropItem;
    check(FindAngelicGate() == kGateJe && !Logged("call site not found"),
          "after dropmult/the gate is still found in the game's own DropItem");

    // `angelicwatch` hooks DropItemAngelicChance the same way; DropItem's call
    // still targets the game's function, not the hook.
    Reset(); CaptureAngelicScriptCode();
    chanceFunctions.m_ScriptFunction = kHookChance;
    check(FindAngelicGate() == kGateJe,
          "after angelicwatch/a hooked DropItemAngelicChance still matches the call");

    Reset(); CaptureAngelicScriptCode();
    dropFunctions.m_ScriptFunction = kHookDropItem;
    chanceFunctions.m_ScriptFunction = kHookChance;
    check(FindAngelicGate() == kGateJe, "after both hooks/the gate is still found");

    // Nothing recorded yet (the finder runs first): it records now, from a
    // table that is still the game's own.
    Reset();
    check(FindAngelicGate() == kGateJe, "no startup record/a pristine table is read on first use");

    // Some other module swapped an entry before ForgePact started: that entry
    // is not game code, so it is refused and never scanned or patched.
    Reset(); dropFunctions.m_ScriptFunction = kHookDropItem; CaptureAngelicScriptCode();
    check(FindAngelicGate() == nullptr && !Logged("call site not found")
              && Logged("the game's own code") && g_AngelicGate == nullptr,
          "hooked before startup/a non-game DropItem is refused, never scanned");

    Reset(); chanceFunctions.m_ScriptFunction = kHookChance; CaptureAngelicScriptCode();
    check(FindAngelicGate() == nullptr && !Logged("call site not found") && Logged("the game's own code"),
          "hooked before startup/a non-game DropItemAngelicChance is refused");

    // A refusal is not recorded: once the entry reads as game code again, the
    // next call finds the gate.
    Reset(); dropFunctions.m_ScriptFunction = kHookDropItem;
    const bool refused = FindAngelicGate() == nullptr;
    dropFunctions.m_ScriptFunction = kDropItem;
    check(refused && FindAngelicGate() == kGateJe, "refusal/is not recorded");

    Reset(2); CaptureAngelicScriptCode();
    check(FindAngelicGate() == nullptr && Logged("not uniquely identified"),
          "ambiguity/two candidate gates patch nothing");

    Reset(); CaptureAngelicScriptCode();
    unsigned char* first = FindAngelicGate();
    dropFunctions.m_ScriptFunction = kHookDropItem;
    const size_t before = logs.size();
    check(first == kGateJe && FindAngelicGate() == first && logs.size() == before,
          "cache/a found gate is returned again without rescanning");

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << '\n';
    return failures ? 1 : 0;
}
