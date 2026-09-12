// BloodPact live diagnostic + injection plugin for Hero Siege (YYTK / Aurie)
// Command-file IPC so ALL experimentation happens in ONE game session.
//
// Protocol:
//   We write a command into  bp_ipc\cmd.txt  (whole-file overwrite).
//   Every few frames the plugin reads it, deletes it, runs it, and
//   APPENDS the result to  bp_ipc\out.txt .
//
// Supported commands (one per line):
//   ping
//   script <Name>            -> look up gml_Script_<Name> and <Name>, report index
//   exists <globalName>      -> variable_global_exists
//   get <globalName>         -> variable_global_get, describe value
//   dump <substr>            -> enumerate global vars whose name contains <substr> (case-insensitive)
//   setn <globalName> <num>  -> variable_global_set(name, real)
//   sets <globalName> <str>  -> variable_global_set(name, string)
//   call <ScriptName>            -> CallGameScript gml_Script_<ScriptName> with no args
//   call <ScriptName> <strarg>   -> ... with one string arg (rest of line)
//   callfile <ScriptName> <path> -> ... with one string arg = file contents

#include <winsock2.h>
#include <ws2tcpip.h>
#include <YYToolkit/YYTK_Shared.hpp>
#include <hs_game_sdk/hs_game_sdk.hpp>
#include <windows.h>
#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <thread>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <cstring>
#pragma comment(lib, "ws2_32.lib")
#include <sstream>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <array>
#include <map>
#include <random>
#include <chrono>
#include <vector>
#include <cctype>
#include <unordered_map>
#include <map>
#include <set>
#include <deque>
#include <cmath>
#include <intrin.h>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_Yytk = nullptr;

// Player builds keep only counters that affect behaviour.  Telemetry counters
// are useful while reverse-engineering, but their atomic increments sit on hot
// combat/drop/stat paths and must be free in the shipped build.
#ifdef FORGEPACT_RELEASE
#define BP_DIAG_INCREMENT(counter) ((void)0)
#else
#define BP_DIAG_INCREMENT(counter) InterlockedIncrement(&(counter))
#endif

// LogDrop's full body stays where it was (a general research/logging utility,
// not drop-multiplier-specific); only forward-declared here, alongside its
// macro, so ForgePact::DropManager's hook bodies can use BP_LOGDROP despite
// being included well before LogDrop's definition. Preprocessor macros have
// no forward-declaration equivalent - unlike a function, the #define itself
// must move, or anything textually before it treats BP_LOGDROP as unknown.
static void LogDrop(const char* fn, RValue& res, int argc, RValue** A);
#ifdef FORGEPACT_RELEASE
  #define BP_LOGDROP(a,b,c,d) ((void)0)
#else
  #define BP_LOGDROP(a,b,c,d) LogDrop(a,b,c,d)
#endif

// ===== Hook state =====
static std::unordered_map<std::string, double> g_Config;   // modifier key -> value
static PFUNC_YYGMLScript g_OrigGetInfo = nullptr;           // trampoline to original GetBloodPactInfo
static PFUNC_YYGMLScript g_OrigGetSlot = nullptr;          // trampoline to original GetSlotBloodPact
static bool g_HookInstalled = false;
static bool g_Setup = false;
static volatile long g_HookCalls = 0;
static volatile long g_HookOverrides = 0;
static volatile long g_SlotCalls = 0;
static double g_ForceSlot = NAN;       // if set (not NaN), GetSlotBloodPact returns this
static std::string g_LastKeys;         // distinct GetBloodPactInfo keys seen
static std::string g_SlotLog;          // distinct GetSlotBloodPact (arg->orig) seen
static std::string g_CallerLog;        // distinct caller RVAs of GetBloodPactInfo
static uintptr_t g_Base = 0;           // game module base
static bool g_ProbeStruct = true;      // numeric-arg probe returns full modifier struct

// ===== Necromancer balance N1 ==============================================
// Keep these values named and centralized: the panel/tests consume this block
// as the runtime contract.  N1 is OFF by default and is applied only after the
// live talent structs have passed an exact vanilla/N1 semantic preflight.
static constexpr double kN1WarriorValue1Vanilla = 8.0;
static constexpr double kN1WarriorValue1Balanced = 9.40;
static constexpr double kN1MageValue1Vanilla = 7.25;
static constexpr double kN1MageValue1Balanced = 8.51;
static constexpr double kN1MageLifeValue2Vanilla = 55.0;
static constexpr double kN1MageLifeValue2Balanced = 68.0;
static constexpr double kN1AmplifyDurationVanilla = 5.0;
static constexpr double kN1AmplifyDurationBalanced = 10.0;
static constexpr double kN1FrenzyDurationVanilla = 25.0;
static constexpr double kN1FrenzyDurationBalanced = 35.0;
static constexpr double kN1FrenzyCooldownVanilla = 70.0;
static constexpr double kN1FrenzyCooldownBalanced = 30.0;
static constexpr double kN1FrenzyStartingValue1Vanilla = 8.0;
static constexpr double kN1FrenzyStartingValue1Balanced = 8.0;
static constexpr double kN1FrenzyValue1Vanilla = 2.0;
static constexpr double kN1FrenzyValue1Balanced = 1.0;
static constexpr double kN1FrenzyStartingValue2Vanilla = 4.0;
static constexpr double kN1FrenzyStartingValue2Balanced = 4.0;
static constexpr double kN1FrenzyValue2Vanilla = 1.0;
static constexpr double kN1FrenzyValue2Balanced = 1.0;
static constexpr double kN1MageMaxSummonsVanilla = 0.0;
static constexpr double kN1MageMaxSummonsBalanced = 2.0;
static constexpr double kN1SpiritMaxSummonsVanilla = 2.0;
static constexpr double kN1SpiritMaxSummonsBalanced = 1.0;
static constexpr double kN1WarriorPlayerRangeVanilla = 48.0;
static constexpr double kN1WarriorPlayerRangeBalanced = 64.0;

static std::atomic<bool> g_NecroBalanceEnabled{ false };
static std::atomic<bool> g_NecroBalanceOwned{ false };
static std::atomic<bool> g_NecroRestorePending{ false };
static std::atomic<bool> g_NecroRangeIntegrity{ true };
static std::atomic<bool> g_NecroPostCreateHooksInstalled{ false };
static bool g_NecroBalanceHookInstalled = false;
static int g_NecroWarriorObjectIndex = -1;
static PFUNC_YYGMLScript g_OrigPopulateTalentStructMapNecromancer = nullptr;
static PFUNC_YYGMLScript g_OrigLoadSummonStatsN1 = nullptr;
static volatile long g_NecroPopulateCalls = 0;
static volatile long g_NecroApplyOk = 0;
static volatile long g_NecroApplyRejected = 0;
static volatile long g_NecroLoadStatsCalls = 0;
static volatile long g_NecroLoadStatsDeferred = 0;
static volatile long g_NecroRangeWrites = 0;
static volatile long g_NecroRangeRejected = 0;
static std::string g_NecroLastStatus = "not installed";

static void NecroBalancePostCreatedInstance(int objectIndex, RValue& instanceId);
static void InstallNecroBalanceHooks();
static void SetNecroBalance(bool enabled);
static void NecroBalanceStatus();

// Derive the IPC dir from the game exe location so EACH game copy (main + backup)
// uses its OWN bp_ipc channel — required for the 2-instance co-op test + real 2-PC deploy.
static std::string ComputeIpcDir()
{
    char buf[MAX_PATH] = { 0 };
    DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0) return "bp_ipc";
    std::string p(buf);
    size_t s = p.find_last_of("\\/");
    std::string dir = (s == std::string::npos) ? std::string(".") : p.substr(0, s);
    return dir + "\\bp_ipc";
}
static const std::string IPC_DIR = ComputeIpcDir();
static std::string CmdPath() { return IPC_DIR + "\\cmd.txt"; }
static std::string OutPath() { return IPC_DIR + "\\out.txt"; }

static void Out(const std::string& s)
{
    std::ofstream f(OutPath(), std::ios::app);
    f << s << "\n";
    if (g_Yytk) g_Yytk->PrintInfo("[BP] %s", s.c_str());
}

// Crash-pinpoint trace: flushes a marker to bp_ipc\loadtrace.txt at each load step,
// so if the game crashes during init we can see the LAST step reached.
static void Trace(const char* phase)
{
#ifdef FORGEPACT_RELEASE
    (void)phase; return;   // yayin: teshis izi yok
#else
    std::ofstream f(IPC_DIR + "\\loadtrace.txt", std::ios::app);
    f << phase << "\n";
    f.flush();
#endif
}

static std::string Lower(std::string s)
{
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// A game-memory-read RValue (e.g. an item's droprate.base) can come back as an
// inf/NaN/huge double when an index/offset is stale after a game update, and
// an IPC-supplied value (e.g. "droprate set 2 1e300") can be huge without
// being inf/NaN at all. %f/%.0f on any such value expands to hundreds of
// characters and overruns a fixed sprintf_s buffer, which makes the CRT
// abort the whole process (0xC0000409 / STATUS_STACK_BUFFER_OVERRUN). Route
// every such double through this before handing it to a %f-style sprintf_s
// so a bad read degrades the printed number instead of crashing the game.
// Clamped, not just filtered: rejecting only inf/NaN still let a finite value
// like 1e300 through, and %.0f of that alone is ~300 characters - already
// wider than every fixed buffer this is used with. 1e15 is a small fraction
// of any of those buffers (16 characters under %.0f) while comfortably
// covering every real in-game rate/multiplier this file prints.
static inline double SafeF(double v) {
    if (!std::isfinite(v)) return 0.0;
    constexpr double kMaxSafeF = 1e15;
    if (v > kMaxSafeF) return kMaxSafeF;
    if (v < -kMaxSafeF) return -kMaxSafeF;
    return v;
}

static std::string Describe(const RValue& v)
{
    try {
        switch (v.m_Kind) {
        case VALUE_REAL:      return "real:" + std::to_string(v.ToDouble());
        case VALUE_INT32:     return "int32:" + std::to_string(v.ToInt32());
        case VALUE_INT64:     return "int64:" + std::to_string(v.ToInt64());
        case VALUE_BOOL:      return std::string("bool:") + (v.ToBoolean() ? "true" : "false");
        case VALUE_STRING:    return "string:\"" + v.ToString() + "\"";
        case VALUE_OBJECT:    return "object/struct";
        case VALUE_ARRAY:     return "array";
        case VALUE_PTR:       return "ptr";
        case VALUE_UNDEFINED: return "undefined";
        case VALUE_NULL:      return "null";
        default:
            return "kind=" + std::to_string((int)v.m_Kind) + " str=" + v.ToString();
        }
    } catch (...) { return "<describe-failed>"; }
}

// split first token off, return token, set rest to remainder (trimmed)
static std::string FirstToken(const std::string& line, std::string& rest)
{
    size_t i = 0;
    while (i < line.size() && std::isspace((unsigned char)line[i])) i++;
    size_t j = i;
    while (j < line.size() && !std::isspace((unsigned char)line[j])) j++;
    std::string tok = line.substr(i, j - i);
    size_t k = j;
    while (k < line.size() && std::isspace((unsigned char)line[k])) k++;
    rest = line.substr(k);
    // strip trailing CR/space
    while (!rest.empty() && (rest.back() == '\r' || rest.back() == '\n')) rest.pop_back();
    return tok;
}

// ===== ForgePact:: module includes ==========================================
// Anchor point for the incremental class-based split of this file (2026-09).
//
// This has to sit here, this early, because DensityManager's own usage sites
// (DensityWindowActive and friends, a few dozen lines below) are themselves
// earlier in the file than several other modules' prerequisites - there is no
// single later point that works for every module without forward-declaring
// what is used before it is defined. The alternative (moving the anchor later
// each time a module needs something not yet declared) ran out of room here;
// forward-declaring is the sustainable fix for the remaining modules too.
//
// Only Out/Lower/FirstToken (just defined above) are needed unqualified by
// the class bodies; everything else a class calls into ModuleMain for is
// forward-declared right here, specifically so this include block can move
// only forward from now on, never back:
static bool HookOneScript(const char* shortName, const char* id, PVOID dest, PFUNC_YYGMLScript* origOut);
static bool AddrIsExecutableInModule(HMODULE mod, const void* addr);   // defined with the pet-quest collect call
static bool HhResolveLocalPlayer(RValue& out, std::string* how = nullptr);
static void InstallCreateHooks();
static void InstallDensityLifecycleHooks();
static void OpenDensityWindow();
static void RunCommand(const std::string& line);

#include <ForgePact/Common.hpp>
#include <ForgePact/MapRevealManager.hpp>
#include <ForgePact/StatsManager.hpp>
#include <ForgePact/RelicFilterMod.hpp>
#include <ForgePact/PetQuestCollectorMod.hpp>
#include <ForgePact/DensityManager.hpp>
#include <ForgePact/DropManager.hpp>
#include <ForgePact/IpcServer.hpp>
#include <ForgePact/ModManager.hpp>

static void DoScriptLookup(const std::string& name)
{
    int idx = -1;
    std::string full = "gml_Script_" + name;
    AurieStatus s1 = g_Yytk->GetNamedRoutineIndex(full.c_str(), &idx);
    Out("script '" + full + "' -> status=" + std::to_string((int)s1) + " index=" + std::to_string(idx));
    int idx2 = -1;
    AurieStatus s2 = g_Yytk->GetNamedRoutineIndex(name.c_str(), &idx2);
    Out("script '" + name + "' -> status=" + std::to_string((int)s2) + " index=" + std::to_string(idx2));
}

static void DoExists(const std::string& name)
{
    RValue r = g_Yytk->CallBuiltin("variable_global_exists", { RValue(name) });
    Out("exists '" + name + "' -> " + Describe(r));
}


// Global bir dizinin tek elemanini okur/yazar.
// PSet ile ayni yol: variable_global_get -> tur kontrolu -> array_set.
// Tur kontrolu SART; dizi olmayan bir degere array_set tanimsiz davranistir.
static RValue GlobalArray(const std::string& var, int& len)
{
    len = -1;
    RValue ex = g_Yytk->CallBuiltin("variable_global_exists", { RValue(var) });
    if (!ex.ToBoolean()) return RValue();
    RValue arr = g_Yytk->CallBuiltin("variable_global_get", { RValue(var) });
    if (arr.m_Kind != VALUE_ARRAY) return RValue();
    RValue n = g_Yytk->CallBuiltin("array_length", { arr });
    len = (int)n.ToDouble();
    return arr;
}

static void GlobalArrayGet(const std::string& var, int idx)
{
    try {
        int len = -1; RValue arr = GlobalArray(var, len);
        if (len < 0) { Out("gaget: global '" + var + "' yok ya da dizi degil"); return; }
        if (idx < 0 || idx >= len) { Out("gaget: indeks disarida (len=" + std::to_string(len) + ")"); return; }
        RValue e = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)idx) });
        Out("gaget " + var + "[" + std::to_string(idx) + "] -> " + Describe(e));
    } catch (...) { Out("gaget EXCEPTION"); }
}

static void GlobalArraySet(const std::string& var, int idx, double value)
{
    try {
        int len = -1; RValue arr = GlobalArray(var, len);
        if (len < 0) { Out("gaset: global '" + var + "' yok ya da dizi degil"); return; }
        if (idx < 0 || idx >= len) { Out("gaset: indeks disarida (len=" + std::to_string(len) + ")"); return; }
        g_Yytk->CallBuiltin("array_set", { arr, RValue((double)idx), RValue(value) });
        RValue e = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)idx) });
        Out("gaset " + var + "[" + std::to_string(idx) + "] = " + std::to_string(value)
            + " -> simdi " + Describe(e));
    } catch (...) { Out("gaset EXCEPTION"); }
}


// --- eSt zorlama -----------------------------------------------------------
// Oyun her oda baslangicinda eSt'i yeniden dolduruyor; kapiyi acik tutmak
// icin degeri her karede geri yaziyoruz.  Yalnizca gercekten farkliysa
// yaziyoruz, boylece bosuna array_set cagrilmiyor.
static std::map<int, double> g_EstForce;
static uint64_t g_EstForceWrites = 0;

static void EstForceApply()
{
    if (g_EstForce.empty() || !g_Yytk) return;
    try {
        int len = -1;
        RValue arr = GlobalArray("eSt", len);
        if (len < 0) return;
        for (auto& kv : g_EstForce) {
            if (kv.first < 0 || kv.first >= len) continue;
            RValue cur = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)kv.first) });
            if (cur.ToDouble() == kv.second) continue;
            g_Yytk->CallBuiltin("array_set", { arr, RValue((double)kv.first), RValue(kv.second) });
            g_EstForceWrites++;
        }
    } catch (...) {}
}

static void EstStat()
{
    int len = -1;
    RValue arr = GlobalArray("eSt", len);
    std::string s = "eststat: len=" + std::to_string(len)
                  + " yazma=" + std::to_string(g_EstForceWrites) + " | zorlanan:";
    if (g_EstForce.empty()) s += " (yok)";
    for (auto& kv : g_EstForce)
        s += " [" + std::to_string(kv.first) + "]=" + std::to_string((int)kv.second);
    Out(s);
    if (len > 0) {
        std::string v = "  guncel eSt:";
        for (int i = 0; i < len; i++) {
            try {
                RValue e = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)i) });
                v += " " + std::to_string((int)e.ToDouble());
            } catch (...) { v += " ?"; }
        }
        Out(v);
    }
}


// --- Oyunun kendi gunlugu --------------------------------------------------
// DebugLogAddExt'i kancalayip argumanlari diske yaziyoruz.  Shadow Realm /
// Abyss / Traveling Merchant mekanikleri bunu cagiriyor; neden vazgectiklerini
// oyunun kendi agzindan ogrenmek icin.
static PFUNC_YYGMLScript g_Orig_DebugLogAddExt = nullptr;
static bool g_GameLogOn = false;
static uint64_t g_GameLogLines = 0;

static void GameLogWrite(const std::string& s)
{
    std::ofstream f(IPC_DIR + "\\gamelog.txt", std::ios::app);
    f << s << "\n";
    f.flush();
    g_GameLogLines++;
}

static RValue& Hook_DebugLogAddExt(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_GameLogOn) {
        try {
            std::string line;
            for (int i = 0; i < argc && i < 8; i++) {
                if (!A[i]) continue;
                if (!line.empty()) line += " | ";
                line += Describe(*A[i]);
            }
            if (!line.empty()) GameLogWrite(line);
        } catch (...) {}
    }
    return g_Orig_DebugLogAddExt ? g_Orig_DebugLogAddExt(S, O, R, argc, A) : R;
}


// --- Abyss izleme ----------------------------------------------------------
static PFUNC_YYGMLScript g_Orig_AbyssMech = nullptr;
static PFUNC_YYGMLScript g_Orig_GPV_Trace = nullptr;
static bool g_AbyssTraceOn = false;
static bool g_InAbyss = false;
static uint64_t g_AbyssRuns = 0;

static void AbyssWrite(const std::string& s)
{
    std::ofstream f(IPC_DIR + "\\abyss.txt", std::ios::app);
    f << s << "\n";
    f.flush();
}

// GPV yalnizca Abyss mekanigi calisirken kaydedilir.
static RValue& Hook_GPV_Trace(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    const bool inside = g_AbyssTraceOn && g_InAbyss;
    std::string args;
    if (inside) {
        try {
            for (int i = 0; i < argc && i < 6; i++) {
                if (!A[i]) continue;
                if (!args.empty()) args += ", ";
                args += Describe(*A[i]);
            }
        } catch (...) {}
    }
    RValue& r = g_Orig_GPV_Trace ? g_Orig_GPV_Trace(S, O, R, argc, A) : R;
    if (inside) {
        try { AbyssWrite("  GPV(" + args + ") -> " + Describe(r)); } catch (...) {}
    }
    return r;
}

static RValue& Hook_AbyssMech(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    const bool prev = g_InAbyss;
    g_InAbyss = true;
    g_AbyssRuns++;
    if (g_AbyssTraceOn) AbyssWrite("=== Abyss mekanigi calisti #" + std::to_string(g_AbyssRuns));
    RValue& r = g_Orig_AbyssMech ? g_Orig_AbyssMech(S, O, R, argc, A) : R;
    if (g_AbyssTraceOn) AbyssWrite("=== bitti -> " + Describe(r));
    g_InAbyss = prev;
    return r;
}


// --- Abyss konum kapisini zorlama ------------------------------------------
static PFUNC_YYGMLScript g_Orig_Obtain = nullptr;
static bool g_ForceObtain = false;
static uint64_t g_ObtainCalls = 0;    // Abyss icindeyken kac kez soruldu
static uint64_t g_ObtainFalse = 0;    // kacinda oyun "olmaz" dedi
static uint64_t g_ObtainForced = 0;   // kacini biz "olur"a cevirdik

static RValue& Hook_Obtain(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    RValue& r = g_Orig_Obtain ? g_Orig_Obtain(S, O, R, argc, A) : R;
    if (g_InAbyss) {
        g_ObtainCalls++;
        bool ok = true;
        try { ok = r.ToBoolean(); } catch (...) {}
        if (!ok) {
            g_ObtainFalse++;
            if (g_ForceObtain) { r = RValue(1.0); g_ObtainForced++; }
        }
    }
    return r;
}


static void DoGet(const std::string& name)
{
    RValue ex = g_Yytk->CallBuiltin("variable_global_exists", { RValue(name) });
    if (!ex.ToBoolean()) { Out("get '" + name + "' -> (does not exist)"); return; }
    RValue r = g_Yytk->CallBuiltin("variable_global_get", { RValue(name) });
    Out("get '" + name + "' -> " + Describe(r));
}

static void DoDump(const std::string& substr)
{
    CInstance* global = nullptr;
    AurieStatus st = g_Yytk->GetGlobalInstance(&global);
    if (!AurieSuccess(st) || !global) { Out("dump: GetGlobalInstance failed st=" + std::to_string((int)st)); return; }

    std::string needle = Lower(substr);
    RValue globalrv = RValue(global); // object RValue
    int count = 0;
    g_Yytk->EnumInstanceMembers(
        globalrv,
        [&](const char* name, RValue* val) -> bool {
            if (name && Lower(name).find(needle) != std::string::npos) {
                std::string d = val ? Describe(*val) : "<null>";
                Out("  global." + std::string(name) + " = " + d);
                count++;
            }
            return false; // keep enumerating ALL members
        }
    );
    Out("dump '" + substr + "' -> " + std::to_string(count) + " matches");
}

static void DoSetNum(const std::string& name, const std::string& numstr)
{
    double v = 0.0;
    try { v = std::stod(numstr); } catch (...) { Out("setn: bad number '" + numstr + "'"); return; }
    g_Yytk->CallBuiltin("variable_global_set", { RValue(name), RValue(v) });
    RValue r = g_Yytk->CallBuiltin("variable_global_get", { RValue(name) });
    Out("setn '" + name + "' = " + numstr + " -> now " + Describe(r));
}

static void DoSetStr(const std::string& name, const std::string& str)
{
    g_Yytk->CallBuiltin("variable_global_set", { RValue(name), RValue(str) });
    RValue r = g_Yytk->CallBuiltin("variable_global_get", { RValue(name) });
    Out("sets '" + name + "' = \"" + str + "\" -> now " + Describe(r));
}

static void DoCall(const std::string& scriptName, const std::string& strArg, bool hasArg)
{
    std::string full = "gml_Script_" + scriptName;
    std::vector<RValue> args;
    if (hasArg) args.push_back(RValue(strArg));
    try {
        RValue res = g_Yytk->CallGameScript(full, args);
        Out("call '" + full + "'(" + (hasArg ? ("\"" + strArg + "\"") : "") + ") -> " + Describe(res));
    } catch (...) {
        Out("call '" + full + "' -> EXCEPTION");
    }
}

static void DoCallFile(const std::string& scriptName, const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { Out("callfile: cannot open " + path); return; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();
    Out("callfile: read " + std::to_string(content.size()) + " bytes from " + path);
    DoCall(scriptName, content, true);
}

// json_parse the file into a STRUCT, then call gml_Script_<scriptName> with that struct.
static void DoCallJson(const std::string& scriptName, const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { Out("calljson: cannot open " + path); return; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();
    try {
        RValue parsed = g_Yytk->CallBuiltin("json_parse", { RValue(content) });
        Out("calljson: json_parse -> " + Describe(parsed));
        std::string full = "gml_Script_" + scriptName;
        RValue res = g_Yytk->CallGameScript(full, { parsed });
        Out("calljson '" + full + "'(struct) -> " + Describe(res));
    } catch (...) {
        Out("calljson '" + scriptName + "' -> EXCEPTION");
    }
}

static void DoStructDump(const std::string& globalName)
{
    RValue ex = g_Yytk->CallBuiltin("variable_global_exists", { RValue(globalName) });
    if (!ex.ToBoolean()) { Out("structdump '" + globalName + "' -> (does not exist)"); return; }
    RValue obj = g_Yytk->CallBuiltin("variable_global_get", { RValue(globalName) });
    Out("structdump '" + globalName + "' (" + Describe(obj) + "):");
    int count = 0;
    AurieStatus st = g_Yytk->EnumInstanceMembers(
        obj,
        [&](const char* name, RValue* val) -> bool {
            std::string d = val ? Describe(*val) : "<null>";
            Out("    ." + std::string(name ? name : "?") + " = " + d);
            count++;
            return false;
        }
    );
    Out("structdump '" + globalName + "' -> " + std::to_string(count) + " members (st=" + std::to_string((int)st) + ")");
}

static void DoRoutinePtr(const std::string& name)
{
    std::string full = "gml_Script_" + name;
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(full.c_str(), &p);
    char buf[256];
    HMODULE base = GetModuleHandleA(nullptr);
    if (!AurieSuccess(st) || !p) {
        Out("routineptr '" + full + "' -> status=" + std::to_string((int)st));
        return;
    }
    CScript* sc = reinterpret_cast<CScript*>(p);
    void* fn = nullptr;
    try { fn = (void*)sc->m_Functions->m_ScriptFunction; } catch (...) {}
    unsigned long long fnv = (unsigned long long)fn;
    unsigned long long basev = (unsigned long long)base;
    unsigned long long rva = (fn && fnv > basev) ? (fnv - basev) : 0;
    sprintf_s(buf, "routineptr '%s' -> CScript=%p func=%p base=%p rva=0x%llX",
        full.c_str(), p, fn, (void*)base, rva);
    Out(buf);
}

static void DoReadMem(const std::string& hexaddr, const std::string& lenstr)
{
    unsigned long long addr = 0; int len = 0;
    try { addr = std::stoull(hexaddr, nullptr, 16); len = std::stoi(lenstr); } catch (...) { Out("readmem: bad args"); return; }
    if (len <= 0 || len > 8192) { Out("readmem: len out of range"); return; }
    unsigned char* p = (unsigned char*)addr;
    if (IsBadReadPtr(p, len)) { Out("readmem: address not readable"); return; }
    std::string hex;
    char b[4];
    for (int i = 0; i < len; i++) { sprintf_s(b, "%02X", p[i]); hex += b; }
    std::ofstream mf(IPC_DIR + "\\mem.txt", std::ios::trunc);
    mf << hexaddr << " " << len << "\n" << hex << "\n";
    Out("readmem: wrote " + std::to_string(len) + " bytes of " + hexaddr + " to mem.txt");
}

// ===== instance_create_* builtin hooks (find & multiply enemy creation) =====
static TRoutine g_OrigICD = nullptr; // instance_create_depth
static TRoutine g_OrigICL = nullptr; // instance_create_layer
static std::map<int, long> g_CreateCounts;   // object index -> times created
static std::unordered_map<int, int> g_ObjMult; // object index -> spawn multiplier
// The instance-create builtins are also used by every projectile and hit effect.
// Keep the six Special Content lookups as a direct indexed read instead of a
// hash-table lookup on that combat-hot path. Very large/nonstandard asset
// indices still fall back to g_ObjMult.
static std::vector<int> g_ObjMultFast;
static constexpr int kFastObjectIndexLimit = 1 << 20;
static bool g_LogCreates = true;
static int  g_WatchObj = -1;                 // when this object is created, log the caller RVA
static std::string g_WatchCallers;           // distinct caller RVAs of the watched object's creation
static int  g_EnemyParentIdx = -1;           // asset index of Enemy_Parent_obj
static int  g_EnemyMultAll = 1;              // multiplier applied to ALL enemy descendants (direct)
// ---- research profiler ----------------------------------------------------------------
// Time spent inside ForgePact's own hooks versus the game's frame interval, so "does the
// plugin cost frames at density x3" is a measurement, not a guess.  Compiled out of the
// player build; the PERF_SCOPE macro is empty there.
#ifndef FORGEPACT_RELEASE
struct PerfBucket { const char* name; long long ns = 0; long calls = 0; };
static PerfBucket g_PerfICD{ "instance_create hooks" }, g_PerfForge{ "item create post-process" },
                  g_PerfDeltas{ "rune/special snapshots" }, g_PerfRarity{ "EnemyRaritySettings hook" },
                  g_PerfKill{ "kill hook" }, g_PerfHud{ "DrawHudBuffs hook" }, g_PerfPoll{ "PollCommands" },
                  g_PerfFrame{ "FrameCallback body" };
static PerfBucket* const g_PerfAll[] = { &g_PerfICD, &g_PerfForge, &g_PerfDeltas, &g_PerfRarity, &g_PerfKill, &g_PerfHud, &g_PerfPoll, &g_PerfFrame };
static long long PerfFreq() { static long long f = 0; if (!f) { LARGE_INTEGER q; QueryPerformanceFrequency(&q); f = q.QuadPart; } return f; }
struct PerfScope {
    PerfBucket& b; long long t0;
    explicit PerfScope(PerfBucket& bb) : b(bb) { LARGE_INTEGER q; QueryPerformanceCounter(&q); t0 = q.QuadPart; }
    ~PerfScope() { LARGE_INTEGER q; QueryPerformanceCounter(&q); b.ns += (q.QuadPart - t0) * 1000000000LL / PerfFreq(); ++b.calls; }
};
#define PERF_SCOPE(bucket) PerfScope _perfScope_##__LINE__(bucket)
static long long g_PerfFrameLast = 0, g_PerfFrameSumNs = 0, g_PerfFrameMaxNs = 0; static long g_PerfFrames = 0;
static void PerfFrameTick()
{
    LARGE_INTEGER q; QueryPerformanceCounter(&q);
    if (g_PerfFrameLast) { long long d = (q.QuadPart - g_PerfFrameLast) * 1000000000LL / PerfFreq(); g_PerfFrameSumNs += d; if (d > g_PerfFrameMaxNs) g_PerfFrameMaxNs = d; ++g_PerfFrames; }
    g_PerfFrameLast = q.QuadPart;
}
static void PerfReset() { for (PerfBucket* b : g_PerfAll) { b->ns = 0; b->calls = 0; } g_PerfFrameSumNs = 0; g_PerfFrameMaxNs = 0; g_PerfFrames = 0; g_PerfFrameLast = 0; }
static void PerfReport()
{
    const double frameMs = g_PerfFrames ? g_PerfFrameSumNs / 1e6 : 0.0;
    char b[200];
    sprintf_s(b, "perf: %ld frames, avg %.2f ms, max %.1f ms (game frame interval)", g_PerfFrames, g_PerfFrames ? frameMs / g_PerfFrames : 0.0, g_PerfFrameMaxNs / 1e6);
    Out(b);
    long long total = 0;
    for (PerfBucket* pb : g_PerfAll) {
        total += pb->ns;
        sprintf_s(b, "  %-26s calls=%-7ld total=%8.2f ms  avg=%7.1f us  share of frames=%.3f%%", pb->name, pb->calls, pb->ns / 1e6, pb->calls ? pb->ns / 1e3 / pb->calls : 0.0, frameMs > 0 ? pb->ns / 1e6 / frameMs * 100.0 : 0.0);
        Out(b);
    }
    sprintf_s(b, "  plugin total %.2f ms of %.2f ms = %.3f%% of the game's frame time", total / 1e6, frameMs, frameMs > 0 ? total / 1e6 / frameMs * 100.0 : 0.0);
    Out(b);
}
#else
#define PERF_SCOPE(bucket)
#endif

// Density multiplier + fractional carry moved to ForgePact::DensityManager
// (Instance().Mult / .Frac) - module includes anchor above, after FirstToken.
static bool g_CallerIsEnemy = false;                 // set while a monster's own instance_create runs (see EnemyBornScope)
static volatile LONG g_DensitySkippedEnemyBorn = 0;
static std::unordered_map<int, bool> g_IsEnemyCache;   // object index -> is enemy descendant
static std::unordered_map<int, bool> g_IsCreatorCache; // object index -> name starts with "Enemy_Creator"
static std::vector<int8_t> g_IsCreatorFast;
static bool g_KnownCreatorObjectsResolved = false;
static volatile long g_ExtraCreators = 0;    // extra spawner instances the multiplier created
static volatile long g_ExtraEnemies = 0;     // extra enemy instances the multiplier created
static volatile long g_DensityRevisitSkips = 0;

// Density is a map-generation feature, not a combat-event feature. The global
// create builtins remain hooked after installation, so without a generation
// window every projectile/effect created during combat was classified as a
// possible Enemy_Creator. Open the window on a zone transition and close it
// shortly after the last normal creator was seen.
static uint64_t g_RuntimeFrame = 0;
static uint64_t g_DensityWindowHardEnd = 0;
static uint64_t g_DensityWindowLastCreator = 0;
static bool g_DensityWindowSawCreator = false;
static constexpr uint64_t kDensityWindowHardFrames = 900;
static constexpr uint64_t kDensityWindowIdleFrames = 60;

static void OpenDensityWindow()
{
    g_DensityWindowHardEnd = g_RuntimeFrame + kDensityWindowHardFrames;
    g_DensityWindowLastCreator = 0;
    g_DensityWindowSawCreator = false;
}

static bool DensityWindowActive()
{
    if (ForgePact::DensityManager::Instance().Mult <= 1.0 || g_RuntimeFrame > g_DensityWindowHardEnd)
        return false;
    return !g_DensityWindowSawCreator
        || g_RuntimeFrame <= g_DensityWindowLastCreator + kDensityWindowIdleFrames;
}

static void NoteDensityCreator()
{
    g_DensityWindowSawCreator = true;
    g_DensityWindowLastCreator = g_RuntimeFrame;
}

static int ObjectMultiplier(int objIdx)
{
    if (objIdx >= 0 && objIdx < static_cast<int>(g_ObjMultFast.size()))
        return g_ObjMultFast[static_cast<size_t>(objIdx)];
    auto it = g_ObjMult.find(objIdx);
    return it == g_ObjMult.end() ? 1 : it->second;
}

static void SetObjectMultiplier(int objIdx, int multiplier)
{
    if (objIdx < 0) return;
    if (multiplier < 1) multiplier = 1;
    if (objIdx <= kFastObjectIndexLimit) {
        const size_t wanted = static_cast<size_t>(objIdx) + 1;
        if (g_ObjMultFast.size() < wanted) g_ObjMultFast.resize(wanted, 1);
        g_ObjMultFast[static_cast<size_t>(objIdx)] = multiplier;
    }
    if (multiplier > 1) g_ObjMult[objIdx] = multiplier;
    else g_ObjMult.erase(objIdx);
}

// GameMaker runs an instance's Create event before instance_create_* returns.
// This scope identifies Enemy_Creator objects produced by Special Content.
// They belong to that content multiplier and must not be multiplied a second
// time by Monster Density.
static thread_local uint32_t g_SpecialCreateDepth = 0;

struct SpecialCreateScope
{
    bool active;
    explicit SpecialCreateScope(bool enabled) : active(enabled)
    {
        if (active) ++g_SpecialCreateDepth;
    }
    ~SpecialCreateScope()
    {
        if (active) --g_SpecialCreateDepth;
    }
};

// Hero Siege S10 saves every creator placement in ZoneState.  Cloning a creator
// therefore works on the first visit, but the saved original + ForgePact copies
// all pass through instance_create_* again on a revisit.  Multiplying every one
// of those entries turns 4x into 16x, then 64x.  Remember the exact placements
// which have already received density, including the copies we create.
//
// Do NOT include global.room, the runner room, depth/layer or another transient
// load value in this key.  Those values can differ while the same ZoneState is
// being restored, which made a saved placement look new and expanded it again.
// Object + rounded position is deliberately conservative: a coincident creator
// in another zone may be left at vanilla density, but density can never grow on
// every revisit.  Preventing cumulative growth is the stronger invariant.
struct DensityPlacementKey
{
    int32_t objectIndex;
    int64_t x;
    int64_t y;

    bool operator==(const DensityPlacementKey& other) const
    {
        return objectIndex == other.objectIndex && x == other.x && y == other.y;
    }
};

static uint64_t DensityHashMix(uint64_t seed, uint64_t value)
{
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBull;
    value ^= value >> 31;
    return seed ^ (value + (seed << 6) + (seed >> 2));
}

struct DensityPlacementHash
{
    size_t operator()(const DensityPlacementKey& key) const
    {
        uint64_t h = DensityHashMix(0x46504744454E5349ull,
                                    static_cast<uint32_t>(key.objectIndex));
        h = DensityHashMix(h, static_cast<uint64_t>(key.x));
        h = DensityHashMix(h, static_cast<uint64_t>(key.y));
        return static_cast<size_t>(h);
    }
};

static std::unordered_set<DensityPlacementKey, DensityPlacementHash> g_DensityKnownPlacements;
static std::mutex g_DensityPlacementMutex;

static DensityPlacementKey MakeDensityPlacementKey(
    int objIdx, RValue* args, int argc)
{
    DensityPlacementKey key{};
    key.objectIndex = objIdx;
    if (args && argc >= 2) {
        try { key.x = static_cast<int64_t>(std::llround(args[0].ToDouble())); } catch (...) {}
        try { key.y = static_cast<int64_t>(std::llround(args[1].ToDouble())); } catch (...) {}
    }
    return key;
}

static bool RememberDensityPlacement(const DensityPlacementKey& key)
{
    std::lock_guard<std::mutex> lock(g_DensityPlacementMutex);
    return g_DensityKnownPlacements.insert(key).second;
}

static void ForgetDensityPlacements()
{
    std::lock_guard<std::mutex> lock(g_DensityPlacementMutex);
    g_DensityKnownPlacements.clear();
    ForgePact::DensityManager::Instance().Frac = 0.0;
}

static size_t DensityPlacementCount()
{
    std::lock_guard<std::mutex> lock(g_DensityPlacementMutex);
    return g_DensityKnownPlacements.size();
}

static bool IsCreatorObject(int objIdx)
{
    if (objIdx < 0) return false;
    if (objIdx < static_cast<int>(g_IsCreatorFast.size())) {
        const int8_t cached = g_IsCreatorFast[static_cast<size_t>(objIdx)];
        if (cached >= 0) return cached != 0;
    }
    auto it = g_IsCreatorCache.find(objIdx);
    if (it != g_IsCreatorCache.end()) return it->second;
    bool res = false;
    try {
        RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)objIdx) });
        std::string name = n.ToString();
        res = (name.rfind("Enemy_Creator", 0) == 0);
    } catch (...) { res = false; }
    g_IsCreatorCache[objIdx] = res;
    if (objIdx <= kFastObjectIndexLimit) {
        const size_t wanted = static_cast<size_t>(objIdx) + 1;
        if (g_IsCreatorFast.size() < wanted) g_IsCreatorFast.resize(wanted, -1);
        g_IsCreatorFast[static_cast<size_t>(objIdx)] = res ? 1 : 0;
    }
    return res;
}

// These are the complete Season 10 map-density creator family. Resolve them by
// name once, rather than relying on build-specific numeric object indices. The
// create hooks can then recognize creators in every zone with one indexed read,
// even when a zone-transition script did not open the discovery window.
static constexpr const char* kKnownDensityCreatorObjects[] = {
    "Enemy_Creator_obj",
    "Enemy_Creator_Ambush_obj",
    "Enemy_Creator_Ancient_obj",
    "Enemy_Creator_Champion_obj",
    "Enemy_Creator_Colossal_Chest_obj",
    "Enemy_Creator_Legion_obj",
    "Enemy_Creator_Miniboss_obj",
};

static bool IsCachedCreatorObject(int objIdx)
{
    if (objIdx < 0) return false;
    if (objIdx < static_cast<int>(g_IsCreatorFast.size()))
        return g_IsCreatorFast[static_cast<size_t>(objIdx)] == 1;
    auto it = g_IsCreatorCache.find(objIdx);
    return it != g_IsCreatorCache.end() && it->second;
}

static void CacheCreatorObject(int objIdx)
{
    if (objIdx < 0) return;
    g_IsCreatorCache[objIdx] = true;
    if (objIdx <= kFastObjectIndexLimit) {
        const size_t wanted = static_cast<size_t>(objIdx) + 1;
        if (g_IsCreatorFast.size() < wanted) g_IsCreatorFast.resize(wanted, -1);
        g_IsCreatorFast[static_cast<size_t>(objIdx)] = 1;
    }
}

static void ResolveKnownCreatorObjects()
{
    if (g_KnownCreatorObjectsResolved || !g_Yytk) return;
    int resolved = 0;
    for (const char* name : kKnownDensityCreatorObjects) {
        try {
            RValue r = g_Yytk->CallBuiltin("asset_get_index", { RValue(name) });
            const int objIdx = static_cast<int>(r.ToDouble());
            if (objIdx >= 0) {
                CacheCreatorObject(objIdx);
                ++resolved;
            }
        } catch (...) {}
    }
    g_KnownCreatorObjectsResolved = true;
    Out("density creators cached = " + std::to_string(resolved));
}

static bool IsEnemyObject(int objIdx)
{
    if (objIdx < 0) return false;
    auto it = g_IsEnemyCache.find(objIdx);
    if (it != g_IsEnemyCache.end()) return it->second;
    bool res = false;
    try {
        if (g_EnemyParentIdx >= 0) {
            if (objIdx == g_EnemyParentIdx) res = true;
            else {
                RValue r = g_Yytk->CallBuiltin("object_is_ancestor",
                    { RValue((double)objIdx), RValue((double)g_EnemyParentIdx) });
                res = r.ToBoolean();
            }
        }
    } catch (...) { res = false; }
    g_IsEnemyCache[objIdx] = res;
    return res;
}

static void LogCreatePos(int objIdx, RValue* Args, int argc);
static void PullNearApply(int objIdx, RValue* Args, int argc);
static void PostCreateCheck(int objIdx, RValue& Result, RValue* Args, int argc);


// --- Ozel icerik yaratimini zamana yayma -----------------------------------
// Room Start'ta butun marker kopyalari ayni karede yaratilinca oyun cokuyordu.
// Kopyalar kuyruga alinip her karede birkac tanesi yaratilir; sonuc ayni,
// tepe yuk bolunur.  Density yolu bundan etkilenmez.
struct GecikmeliYaratim { bool katman; double x, y; RValue yuva; RValue nesne; unsigned long long dogum; };
static std::deque<GecikmeliYaratim> g_Kuyruk;
static int  g_KareBasina = 3;          // 0 = kapali (aninda yarat)
static bool g_KuyruktanYaratim = false; // yeniden girisi engeller
static uint64_t g_KuyrukToplam = 0;

static int g_OrnekButce = 14000;  // 0 = sinirsiz
static uint64_t g_ButceIptal = 0;  // butce/yas yuzunden atilan yaratim sayisi
static unsigned long long g_KuyrukKare = 0;
static int g_SonOrnekSayisi = -1;

static void KuyruktanNesneyiSil(int objIdx)
{
    g_Kuyruk.erase(
        std::remove_if(g_Kuyruk.begin(), g_Kuyruk.end(), [objIdx](const GecikmeliYaratim& g) {
            try { return static_cast<int>(g.nesne.ToDouble()) == objIdx; }
            catch (...) { return false; }
        }),
        g_Kuyruk.end());
}

// Oyundaki toplam etkin ornek sayisi (GML'de `all` = -3).  Hata olursa -1.
static int ToplamOrnek()
{
    try {
        RValue r = g_Yytk->CallBuiltin("instance_number", { RValue(-3.0) });
        return (int)r.ToDouble();
    } catch (...) { return -1; }
}


#ifdef FORGEPACT_RELEASE
// YYToolkit'in "YYToolkit Log" konsolu oyuncuya gorunmesin.
// (sinif ConsoleWindowClass, oyunun kendi surecinde AllocConsole ile aciliyor)
static void KonsoluGizle()
{
    HWND h = GetConsoleWindow();
    if (h && IsWindowVisible(h)) ShowWindow(h, SW_HIDE);
}
#endif

static void KuyrukIsle()
{
    g_KuyrukKare++;
    if (g_Kuyruk.empty() || !g_Yytk || g_KareBasina <= 0) return;

    // Butce asildiysa kalan kuyrugu birak.  Olculdu: ~13.600'de oyun yasiyor,
    // 22.575'te oluyor.  Mekanikler yaratimdan SONRA da dusman dogurdugu icin
    // butce bilerek bu araligin altinda.
    // Butce asilmissa BU KAREYI ATLA - kuyruk korunur, yer acilinca devam eder.
    // (Once temizliyordum: harita yuklenirken olusan anlik tepe tum icerigi
    //  kalicii olarak iptal ediyordu.)
    if (g_OrnekButce > 0) {
        // instance_number(all) traverses the live instance table.  Calling it
        // every frame while a large special-content queue drains caused
        // avoidable spikes in heavy maps such as Eternal Battlefield.
        if (g_SonOrnekSayisi < 0 || (g_KuyrukKare % 10) == 0)
            g_SonOrnekSayisi = ToplamOrnek();
        if (g_SonOrnekSayisi >= 0 && g_SonOrnekSayisi >= g_OrnekButce) return;
    }

    g_KuyruktanYaratim = true;
    for (int i = 0; i < g_KareBasina && !g_Kuyruk.empty(); i++) {
        GecikmeliYaratim g = g_Kuyruk.front();
        g_Kuyruk.pop_front();
        // Cok bekleyen oge dusurulur, yoksa bir sonraki haritada dogar.
        if (g_KuyrukKare > g.dogum + 900) { g_ButceIptal++; continue; }
        try {
            g_Yytk->CallBuiltin(g.katman ? "instance_create_layer" : "instance_create_depth",
                                { RValue(g.x), RValue(g.y), g.yuva, g.nesne });
            if (g_SonOrnekSayisi >= 0) g_SonOrnekSayisi++;
        } catch (...) {}
    }
    g_KuyruktanYaratim = false;
}

static int CallerObjectIndex(CInstance* S);   // defined with the enemy-born guard below
static void DoMultiCreate(TRoutine orig, RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args, int objIdx, bool katman)
{
#ifndef FORGEPACT_RELEASE
    if (objIdx >= 0 && g_LogCreates) g_CreateCounts[objIdx]++;
    PullNearApply(objIdx, Args, argc);
    LogCreatePos(objIdx, Args, argc);
#endif
    int mult = ObjectMultiplier(objIdx);
    const bool ozelIcerik = mult > 1;
    const bool specialChild = g_SpecialCreateDepth > 0;
    const bool knownCreator = IsCachedCreatorObject(objIdx);
    const bool inspectCreator = ForgePact::DensityManager::Instance().Mult > 1.0
        && (knownCreator || DensityWindowActive() || specialChild);
    const bool isCreator = inspectCreator
        && (knownCreator || IsCreatorObject(objIdx));

    bool densityAlreadyApplied = false;
    if (isCreator && Args && argc >= 4) {
        const DensityPlacementKey key = MakeDensityPlacementKey(objIdx, Args, argc);
        if (specialChild) {
            // Preserve this exemption across a ZoneState revisit as well.
            RememberDensityPlacement(key);
            densityAlreadyApplied = true;
        } else if (!g_KuyruktanYaratim && !RememberDensityPlacement(key)) {
            densityAlreadyApplied = true;
            BP_DIAG_INCREMENT(g_DensityRevisitSkips);
        }
    }
    if (isCreator && !specialChild) NoteDensityCreator();
    // density: multiply all Enemy_Creator* spawners (produces fully-configured enemies)
    if (g_CallerIsEnemy && isCreator) InterlockedIncrement(&g_DensitySkippedEnemyBorn);
#ifndef FORGEPACT_RELEASE
    if (ForgePact::DensityManager::Instance().Mult > 1.0 && isCreator && !specialChild && !g_CallerIsEnemy && !densityAlreadyApplied && knownCreator && !DensityWindowActive()) {
        static long s_LateLogs = 0;
        if (++s_LateLogs <= 60) {
            std::string who = "?"; try { RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)CallerObjectIndex(S)) }); who = n.ToString(); } catch (...) {}
            std::string what = "?"; try { RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)objIdx) }); what = n.ToString(); } catch (...) {}
            Out("density: late spawner multiplied: " + what + " created by " + who + " at frame " + std::to_string((long long)g_RuntimeFrame));
        }
    }
#endif
    if (ForgePact::DensityManager::Instance().Mult > 1.0 && isCreator && !specialChild && !g_CallerIsEnemy
        && !densityAlreadyApplied
        && (knownCreator || DensityWindowActive())) {
        // Tam kisim herkese; kesir kismi birikime yayilir ve 1'e ulasinca
        // O ureticiye bir fazla kopya dusar.  Rastgelelik yok, deterministik.
        int tam = (int)ForgePact::DensityManager::Instance().Mult;
        double kesir = ForgePact::DensityManager::Instance().Mult - (double)tam;
        int m = tam;
        if (kesir > 0.0) {
            ForgePact::DensityManager::Instance().Frac += kesir;
            if (ForgePact::DensityManager::Instance().Frac >= 1.0) { ForgePact::DensityManager::Instance().Frac -= 1.0; m += 1; }
        }
        if (m > mult) mult = m;
    }
    // (optional) direct enemy-descendant multiplier — off by default, creators are the right layer
    else if (g_EnemyMultAll > 1 && IsEnemyObject(objIdx) && g_EnemyMultAll > mult)
        mult = g_EnemyMultAll;
    if (mult > 1 && argc >= 4 && orig && !g_KuyruktanYaratim) {
        for (int i = 1; i < mult; i++) {
            try {
                // Ozel icerik marker'i: kuyruga al, karelere yay.
                if (ozelIcerik && g_KareBasina > 0) {
                    GecikmeliYaratim g;
                    g.katman = katman;
                    g.x = Args[0].ToDouble() + (double)(((i % 5) - 2) * 28);
                    g.y = Args[1].ToDouble() + (double)(((i / 5) - 2) * 28);
                    g.yuva = Args[2];
                    g.nesne = Args[3];
                    g.dogum = g_KuyrukKare;
                    g_Kuyruk.push_back(g);
                    g_KuyrukToplam++;
                    continue;
                }
                std::vector<RValue> a(Args, Args + argc);
                a[0] = RValue(Args[0].ToDouble() + (double)(((i % 5) - 2) * 28));
                a[1] = RValue(Args[1].ToDouble() + (double)(((i / 5) - 2) * 28));
                // Register the generated placement before GameMaker sees it.
                // ZoneState may serialize it immediately during the original
                // call; on reload it must be recognized as an existing copy.
                if (isCreator)
                    RememberDensityPlacement(MakeDensityPlacementKey(objIdx, a.data(), argc));
                RValue tmp;
                orig(tmp, S, O, argc, a.data());
                if (isCreator) BP_DIAG_INCREMENT(g_ExtraCreators);
                else BP_DIAG_INCREMENT(g_ExtraEnemies);
            } catch (...) {}
        }
    }
    {
        SpecialCreateScope specialScope(ozelIcerik);
        orig(Result, S, O, argc, Args);
    }
#ifndef FORGEPACT_RELEASE
    PostCreateCheck(objIdx, Result, Args, argc);
#endif
}


// --- Yaratim konumu kaydi --------------------------------------------------
static std::set<int> g_LogCreatePos;
static uint64_t g_LogCreatePosLines = 0;

static void LogCreatePos(int objIdx, RValue* Args, int argc)
{
    if (g_LogCreatePos.empty() || objIdx < 0) return;
    if (!g_LogCreatePos.count(objIdx)) return;
    if (argc < 2 || !Args) return;
    try {
        double x = Args[0].ToDouble();
        double y = Args[1].ToDouble();
        std::string nm;
        try {
            RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)objIdx) });
            nm = n.ToString();
        } catch (...) { nm = std::to_string(objIdx); }
        std::ofstream f(IPC_DIR + "\\createpos.txt", std::ios::app);
        f << nm << "  x=" << (int)x << "  y=" << (int)y << "\n";
        f.flush();
        g_LogCreatePosLines++;
    } catch (...) {}
}


// --- Yaratimi oyuncunun yanina cekme ---------------------------------------
static std::set<int> g_PullNear;
static double g_PullRadius = 700.0;
static uint64_t g_PullCount = 0;

// Ust uste binmesinler diye halka seklinde dagitilir.
static void PullNearApply(int objIdx, RValue* Args, int argc)
{
    if (g_PullNear.empty() || objIdx < 0 || argc < 2 || !Args) return;
    if (!g_PullNear.count(objIdx)) return;
    try {
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue pid = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        if (pid.ToDouble() < 0) return;
        RValue px = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("x") });
        RValue py = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("y") });

        const double step = 0.7;                 // halka acisi adimi (radyan)
        const double a = g_PullCount * step;
        const double r = g_PullRadius * (0.45 + 0.55 * ((g_PullCount % 3) / 2.0));
        Args[0] = RValue(px.ToDouble() + cos(a) * r);
        Args[1] = RValue(py.ToDouble() + sin(a) * r);
        g_PullCount++;
    } catch (...) {}
}


// --- Yaratim sonrasi dogrulama ---------------------------------------------
static void PostCreateCheck(int objIdx, RValue& Result, RValue* Args, int argc)
{
    // N1 uses the final instance returned by GameMaker's own create builtin.
    // This is later than LoadSummonStats/the child's Create assignments, so the
    // Warrior's vanilla playerRange=48 can no longer overwrite our value.
    NecroBalancePostCreatedInstance(objIdx, Result);

    if (g_LogCreatePos.empty() || objIdx < 0) return;
    if (!g_LogCreatePos.count(objIdx)) return;
    try {
        std::string nm;
        try {
            RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)objIdx) });
            nm = n.ToString();
        } catch (...) { nm = std::to_string(objIdx); }

        double id = -1.0;
        try { id = Result.ToDouble(); } catch (...) {}

        int ex = -1;
        try {
            RValue e = g_Yytk->CallBuiltin("instance_exists", { Result });
            ex = e.ToBoolean() ? 1 : 0;
        } catch (...) {}

        double x = (argc >= 2 && Args) ? Args[0].ToDouble() : 0.0;
        double y = (argc >= 2 && Args) ? Args[1].ToDouble() : 0.0;

        std::ofstream f(IPC_DIR + "\\postcheck.txt", std::ios::app);
        f << nm << "  id=" << (long long)id << "  exists=" << ex
          << "  x=" << (int)x << " y=" << (int)y << "\n";
        f.flush();
    } catch (...) {}
}


// --- Silme izleme ----------------------------------------------------------
static TRoutine g_OrigDestroy = nullptr;
static std::set<int> g_DestroyWatch;
static uint64_t g_DestroyHits = 0;

static void HookDestroy(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    void* ret = _ReturnAddress();
    if (!g_DestroyWatch.empty()) {
        try {
            // Argumansiz cagri: kendini yok ediyor -> S'nin nesne indeksi
            int idx = -1;
            if (argc >= 1 && Args) {
                RValue oi = g_Yytk->CallBuiltin("instance_exists", { Args[0] });
                if (oi.ToBoolean()) {
                    RValue r = g_Yytk->CallBuiltin("variable_instance_get",
                                                   { Args[0], RValue("object_index") });
                    idx = (int)r.ToDouble();
                }
            } else if (S) {
                RValue r = g_Yytk->CallBuiltin("variable_instance_get",
                                               { RValue(S), RValue("object_index") });
                idx = (int)r.ToDouble();
            }
            if (idx >= 0 && g_DestroyWatch.count(idx)) {
                std::string nm;
                try {
                    RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)idx) });
                    nm = n.ToString();
                } catch (...) { nm = std::to_string(idx); }
                char rb[32];
                sprintf_s(rb, "0x%llX", (unsigned long long)((uintptr_t)ret - g_Base));
                std::ofstream f(IPC_DIR + "\\destroy.txt", std::ios::app);
                f << nm << "  silen_rva=" << rb << "\n";
                f.flush();
                g_DestroyHits++;
            }
        } catch (...) {}
    }
    if (g_OrigDestroy) g_OrigDestroy(Result, S, O, argc, Args);
}

static void WatchLog(void* ret, int objIdx)
{
    if (objIdx == g_WatchObj && g_Base && objIdx >= 0) {
        char rb[24]; sprintf_s(rb, "<0x%llX>", (unsigned long long)((uintptr_t)ret - g_Base));
        if (g_WatchCallers.size() < 800 && g_WatchCallers.find(rb) == std::string::npos)
            g_WatchCallers += rb;
    }
}
// ---- enemy-born creations ----------------------------------------------------------------
// Worms, spiders and similar split into new monsters when they die, and a dying monster
// can also spawn a legion creator.  Anything a monster itself creates is remembered here:
// the rarity raise and Tyrant's Crown leave those alone (a re-raised split child splits
// again - "the pack grows forever", tester report 2026-09-07) and Monster Density does
// not multiply a creator a monster spawned.  Cheap: the caller is only inspected when a
// raise or density is active and the created object is a monster or a known creator.
static bool RarityFloorActive();
static bool TyrantActive();
static bool g_CreatingFromEnemy = false;             // true while a monster runs instance_create
static std::unordered_set<int> g_EnemyBornIds;       // monsters created by a monster, by instance id
static volatile LONG g_EnemyBornSeen = 0, g_RarSkippedEnemyBorn = 0;
static int CallerObjectIndex(CInstance* S)
{
    if (!S) return -1;
    try { RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { RValue(S), RValue("object_index") }); return (int)oi.ToDouble(); } catch (...) { return -1; }
}
static bool CallerIsEnemyInstance(CInstance* S) { return IsEnemyObject(CallerObjectIndex(S)); }
// The built-in `id` is not a struct member: variable_struct_get gives undefined for it (the
// enemy-born match was silently dead, 2026-09-07: 1261 births recorded, 0 matched).
static double InstanceIdOf(const RValue& inst)
{
    try { RValue v = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("id") }); return v.ToDouble(); } catch (...) { return -1.0; }
}
// A spawner created by a monster OR by another spawner is a runtime chain link, not part of
// the zone's layout: density leaves it alone, otherwise every generation multiplies again.
static bool CallerIsEnemyOrCreator(CInstance* S)
{
    const int idx = CallerObjectIndex(S);
    return idx >= 0 && (IsEnemyObject(idx) || IsCachedCreatorObject(idx) || IsCreatorObject(idx));
}
// Scoped guard placed first in HookICD / HookICL: when a MONSTER instance itself creates a
// monster or a spawner, the flags are raised for the duration of the original call and the
// new monster's id is remembered afterwards, so the rarity raise leaves split children alone.
// enemyCaller only: a spawner (Enemy_Creator*) creating a monster is the game's ordinary
// zone population - counting that as "monster-born" silenced Tyrant's Crown and Monster
// Rarity completely (3370 monsters seen, 0 raised - user log 2026-09-08).
struct EnemyBornScope
{
    RValue& result; int objIdx = -1; bool active = false; bool prevCreating = false, prevCaller = false;
    EnemyBornScope(RValue& r, CInstance* S, int argc, RValue* Args) : result(r)
    {
        try { if (argc >= 4) objIdx = (int)Args[3].ToDouble(); } catch (...) {}
        if (!(RarityFloorActive() || TyrantActive() || ForgePact::DensityManager::Instance().Mult > 1.0)) return;
        if (!(IsEnemyObject(objIdx) || IsCachedCreatorObject(objIdx))) return;
        if (!CallerIsEnemyInstance(S)) return;   // only a real monster starts a chain
        active = true; prevCreating = g_CreatingFromEnemy; prevCaller = g_CallerIsEnemy;
        g_CreatingFromEnemy = g_CreatingFromEnemy || IsEnemyObject(objIdx);
        g_CallerIsEnemy = true;
    }
    ~EnemyBornScope()
    {
        if (!active) return;
        g_CreatingFromEnemy = prevCreating; g_CallerIsEnemy = prevCaller;
        try {
            if (IsEnemyObject(objIdx) && (result.m_Kind == VALUE_REAL || result.m_Kind == VALUE_INT32 || result.m_Kind == VALUE_INT64 || result.m_Kind == VALUE_REF)) {
                if (g_EnemyBornIds.size() > 5000) g_EnemyBornIds.clear();
                g_EnemyBornIds.insert((int)result.ToDouble());
                InterlockedIncrement(&g_EnemyBornSeen);
            }
        } catch (...) {}
    }
};
static void HhDeathEffectTrigger(CInstance* S, int objIdx);   // defined with the Headhunter module below
static bool HeadhunterRunning();                              // same
static void HookICD(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    PERF_SCOPE(g_PerfICD);
    EnemyBornScope _born(Result, S, argc, Args);
    if (argc >= 4 && HeadhunterRunning()) { try { HhDeathEffectTrigger(S, (int)Args[3].ToDouble()); } catch (...) {} }
#ifdef FORGEPACT_RELEASE
    // A hook installed earlier in this process cannot be removed safely while
    // the game is running.  With every related feature Off, take a true native
    // pass-through path: no object lookup, cache access or post-create work.
    if (ForgePact::DensityManager::Instance().Mult <= 1.0 && g_EnemyMultAll <= 1 && g_ObjMult.empty()) {
        if (g_OrigICD) g_OrigICD(Result, S, O, argc, Args);
        return;
    }
#endif
#ifndef FORGEPACT_RELEASE
    void* ret = _ReturnAddress();
#endif
    int objIdx = -1;
    try { if (argc >= 4) objIdx = (int)Args[3].ToDouble(); } catch (...) {}
#ifdef FORGEPACT_RELEASE
    // Special Content keeps the create hook installed, but ordinary combat
    // objects are not special markers. One indexed read is enough to return to
    // the original builtin without entering the multiplier machinery.
    const bool densityCreate = ForgePact::DensityManager::Instance().Mult > 1.0
        && (DensityWindowActive() || IsCachedCreatorObject(objIdx));
    if (!densityCreate && g_EnemyMultAll <= 1 && g_SpecialCreateDepth == 0
        && ObjectMultiplier(objIdx) <= 1) {
        if (g_OrigICD) g_OrigICD(Result, S, O, argc, Args);
        return;
    }
#endif
#ifndef FORGEPACT_RELEASE
    WatchLog(ret, objIdx);
#endif
    if (g_OrigICD) DoMultiCreate(g_OrigICD, Result, S, O, argc, Args, objIdx, false);
}
static void HookICL(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    PERF_SCOPE(g_PerfICD);
    EnemyBornScope _born(Result, S, argc, Args);
    if (argc >= 4 && HeadhunterRunning()) { try { HhDeathEffectTrigger(S, (int)Args[3].ToDouble()); } catch (...) {} }
#ifdef FORGEPACT_RELEASE
    if (ForgePact::DensityManager::Instance().Mult <= 1.0 && g_EnemyMultAll <= 1 && g_ObjMult.empty()) {
        if (g_OrigICL) g_OrigICL(Result, S, O, argc, Args);
        return;
    }
#endif
#ifndef FORGEPACT_RELEASE
    void* ret = _ReturnAddress();
#endif
    int objIdx = -1;
    try { if (argc >= 4) objIdx = (int)Args[3].ToDouble(); } catch (...) {}
#ifdef FORGEPACT_RELEASE
    const bool densityCreate = ForgePact::DensityManager::Instance().Mult > 1.0
        && (DensityWindowActive() || IsCachedCreatorObject(objIdx));
    if (!densityCreate && g_EnemyMultAll <= 1 && g_SpecialCreateDepth == 0
        && ObjectMultiplier(objIdx) <= 1) {
        if (g_OrigICL) g_OrigICL(Result, S, O, argc, Args);
        return;
    }
#endif
#ifndef FORGEPACT_RELEASE
    WatchLog(ret, objIdx);
#endif
    if (g_OrigICL) DoMultiCreate(g_OrigICL, Result, S, O, argc, Args, objIdx, true);
}

static bool HookBuiltin(const char* name, const char* id, PVOID dest, TRoutine* origOut)
{
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(name, &p);
    if (!AurieSuccess(st) || !p) { Out(std::string("hookbuiltin ") + name + ": not found st=" + std::to_string((int)st)); return false; }
    PVOID tramp = nullptr;
    AurieStatus hs = MmCreateHook(g_ArSelfModule, id, p, dest, &tramp);
    if (!AurieSuccess(hs)) { Out(std::string("hookbuiltin ") + name + ": failed st=" + std::to_string((int)hs)); return false; }
    *origOut = reinterpret_cast<TRoutine>(tramp);
    Out(std::string("HOOK INSTALLED on builtin ") + name);
    return true;
}

static void InstallCreateHooks()
{
    // This is idempotent and must run even if Special Content installed the
    // create hooks before Monster Density was enabled.
    ResolveKnownCreatorObjects();
    if (g_OrigICD && g_OrigICL) {
        g_NecroPostCreateHooksInstalled.store(true);
        return;
    }
    bool depthReady = g_OrigICD != nullptr;
    bool layerReady = g_OrigICL != nullptr;
    if (!depthReady)
        depthReady = HookBuiltin("instance_create_depth", "bp_icd", (PVOID)HookICD, &g_OrigICD);
    if (!layerReady)
        layerReady = HookBuiltin("instance_create_layer", "bp_icl", (PVOID)HookICL, &g_OrigICL);
    // N1's authoritative final-range patch needs both creation paths.  Existing
    // trampolines count as ready, making repeated InstallHook calls idempotent.
    g_NecroPostCreateHooksInstalled.store(depthReady && layerReady);
    try {
        RValue r = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
        g_EnemyParentIdx = (int)r.ToDouble();
        Out("Enemy_Parent_obj index = " + std::to_string(g_EnemyParentIdx));
    } catch (...) { Out("could not resolve Enemy_Parent_obj"); }
}

// Dumps ALL created objects to bp_ipc/createlog.txt, marking enemies.
static void CreateLog(bool enemiesOnly)
{
    std::ofstream lf(IPC_DIR + "\\createlog.txt", std::ios::trunc);
    int enemyCount = 0;
    for (auto& kv : g_CreateCounts) {
        std::string name = "?";
        try {
            RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)kv.first) });
            name = n.ToString();
        } catch (...) {}
        bool isEnemy = IsEnemyObject(kv.first);
        if (isEnemy) enemyCount++;
        if (enemiesOnly && !isEnemy) continue;
        std::string line = std::to_string(kv.first) + " : " + name + " : count=" + std::to_string(kv.second);
        if (isEnemy) line += "  [ENEMY]";
        lf << line << "\n";
    }
    Out("createlog written to bp_ipc/createlog.txt | distinct=" + std::to_string(g_CreateCounts.size())
        + " enemies=" + std::to_string(enemyCount) + (enemiesOnly ? " (enemies only)" : ""));
}

// ===== Enemy-spawn hooks (density test) =====
static PFUNC_YYGMLScript g_OrigFreePos = nullptr; static volatile long g_cntFreePos = 0; static int g_MultFreePos = 1;
static PFUNC_YYGMLScript g_OrigCreate  = nullptr; static volatile long g_cntCreate  = 0; static int g_MultCreate  = 1;
static PFUNC_YYGMLScript g_OrigElite   = nullptr; static volatile long g_cntElite   = 0; static int g_MultElite   = 1;

static RValue& HookFreePos(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    InterlockedIncrement(&g_cntFreePos);
    for (int i = 1; i < g_MultFreePos; i++) { RValue t; if (g_OrigFreePos) g_OrigFreePos(S, O, t, argc, A); }
    return g_OrigFreePos ? g_OrigFreePos(S, O, R, argc, A) : R;
}
static RValue& HookCreate(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    InterlockedIncrement(&g_cntCreate);
    for (int i = 1; i < g_MultCreate; i++) { RValue t; if (g_OrigCreate) g_OrigCreate(S, O, t, argc, A); }
    return g_OrigCreate ? g_OrigCreate(S, O, R, argc, A) : R;
}
static RValue& HookElite(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    InterlockedIncrement(&g_cntElite);
    for (int i = 1; i < g_MultElite; i++) { RValue t; if (g_OrigElite) g_OrigElite(S, O, t, argc, A); }
    return g_OrigElite ? g_OrigElite(S, O, R, argc, A) : R;
}

// Table-only install: swaps the pointer inside the script-table entry.
//
// This is BLIND to compiled GML's direct calls. This build's YYC code calls
// another script with a `call rel32` bound at compile time, which never reads
// the script table - the finding that invalidated 34 hooked call sites'
// worth of "0 calls" results (see agents.md, "Prove the Instrument Before
// Trusting a Negative Result").
//
// So this is NOT the installer gameplay hooks should use; HookOneScript below
// is. It stays because the `citrace` research commands need a deliberately
// table-only hook: `citrace nativetrace`'s whole purpose is to run a native
// detour alongside a table hook on the same target and print both counters,
// and that comparison only means anything while one of them really is
// table-only.
static bool HookOneScriptTable(const char* shortName, const char* id, PVOID dest, PFUNC_YYGMLScript* origOut)
{
    UNREFERENCED_PARAMETER(id);
    std::string full = std::string("gml_Script_") + shortName;
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(full.c_str(), &p);
    if (!AurieSuccess(st) || !p) { Out(std::string("hook ") + shortName + ": not found st=" + std::to_string((int)st)); return false; }
    CScript* sc = reinterpret_cast<CScript*>(p);
    if (!sc || !sc->m_Functions) { Out(std::string("hook ") + shortName + ": null functions"); return false; }
    if (origOut && !*origOut) {
        *origOut = sc->m_Functions->m_ScriptFunction;
    }
    sc->m_Functions->m_ScriptFunction = reinterpret_cast<PFUNC_YYGMLScript>(dest);
    Out(std::string("HOOK INSTALLED on ") + shortName);
    return true;
}

// The installer every shipped gameplay hook uses. Installs BOTH:
//
//   - the script-table swap, which catches calls routed through the table;
//   - an inline detour at the function's own address (MmCreateHook), which
//     catches compiled GML's direct `call rel32` - the ones the table swap
//     cannot see.
//
// REPORTED 2026-09-12 by origin's review of PR #2, and it is the same defect
// this branch had already found and fixed for exactly one hook: a table-only
// install reports "HOOK INSTALLED" and then silently does nothing on the
// paths the game actually uses. Read-only inspection of the shipped exe found
// direct native callers for StatMovementSpeed, StatAttackSpeed, DropRelic,
// DropMonsterGold and DropGold among others - so stat scaling, drop
// multipliers and the max-level relic filter could all report installed while
// the game ran straight past them. Rather than layer a second detour onto the
// five names that happened to get verified, the interception belongs in the
// installer, where every present and future gameplay hook gets it.
//
// `*origOut` becomes the TRAMPOLINE, so a hook body that calls through it
// reaches the real original from either route and never re-enters itself.
//
// Three things make repeat installation safe - it matters, because shared
// chokepoints like DropRelic are installed from more than one call site
// (RelicFilterMod's max-relic exclusion and the panel's dropmult both want
// it), and re-install is how this file has always made that idempotent:
//
//   1. The detour is attempted only on the FIRST install (`!*origOut`), the
//      one moment the table is guaranteed to still hold the game's own
//      function. A later install would read our own detour out of the table
//      and patch that, which is an infinite loop.
//   2. The target must lie inside Hero_Siege.exe. If some other hook already
//      swapped this entry, the table holds a pointer into THIS module, and
//      patching it would detour our own code instead of the game's.
//   3. A failed detour is not fatal: `*origOut` falls back to the table entry
//      and the hook stays table-only, exactly as it behaved before. It says
//      so in the log rather than pretending.
static bool HookOneScript(const char* shortName, const char* id, PVOID dest, PFUNC_YYGMLScript* origOut)
{
    std::string full = std::string("gml_Script_") + shortName;
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(full.c_str(), &p);
    if (!AurieSuccess(st) || !p) { Out(std::string("hook ") + shortName + ": not found st=" + std::to_string((int)st)); return false; }
    CScript* sc = reinterpret_cast<CScript*>(p);
    if (!sc || !sc->m_Functions) { Out(std::string("hook ") + shortName + ": null functions"); return false; }

    PFUNC_YYGMLScript tableEntry = sc->m_Functions->m_ScriptFunction;
    const bool firstInstall = (origOut && !*origOut);
    if (firstInstall) {
        bool native = false;
        std::string why;
        if (!id || !*id) {
            why = "no hook id";
        } else if (!AddrIsExecutableInModule(GetModuleHandleA(nullptr), (const void*)tableEntry)) {
            // Already detoured by something in this module, or not code at
            // all. Either way the address is not the game's to patch.
            why = "table entry is not code inside Hero_Siege.exe";
        } else {
            PVOID tramp = nullptr;
            AurieStatus ns = MmCreateHook(g_ArSelfModule, id, (PVOID)tableEntry, dest, &tramp);
            if (AurieSuccess(ns) && tramp) {
                *origOut = reinterpret_cast<PFUNC_YYGMLScript>(tramp);
                native = true;
            } else {
                why = "MmCreateHook st=" + std::to_string((int)ns);
            }
        }
        if (!native) {
            *origOut = tableEntry;
            Out(std::string("hook ") + shortName + ": TABLE-ONLY (" + why
                + ") - direct compiled-GML calls will bypass this hook");
        }
    }
    sc->m_Functions->m_ScriptFunction = reinterpret_cast<PFUNC_YYGMLScript>(dest);
    Out(std::string("HOOK INSTALLED on ") + shortName);
    return true;
}

// MEASURED 2026-09-10, session 7: every hook this file installs goes through
// HookOneScript above, which always prepends "gml_Script_" - correct for
// script assets and the anonymous closures GameMaker nests inside an
// object's Create event (both genuinely carry that prefix), but an object's
// own built-in event code (Step, Destroy, Mouse, Alarm, ...) is named
// "gml_Object_<ObjName>_<Event>_<N>" with NO "gml_Script_" prefix at all.
// That naming was structurally unreachable through HookOneScript no matter
// what name was tried - this is the raw form, taking the exact routine name
// with no prefix added, so those object-event entry points can finally be
// tried directly.
static bool HookRawNamedRoutine(const char* fullName, const char* id, PVOID dest, PFUNC_YYGMLScript* origOut)
{
    UNREFERENCED_PARAMETER(id);
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(fullName, &p);
    if (!AurieSuccess(st) || !p) { Out(std::string("hookraw ") + fullName + ": not found st=" + std::to_string((int)st)); return false; }
    CScript* sc = reinterpret_cast<CScript*>(p);
    if (!sc || !sc->m_Functions) { Out(std::string("hookraw ") + fullName + ": null functions"); return false; }
    if (origOut && !*origOut) {
        *origOut = sc->m_Functions->m_ScriptFunction;
    }
    sc->m_Functions->m_ScriptFunction = reinterpret_cast<PFUNC_YYGMLScript>(dest);
    Out(std::string("HOOK INSTALLED on raw ") + fullName);
    return true;
}

// Only a full ZoneState reset is a safe boundary for forgetting placements.
// ZoneStateResetSingle is also used during ordinary zone transitions, so hooking
// it caused the guard to forget a map immediately before a revisit and recreated
// the original cumulative-density bug.
static PFUNC_YYGMLScript g_OrigZoneStateResetAllDensity = nullptr;
static PFUNC_YYGMLScript g_OrigZoneStateResetSingleDensityWindow = nullptr;
static bool g_DensityLifecycleHooksInstalled = false;

static RValue& HookZoneStateResetAllDensity(
    CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    ForgetDensityPlacements();
    OpenDensityWindow();
    return g_OrigZoneStateResetAllDensity
        ? g_OrigZoneStateResetAllDensity(S, O, R, argc, A) : R;
}

static RValue& HookZoneStateResetSingleDensityWindow(
    CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    // A single-zone reset marks a transition. It must never clear the stable
    // placement set: doing that is what caused density to multiply on revisit.
    OpenDensityWindow();
    return g_OrigZoneStateResetSingleDensityWindow
        ? g_OrigZoneStateResetSingleDensityWindow(S, O, R, argc, A) : R;
}

static void InstallDensityLifecycleHooks()
{
    if (g_DensityLifecycleHooksInstalled) return;
    g_DensityLifecycleHooksInstalled = true;
    HookOneScript("ZoneStateResetAll", "fp_density_reset_all",
                  (PVOID)HookZoneStateResetAllDensity, &g_OrigZoneStateResetAllDensity);
    HookOneScript("ZoneStateResetSingle", "fp_density_window_single",
                  (PVOID)HookZoneStateResetSingleDensityWindow,
                  &g_OrigZoneStateResetSingleDensityWindow);
}

struct N1TalentField
{
    int talentId;
    const char* field;
    double vanilla;
    double balanced;
};

// Talent IDs are deliberately explicit.  No class-wide/global scalar is ever
// changed, so shared constants (notably Mage/Corpse Explosion's old 7.25) stay
// untouched.
static constexpr N1TalentField kN1TalentFields[] = {
    { 120, "abilityValue1",       kN1WarriorValue1Vanilla,          kN1WarriorValue1Balanced },
    { 122, "abilityValue1",       kN1MageValue1Vanilla,             kN1MageValue1Balanced },
    { 122, "abilityValue2",       kN1MageLifeValue2Vanilla,         kN1MageLifeValue2Balanced },
    { 119, "abilityDuration",     kN1AmplifyDurationVanilla,        kN1AmplifyDurationBalanced },
    { 124, "abilityDuration",     kN1FrenzyDurationVanilla,         kN1FrenzyDurationBalanced },
    { 124, "abilityCooldown",     kN1FrenzyCooldownVanilla,         kN1FrenzyCooldownBalanced },
    { 124, "abilityStartingValue1", kN1FrenzyStartingValue1Vanilla, kN1FrenzyStartingValue1Balanced },
    { 124, "abilityValue1",       kN1FrenzyValue1Vanilla,           kN1FrenzyValue1Balanced },
    { 124, "abilityStartingValue2", kN1FrenzyStartingValue2Vanilla, kN1FrenzyStartingValue2Balanced },
    { 124, "abilityValue2",       kN1FrenzyValue2Vanilla,           kN1FrenzyValue2Balanced },
    { 122, "abilityMaxSummons",   kN1MageMaxSummonsVanilla,         kN1MageMaxSummonsBalanced },
    { 127, "abilityMaxSummons",   kN1SpiritMaxSummonsVanilla,       kN1SpiritMaxSummonsBalanced },
};
static constexpr int kN1TalentFieldCount =
    (int)(sizeof(kN1TalentFields) / sizeof(kN1TalentFields[0]));

struct N1ObservedField
{
    const N1TalentField* spec;
    RValue talent;
    double before;
};

static bool N1NearlyEqual(double a, double b)
{
    return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) <= 0.000001;
}

static bool N1Numeric(const RValue& value)
{
    return value.m_Kind == VALUE_REAL || value.m_Kind == VALUE_INT32 ||
           value.m_Kind == VALUE_INT64;
}

// GameMaker 2024+ can expose asset/object indices as VALUE_REF even though its
// REAL_RValue converter still yields the integral index.  Keep this separate
// from N1Numeric: talent scalars remain strict and booleans remain rejected.
static bool N1ObjectIndex(const RValue& value, int& index)
{
    const auto kind = static_cast<uint32_t>(value.m_Kind) & 0x0FFFFFFFU;
    if (kind != VALUE_REAL && kind != VALUE_INT32 && kind != VALUE_INT64 &&
        kind != VALUE_REF)
        return false;
    try {
        const double converted = value.ToDouble();
        if (!std::isfinite(converted) || converted < 0.0 ||
            converted > 2147483647.0 ||
            !N1NearlyEqual(converted, std::floor(converted)))
            return false;
        index = (int)converted;
        return true;
    } catch (...) {
        return false;
    }
}

static bool N1GetTalentStruct(const RValue& map, int talentId, RValue& talent, std::string& why)
{
    try {
        RValue key((double)talentId);
        RValue exists = g_Yytk->CallBuiltin("ds_map_exists", { map, key });
        if (!exists.ToBoolean()) {
            why = "talentStructMap missing id " + std::to_string(talentId);
            return false;
        }
        talent = g_Yytk->CallBuiltin("ds_map_find_value", { map, key });
        if (talent.m_Kind != VALUE_OBJECT) {
            why = "talent " + std::to_string(talentId) + " is not a struct";
            return false;
        }
        return true;
    } catch (...) {
        why = "talent lookup threw for id " + std::to_string(talentId);
        return false;
    }
}

static bool N1ReadStructNumber(const RValue& talent, const char* field, double& value, std::string& why)
{
    try {
        RValue exists = g_Yytk->CallBuiltin("variable_struct_exists", { talent, RValue(field) });
        if (!exists.ToBoolean()) {
            why = std::string("missing field ") + field;
            return false;
        }
        RValue current = g_Yytk->CallBuiltin("variable_struct_get", { talent, RValue(field) });
        if (!N1Numeric(current)) {
            why = std::string("non-numeric field ") + field;
            return false;
        }
        value = current.ToDouble();
        if (!std::isfinite(value)) {
            why = std::string("non-finite field ") + field;
            return false;
        }
        return true;
    } catch (...) {
        why = std::string("read threw for field ") + field;
        return false;
    }
}

static bool N1WriteStructNumber(const RValue& talent, const N1TalentField& spec,
                                double target, std::string& why)
{
    try {
        g_Yytk->CallBuiltin("variable_struct_set",
                            { talent, RValue(spec.field), RValue(target) });
        double after = 0.0;
        if (!N1ReadStructNumber(talent, spec.field, after, why) || !N1NearlyEqual(after, target)) {
            why = "write verify failed id " + std::to_string(spec.talentId) + "." + spec.field;
            return false;
        }
        return true;
    } catch (...) {
        why = "write threw id " + std::to_string(spec.talentId) + "." + spec.field;
        return false;
    }
}

static bool N1GetTalentMap(RValue& map, std::string& why)
{
    try {
        RValue exists = g_Yytk->CallBuiltin("variable_global_exists", { RValue("talentStructMap") });
        if (!exists.ToBoolean()) { why = "global.talentStructMap is not ready"; return false; }
        map = g_Yytk->CallBuiltin("variable_global_get", { RValue("talentStructMap") });
        // GameMaker's ds_exists(id, ds_type_map) is the lifetime/type gate.
        // ds_type_map is 1.  It is authoritative because current runners may
        // represent a live data-structure handle as VALUE_REF, not a real.
        RValue liveMap = g_Yytk->CallBuiltin("ds_exists", { map, RValue(1.0) });
        if (!liveMap.ToBoolean()) { why = "global.talentStructMap is not a live ds-map"; return false; }
        return true;
    } catch (...) {
        why = "talentStructMap lookup threw";
        return false;
    }
}

static bool N1ApplyTalentMap(bool enable, std::string& detail)
{
    RValue map;
    if (!N1GetTalentMap(map, detail)) return false;

    std::vector<N1ObservedField> observed;
    observed.reserve(sizeof(kN1TalentFields) / sizeof(kN1TalentFields[0]));
    int mismatches = 0;
    bool sawVanillaProfile = false;
    bool sawBalancedProfile = false;

    for (const auto& spec : kN1TalentFields) {
        RValue talent;
        std::string why;
        double abilityId = 0.0;
        double current = 0.0;
        if (!N1GetTalentStruct(map, spec.talentId, talent, why)) {
            detail = why;
            return false;
        }
        if (!N1ReadStructNumber(talent, "abilityId", abilityId, why)) {
            detail = "id " + std::to_string(spec.talentId) + ": " + why;
            return false;
        }
        if (!N1NearlyEqual(abilityId, (double)spec.talentId)) {
            detail = "semantic gate rejected map key " + std::to_string(spec.talentId) +
                     " abilityId=" + std::to_string(abilityId);
            return false;
        }
        if (!N1ReadStructNumber(talent, spec.field, current, why)) {
            detail = why;
            return false;
        }
        const bool known = N1NearlyEqual(current, spec.vanilla) ||
                           N1NearlyEqual(current, spec.balanced);
        if (!known) {
            mismatches++;
            if (enable) {
                detail = "semantic gate rejected id " + std::to_string(spec.talentId) + "." +
                         spec.field + " current=" + std::to_string(current);
                return false; // whole-set preflight: enabling never partially writes
            }
        } else if (!N1NearlyEqual(spec.vanilla, spec.balanced)) {
            if (N1NearlyEqual(current, spec.vanilla)) sawVanillaProfile = true;
            else sawBalancedProfile = true;
        }
        observed.push_back({ &spec, talent, current });
    }

    // Exact per-field values are not enough: an interrupted/foreign partial N1
    // profile must not be normalized silently.  Enable accepts only a complete
    // vanilla profile or a complete N1 profile.  Equal vanilla/N1 pairs do not
    // participate in this decision.
    if (enable && sawVanillaProfile && sawBalancedProfile) {
        detail = "semantic gate rejected mixed vanilla/N1 profile";
        return false;
    }
    if (enable && sawBalancedProfile && !sawVanillaProfile &&
        !g_NecroBalanceOwned.load()) {
        detail = "semantic gate rejected unowned full N1 profile";
        return false;
    }

    int writes = 0;
    int writeFailures = 0;
    std::string firstWriteFailure;
    for (size_t i = 0; i < observed.size(); i++) {
        auto& item = observed[i];
        const double target = enable ? item.spec->balanced : item.spec->vanilla;

        // On disable, restore only values which are recognizably vanilla/N1.
        // A third-party value is left untouched rather than being overwritten.
        if (!N1NearlyEqual(item.before, item.spec->vanilla) &&
            !N1NearlyEqual(item.before, item.spec->balanced))
            continue;
        if (N1NearlyEqual(item.before, target)) continue;

        std::string why;
        if (!N1WriteStructNumber(item.talent, *item.spec, target, why)) {
            if (enable) {
                // A setter can mutate the current field and still fail its read-back
                // verification. Restore it too, then every earlier write in reverse.
                bool rollbackOk = true;
                std::string rollbackWhy;
                {
                    std::string currentWhy;
                    if (!N1WriteStructNumber(item.talent, *item.spec, item.before, currentWhy)) {
                        rollbackOk = false;
                        rollbackWhy = "current=" + currentWhy;
                    }
                }
                for (size_t j = i; j-- > 0;) {
                    auto& previous = observed[j];
                    if (!N1NearlyEqual(previous.before, previous.spec->balanced)) {
                        std::string previousWhy;
                        if (!N1WriteStructNumber(previous.talent, *previous.spec,
                                                 previous.before, previousWhy)) {
                            rollbackOk = false;
                            if (!rollbackWhy.empty()) rollbackWhy += "; ";
                            rollbackWhy += "id " + std::to_string(previous.spec->talentId) +
                                           "." + previous.spec->field + "=" + previousWhy;
                        }
                    }
                }
                if (!rollbackOk) {
                    g_NecroBalanceEnabled.store(false);
                    g_NecroBalanceOwned.store(true);
                    g_NecroRestorePending.store(true);
                    detail = why + "; rollback FAILED: " + rollbackWhy;
                } else {
                    detail = why + "; rollback verified";
                }
                return false;
            }
            writeFailures++;
            if (firstWriteFailure.empty()) firstWriteFailure = why;
            continue;
        }
        writes++;
    }

    detail = std::string(enable ? "N1 applied" : "vanilla restored") +
              " writes=" + std::to_string(writes);
    if (mismatches) detail += " untouched-mismatches=" + std::to_string(mismatches);
    if (writeFailures) {
        detail += " write-failures=" + std::to_string(writeFailures) +
                  " first=" + firstWriteFailure;
    }
    return mismatches == 0 && writeFailures == 0;
}

// Restore is intentionally not the inverse activation transaction.  Each field
// is independent so one missing/foreign entry cannot prevent later owned exact
// N1 values from being restored.  Unknown values are never overwritten.
static bool N1RestoreTalentMapOwned(std::string& detail)
{
    RValue map;
    if (!N1GetTalentMap(map, detail)) return false;

    int writes = 0;
    int failures = 0;
    std::string firstFailure;
    for (const auto& spec : kN1TalentFields) {
        RValue talent;
        std::string why;
        double abilityId = 0.0;
        double current = 0.0;
        bool fieldOk = true;
        if (!N1GetTalentStruct(map, spec.talentId, talent, why)) {
            fieldOk = false;
        } else if (!N1ReadStructNumber(talent, "abilityId", abilityId, why)) {
            fieldOk = false;
        } else if (!N1NearlyEqual(abilityId, (double)spec.talentId)) {
            why = "abilityId mismatch=" + std::to_string(abilityId);
            fieldOk = false;
        } else if (!N1ReadStructNumber(talent, spec.field, current, why)) {
            fieldOk = false;
        } else if (N1NearlyEqual(current, spec.vanilla)) {
            continue;
        } else if (!N1NearlyEqual(current, spec.balanced)) {
            why = "unknown current=" + std::to_string(current);
            fieldOk = false;
        } else if (!N1WriteStructNumber(talent, spec, spec.vanilla, why)) {
            fieldOk = false;
        } else {
            writes++;
            continue;
        }

        if (!fieldOk) {
            failures++;
            if (firstFailure.empty()) {
                firstFailure = "id " + std::to_string(spec.talentId) + "." +
                               spec.field + " " + why;
            }
        }
    }

    detail = "owned talent restore writes=" + std::to_string(writes) +
             " failures=" + std::to_string(failures);
    if (!firstFailure.empty()) detail += " first=" + firstFailure;
    return failures == 0;
}

// Pure read-only live audit for the IPC status command.  Besides reporting the
// profile, this independently checks every map key's embedded abilityId.
static std::string N1AuditTalentProfile(int& fields, std::string& why)
{
    fields = 0;
    RValue map;
    if (!N1GetTalentMap(map, why)) return "UNAVAILABLE";

    bool sawVanilla = false;
    bool sawBalanced = false;
    for (const auto& spec : kN1TalentFields) {
        RValue talent;
        double abilityId = 0.0;
        double current = 0.0;
        if (!N1GetTalentStruct(map, spec.talentId, talent, why) ||
            !N1ReadStructNumber(talent, "abilityId", abilityId, why) ||
            !N1NearlyEqual(abilityId, (double)spec.talentId) ||
            !N1ReadStructNumber(talent, spec.field, current, why)) {
            if (why.empty()) {
                why = "abilityId mismatch at map key " + std::to_string(spec.talentId);
            }
            return "UNAVAILABLE";
        }
        fields++;

        if (N1NearlyEqual(spec.vanilla, spec.balanced)) {
            if (!N1NearlyEqual(current, spec.vanilla)) {
                why = "unknown value at id " + std::to_string(spec.talentId) + "." + spec.field;
                return "UNAVAILABLE";
            }
        } else if (N1NearlyEqual(current, spec.vanilla)) {
            sawVanilla = true;
        } else if (N1NearlyEqual(current, spec.balanced)) {
            sawBalanced = true;
        } else {
            why = "unknown value at id " + std::to_string(spec.talentId) + "." + spec.field;
            return "UNAVAILABLE";
        }
    }

    if (sawVanilla && sawBalanced) return "MIXED";
    if (sawBalanced) return "N1";
    if (sawVanilla) return "VANILLA";
    why = "no changed fields in manifest";
    return "UNAVAILABLE";
}

struct N1RangeAudit
{
    bool available = false;
    int warriors = 0;
    int vanilla = 0;
    int balanced = 0;
    int other = 0;
    double firstOther = 0.0;
    bool sameOther = true;
    std::string why;
};

// Read-only enumeration used both by activation preflight and status.  It never
// resolves/caches an asset, writes an instance field, or changes telemetry.
static N1RangeAudit N1AuditWarriorRanges()
{
    N1RangeAudit audit;
    if (!g_Yytk || g_NecroWarriorObjectIndex < 0) {
        audit.why = "exact Warrior object unavailable";
        return audit;
    }
    try {
        RValue object((double)g_NecroWarriorObjectIndex);
        RValue countValue = g_Yytk->CallBuiltin("instance_number", { object });
        if (!N1Numeric(countValue) || !std::isfinite(countValue.ToDouble()) ||
            countValue.ToDouble() < 0.0 ||
            !N1NearlyEqual(countValue.ToDouble(), std::floor(countValue.ToDouble()))) {
            audit.why = "invalid Warrior instance count";
            return audit;
        }
        audit.warriors = (int)countValue.ToDouble();
        for (int i = 0; i < audit.warriors; i++) {
            RValue instance = g_Yytk->CallBuiltin("instance_find", { object, RValue((double)i) });
            if (!g_Yytk->CallBuiltin("instance_exists", { instance }).ToBoolean()) {
                audit.why = "Warrior disappeared during audit";
                return audit;
            }
            RValue objectIndex = g_Yytk->CallBuiltin("variable_instance_get",
                                                     { instance, RValue("object_index") });
            int exactObjectIndex = -1;
            if (!N1ObjectIndex(objectIndex, exactObjectIndex) ||
                exactObjectIndex != g_NecroWarriorObjectIndex) {
                audit.why = "instance_find returned a non-Warrior";
                return audit;
            }
            RValue range = g_Yytk->CallBuiltin("variable_instance_get",
                                               { instance, RValue("playerRange") });
            if (!N1Numeric(range) || !std::isfinite(range.ToDouble())) {
                audit.why = "non-numeric Warrior playerRange";
                return audit;
            }
            const double current = range.ToDouble();
            if (N1NearlyEqual(current, kN1WarriorPlayerRangeVanilla)) {
                audit.vanilla++;
            } else if (N1NearlyEqual(current, kN1WarriorPlayerRangeBalanced)) {
                audit.balanced++;
            } else {
                if (audit.other == 0) audit.firstOther = current;
                else if (!N1NearlyEqual(audit.firstOther, current)) audit.sameOther = false;
                audit.other++;
            }
        }
        audit.available = true;
        return audit;
    } catch (...) {
        audit.why = "Warrior range audit threw";
        return audit;
    }
}

static std::string N1RangeAuditLabel(const N1RangeAudit& audit)
{
    if (!audit.available) return "UNAVAILABLE";
    if (audit.warriors == 0) return "NO_INSTANCES";
    if (audit.vanilla == audit.warriors) return "48";
    if (audit.balanced == audit.warriors) return "64";
    if (audit.other == audit.warriors && audit.sameOther)
        return "OTHER(" + std::to_string(audit.firstOther) + ")";
    return "MIXED";
}

static bool N1RangeAuditMatches(const N1RangeAudit& audit, bool enable)
{
    if (!audit.available) return false;
    if (audit.warriors == 0) return true;
    return enable ? audit.balanced == audit.warriors
                  : audit.vanilla == audit.warriors;
}

static bool N1PatchWarriorRange(const RValue& instance, bool enable)
{
    if (!g_Yytk || g_NecroWarriorObjectIndex < 0) return false;
    try {
        RValue exists = g_Yytk->CallBuiltin("instance_exists", { instance });
        if (!exists.ToBoolean()) return false;
        RValue objectIndex = g_Yytk->CallBuiltin("variable_instance_get",
                                                 { instance, RValue("object_index") });
        int exactObjectIndex = -1;
        if (!N1ObjectIndex(objectIndex, exactObjectIndex) ||
            exactObjectIndex != g_NecroWarriorObjectIndex)
            return false;
        RValue range = g_Yytk->CallBuiltin("variable_instance_get",
                                           { instance, RValue("playerRange") });
        if (!N1Numeric(range)) { InterlockedIncrement(&g_NecroRangeRejected); return false; }
        const double current = range.ToDouble();
        const double source = enable ? kN1WarriorPlayerRangeVanilla
                                     : kN1WarriorPlayerRangeBalanced;
        const double target = enable ? kN1WarriorPlayerRangeBalanced
                                     : kN1WarriorPlayerRangeVanilla;
        if (N1NearlyEqual(current, target)) return true;
        if (!N1NearlyEqual(current, source)) {
            InterlockedIncrement(&g_NecroRangeRejected);
            return false;
        }
        g_Yytk->CallBuiltin("variable_instance_set",
                            { instance, RValue("playerRange"), RValue(target) });
        RValue after = g_Yytk->CallBuiltin("variable_instance_get",
                                           { instance, RValue("playerRange") });
        if (!N1Numeric(after) || !N1NearlyEqual(after.ToDouble(), target)) {
            InterlockedIncrement(&g_NecroRangeRejected);
            return false;
        }
        InterlockedIncrement(&g_NecroRangeWrites);
        return true;
    } catch (...) {
        InterlockedIncrement(&g_NecroRangeRejected);
        return false;
    }
}

static void N1ResolveWarriorObject()
{
    if (g_NecroWarriorObjectIndex >= 0 || !g_Yytk) return;
    try {
        static constexpr const char* kExactWarriorObjectName =
            "Summon_Skeleton_Warrior_obj";
        RValue object = g_Yytk->CallBuiltin("asset_get_index",
                                            { RValue(kExactWarriorObjectName) });
        int candidate = -1;
        if (!N1ObjectIndex(object, candidate)) return;

        // Never trust the numeric/reference conversion by itself.  The
        // round-trip name gate prevents a stale or differently tagged asset
        // reference from granting write access to an unrelated object.
        RValue resolvedName = g_Yytk->CallBuiltin("object_get_name",
                                                  { RValue((double)candidate) });
        if (resolvedName.m_Kind != VALUE_STRING ||
            resolvedName.ToString() != kExactWarriorObjectName)
            return;
        g_NecroWarriorObjectIndex = candidate;
    } catch (...) {}
}

static bool N1SweepWarriorRange(bool enable, int& warriors, std::string& why)
{
    warriors = 0;
    why.clear();
    N1ResolveWarriorObject();
    if (g_NecroWarriorObjectIndex < 0) {
        why = "exact Warrior object unavailable";
        return false;
    }
    try {
        RValue object((double)g_NecroWarriorObjectIndex);
        RValue countValue = g_Yytk->CallBuiltin("instance_number", { object });
        if (!N1Numeric(countValue) || !std::isfinite(countValue.ToDouble()) ||
            countValue.ToDouble() < 0.0 ||
            !N1NearlyEqual(countValue.ToDouble(), std::floor(countValue.ToDouble()))) {
            why = "invalid Warrior instance count";
            return false;
        }
        warriors = (int)countValue.ToDouble();
        int failed = 0;
        for (int i = 0; i < warriors; i++) {
            RValue instance = g_Yytk->CallBuiltin("instance_find", { object, RValue((double)i) });
            if (!N1PatchWarriorRange(instance, enable)) failed++;
        }
        if (failed) {
            why = "Warrior range failures=" + std::to_string(failed) +
                  "/" + std::to_string(warriors);
            return false;
        }
        why = "Warrior ranges verified=" + std::to_string(warriors);
        return true;
    } catch (...) {
        InterlockedIncrement(&g_NecroRangeRejected);
        why = "Warrior range sweep threw";
        return false;
    }
}

static bool N1VerifyExpectedLiveProfile(bool enable, std::string& profile,
                                        int& fields, N1RangeAudit& range,
                                        std::string& why)
{
    std::string talentWhy;
    profile = N1AuditTalentProfile(fields, talentWhy);
    range = N1AuditWarriorRanges();
    const bool talentOk = fields == kN1TalentFieldCount &&
                          profile == (enable ? "N1" : "VANILLA");
    const bool rangeOk = N1RangeAuditMatches(range, enable);
    if (talentOk && rangeOk) {
        why.clear();
        return true;
    }
    why = "profile=" + profile + " fields=" + std::to_string(fields) +
          " range=" + N1RangeAuditLabel(range);
    if (!talentWhy.empty()) why += " talent=" + talentWhy;
    if (!range.why.empty()) why += " warrior=" + range.why;
    return false;
}

// Ownership is released only after an independent read-only audit proves that
// both resources we may have changed are fully vanilla again.  Operation return
// values are retained in telemetry, but the final audit is authoritative.
static bool N1RestoreOwnedState(std::string& detail)
{
    g_NecroBalanceEnabled.store(false);
    g_NecroBalanceOwned.store(true);
    g_NecroRestorePending.store(true);

    std::string mapDetail;
    const bool mapWriteOk = N1RestoreTalentMapOwned(mapDetail);
    int sweptWarriors = 0;
    std::string sweepDetail;
    const bool rangeWriteOk = N1SweepWarriorRange(false, sweptWarriors, sweepDetail);

    std::string profile;
    int fields = 0;
    N1RangeAudit range;
    std::string verifyWhy;
    const bool verified = N1VerifyExpectedLiveProfile(false, profile, fields, range, verifyWhy);
    if (verified) {
        g_NecroRangeIntegrity.store(true);
        g_NecroRestorePending.store(false);
        g_NecroBalanceOwned.store(false);
        detail = "restore verified; map_op=" + std::string(mapWriteOk ? "OK" : "FINAL_OK") +
                 " range_op=" + std::string(rangeWriteOk ? "OK" : "FINAL_OK") +
                 " warriors=" + std::to_string(range.warriors);
        return true;
    }

    g_NecroBalanceOwned.store(true);
    g_NecroRestorePending.store(true);
    g_NecroRangeIntegrity.store(N1RangeAuditMatches(range, false));
    detail = "RESTORE PENDING; map_op=" + std::string(mapWriteOk ? "OK" : "FAIL") +
             " (" + mapDetail + ") range_op=" + std::string(rangeWriteOk ? "OK" : "FAIL") +
             " (" + sweepDetail + ") verify=" + verifyWhy;
    return false;
}

static bool N1RecoverRangeIntegrityIfVerified()
{
    if (!g_NecroBalanceEnabled.load() || !g_NecroBalanceOwned.load()) return false;
    N1RangeAudit range = N1AuditWarriorRanges();
    if (!N1RangeAuditMatches(range, true)) return false;
    g_NecroRangeIntegrity.store(true);
    return true;
}

static void NecroBalancePostCreatedInstance(int objectIndex, RValue& instanceId)
{
    if (!g_NecroBalanceEnabled.load()) return;
    N1ResolveWarriorObject();
    if (objectIndex != g_NecroWarriorObjectIndex) return;
    if (!N1PatchWarriorRange(instanceId, true)) {
        g_NecroRangeIntegrity.store(false);
        g_NecroBalanceEnabled.store(false);
        g_NecroBalanceOwned.store(true);
        g_NecroRestorePending.store(true);
        InterlockedIncrement(&g_NecroApplyRejected);
        // Do not re-enter the toggle command path from an instance-create hook.
        // The restore helper performs no creates and independently verifies both
        // the talent map and every surviving Warrior before releasing ownership.
        std::string restoreDetail;
        const bool restored = N1RestoreOwnedState(restoreDetail);
        g_NecroLastStatus = "INTEGRITY FAIL: post-create Warrior range patch rejected; " +
                            restoreDetail + (restored ? "; OffClean" : "; retry OFF");
        Out("necrobal: " + g_NecroLastStatus);
        return;
    }
    if (!g_NecroRangeIntegrity.load() && N1RecoverRangeIntegrityIfVerified()) {
        g_NecroLastStatus = "range integrity recovered by verified post-create patch";
        Out("necrobal: " + g_NecroLastStatus);
    }
}

static RValue& HookPopulateTalentStructMapNecromancerN1(
    CInstance* Self, CInstance* Other, RValue& Result, int argc, RValue** Args)
{
    InterlockedIncrement(&g_NecroPopulateCalls);
    RValue& result = g_OrigPopulateTalentStructMapNecromancer
        ? g_OrigPopulateTalentStructMapNecromancer(Self, Other, Result, argc, Args)
        : Result;

    if (g_NecroBalanceEnabled.load()) {
        // Populate replaces the map generation.  Re-establish both halves of
        // N1, then independently verify before keeping enabled published.
        std::string mapDetail;
        const bool mapOk = N1ApplyTalentMap(true, mapDetail);
        int sweptWarriors = 0;
        std::string sweepDetail = "not run";
        const bool rangeOk = mapOk &&
            N1SweepWarriorRange(true, sweptWarriors, sweepDetail);
        std::string profile;
        int fields = 0;
        N1RangeAudit range;
        std::string verifyDetail = "not run";
        const bool verified = mapOk && rangeOk &&
            N1VerifyExpectedLiveProfile(true, profile, fields, range, verifyDetail);
        if (verified) {
            g_NecroBalanceOwned.store(true);
            g_NecroRestorePending.store(false);
            g_NecroRangeIntegrity.store(true);
            InterlockedIncrement(&g_NecroApplyOk);
            g_NecroLastStatus = "populate hook verified: " + mapDetail +
                                "; " + sweepDetail;
        } else {
            // This path was entered only while N1 was enabled, therefore the
            // aggregate ownership is ours even if this new map generation fails.
            g_NecroBalanceEnabled.store(false);
            g_NecroBalanceOwned.store(true);
            g_NecroRestorePending.store(true);
            if (!rangeOk) g_NecroRangeIntegrity.store(false);
            InterlockedIncrement(&g_NecroApplyRejected);
            std::string restoreDetail;
            const bool restored = N1RestoreOwnedState(restoreDetail);
            g_NecroLastStatus = "populate hook rejected: map=" + mapDetail +
                                " range=" + sweepDetail + " verify=" + verifyDetail +
                                "; " + restoreDetail;
            Out(std::string("necrobal: FAIL-CLOSED - ") + g_NecroLastStatus +
                (restored ? "" : " (retry OFF)"));
        }
    }
    return result;
}

static RValue& HookLoadSummonStatsN1(
    CInstance* Self, CInstance* Other, RValue& Result, int argc, RValue** Args)
{
    InterlockedIncrement(&g_NecroLoadStatsCalls);
    RValue& result = g_OrigLoadSummonStatsN1
        ? g_OrigLoadSummonStatsN1(Self, Other, Result, argc, Args)
        : Result;
    // This is useful if LoadSummonStats is called after child initialization.
    // The post-create path remains authoritative because initial inherited
    // Create can run before summonTalent/playerRange are assigned by Warrior.
    if (g_NecroBalanceEnabled.load() && Self) {
        // LoadSummonStats can run from inherited Create before Warrior's child
        // Create assigns playerRange=48.  A false result here is therefore only
        // deferred telemetry; the final post-create result is authoritative.
        bool exactWarrior = false;
        try {
            RValue instance(Self);
            RValue objectIndex = g_Yytk->CallBuiltin("variable_instance_get",
                                                     { instance, RValue("object_index") });
            int exactObjectIndex = -1;
            exactWarrior = N1ObjectIndex(objectIndex, exactObjectIndex) &&
                exactObjectIndex == g_NecroWarriorObjectIndex;
            if (exactWarrior && !N1PatchWarriorRange(instance, true))
                InterlockedIncrement(&g_NecroLoadStatsDeferred);
        } catch (...) {
            if (exactWarrior) InterlockedIncrement(&g_NecroLoadStatsDeferred);
        }
    }
    return result;
}

static void InstallNecroBalanceHooks()
{
    if (g_NecroBalanceHookInstalled) return;
    N1ResolveWarriorObject();
    bool populate = g_OrigPopulateTalentStructMapNecromancer != nullptr;
    if (!populate) {
        populate = HookOneScript("PopulateTalentStructMapNecromancer", "fp_n1_necro_populate",
                                 (PVOID)HookPopulateTalentStructMapNecromancerN1,
                                 &g_OrigPopulateTalentStructMapNecromancer);
    }
    // Optional second safe boundary; post-create still guarantees final range.
    if (!g_OrigLoadSummonStatsN1) {
        HookOneScript("LoadSummonStats", "fp_n1_load_summon_stats",
                      (PVOID)HookLoadSummonStatsN1, &g_OrigLoadSummonStatsN1);
    }
    g_NecroBalanceHookInstalled = populate;
    g_NecroLastStatus = populate ? "hooks ready; N1 off" : "populate hook unavailable; N1 locked off";
}

static void SetNecroBalance(bool enabled)
{
    if (enabled) {
        // N1 is opt-in. Its hooks are installed only on the first ON command;
        // an untouched player build has no Necromancer interception cost.
        InstallCreateHooks();
        InstallNecroBalanceHooks();
        bool wasEnabled = g_NecroBalanceEnabled.load();
        bool wasOwned = g_NecroBalanceOwned.load();
        const bool postCreateReady = g_NecroPostCreateHooksInstalled.load();
        if (!g_NecroBalanceHookInstalled || !postCreateReady) {
            g_NecroBalanceEnabled.store(false);
            if (wasOwned) {
                g_NecroBalanceOwned.store(true);
                g_NecroRestorePending.store(true);
            }
            InterlockedIncrement(&g_NecroApplyRejected);
            g_NecroLastStatus = "enable rejected: required hooks unavailable populate=" +
                                std::to_string(g_NecroBalanceHookInstalled ? 1 : 0) +
                                " postcreate=" + std::to_string(postCreateReady ? 1 : 0) +
                                (wasOwned ? "; RESTORE PENDING" : "; zero mutation");
            Out("necrobal: FAIL-CLOSED - " + g_NecroLastStatus);
            return;
        }
        N1ResolveWarriorObject();
        if (g_NecroWarriorObjectIndex < 0) {
            g_NecroBalanceEnabled.store(false);
            if (wasOwned) {
                g_NecroBalanceOwned.store(true);
                g_NecroRestorePending.store(true);
            }
            InterlockedIncrement(&g_NecroApplyRejected);
            g_NecroLastStatus = std::string("enable rejected: exact Warrior object unavailable") +
                                (wasOwned ? "; RESTORE PENDING" : "; zero mutation");
            Out("necrobal: FAIL-CLOSED - " + g_NecroLastStatus);
            return;
        }

        // A disabled-but-owned state must finish its previous cleanup before a
        // fresh activation.  This also repairs any impossible enabled&&!owned
        // state conservatively by assuming ownership.
        if ((wasOwned && (!wasEnabled || g_NecroRestorePending.load())) ||
            (wasEnabled && !wasOwned)) {
            g_NecroBalanceEnabled.store(false);
            g_NecroBalanceOwned.store(true);
            g_NecroRestorePending.store(true);
            std::string cleanupDetail;
            if (!N1RestoreOwnedState(cleanupDetail)) {
                InterlockedIncrement(&g_NecroApplyRejected);
                g_NecroLastStatus = "enable deferred: " + cleanupDetail;
                Out("necrobal: FAIL-CLOSED - " + g_NecroLastStatus);
                return;
            }
            wasEnabled = false;
            wasOwned = false;
        }

        // An unowned activation may claim only a completely vanilla map and
        // exact-48 existing Warriors.  Therefore every initial rejection below
        // is guaranteed to be zero-mutation with respect to game state.
        if (!g_NecroBalanceOwned.load()) {
            std::string preflightProfile;
            int preflightFields = 0;
            N1RangeAudit preflightRange;
            std::string preflightWhy;
            if (!N1VerifyExpectedLiveProfile(false, preflightProfile, preflightFields,
                                             preflightRange, preflightWhy)) {
                g_NecroBalanceEnabled.store(false);
                InterlockedIncrement(&g_NecroApplyRejected);
                g_NecroLastStatus = "enable rejected zero-mutation preflight: " + preflightWhy;
                Out("necrobal: FAIL-CLOSED - " + g_NecroLastStatus);
                return;
            }
        }

        // Stop hook-side writes while the map+range transaction is in flight.
        g_NecroBalanceEnabled.store(false);
        const bool activationWasOwned = g_NecroBalanceOwned.load();
        std::string mapDetail;
        if (!N1ApplyTalentMap(true, mapDetail)) {
            const bool mustRestore = activationWasOwned || g_NecroBalanceOwned.load();
            if (mustRestore) {
                g_NecroBalanceOwned.store(true);
                g_NecroRestorePending.store(true);
                std::string restoreDetail;
                const bool restored = N1RestoreOwnedState(restoreDetail);
                g_NecroLastStatus = "enable rejected: " + mapDetail + "; " + restoreDetail;
                if (!restored) g_NecroLastStatus += " (retry OFF)";
            } else {
                g_NecroRestorePending.store(false);
                g_NecroLastStatus = "enable rejected zero-mutation: " + mapDetail;
            }
            InterlockedIncrement(&g_NecroApplyRejected);
            Out("necrobal: FAIL-CLOSED - " + g_NecroLastStatus);
            return;
        }

        // From the first successful talent write onward this process owns N1,
        // but remains disabled/pending until Warrior range and live audit pass.
        g_NecroBalanceOwned.store(true);
        g_NecroRestorePending.store(true);
        int sweptWarriors = 0;
        std::string sweepDetail;
        const bool rangeOk = N1SweepWarriorRange(true, sweptWarriors, sweepDetail);
        std::string profile;
        int fields = 0;
        N1RangeAudit range;
        std::string verifyDetail;
        const bool verified = rangeOk &&
            N1VerifyExpectedLiveProfile(true, profile, fields, range, verifyDetail);
        if (!verified) {
            if (!rangeOk) g_NecroRangeIntegrity.store(false);
            std::string restoreDetail;
            const bool restored = N1RestoreOwnedState(restoreDetail);
            InterlockedIncrement(&g_NecroApplyRejected);
            g_NecroLastStatus = "enable range/verify rejected: " + sweepDetail +
                                " verify=" + verifyDetail + "; " + restoreDetail;
            if (!restored) g_NecroLastStatus += " (retry OFF)";
            Out("necrobal: FAIL-CLOSED - " + g_NecroLastStatus);
            return;
        }

        g_NecroBalanceOwned.store(true);
        g_NecroRestorePending.store(false);
        g_NecroRangeIntegrity.store(true);
        g_NecroBalanceEnabled.store(true); // publish LAST
        InterlockedIncrement(&g_NecroApplyOk);
        g_NecroLastStatus = mapDetail + "; " + sweepDetail + "; live verified";
        Out("necrobal: ON - " + g_NecroLastStatus);
    } else {
        // Clear enabled first.  OFF is a no-op only when this process owns
        // nothing and has no unfinished cleanup from a previous attempt.
        const bool wasEnabled = g_NecroBalanceEnabled.exchange(false);
        bool owned = g_NecroBalanceOwned.load();
        const bool pending = g_NecroRestorePending.load();
        if (!wasEnabled && !owned && !pending) {
            g_NecroLastStatus = "off: already disabled (no-op)";
            Out("necrobal: OFF - already disabled (no-op)");
            return;
        }
        if (wasEnabled && !owned) {
            owned = true; // impossible invariant: retain ownership conservatively
            g_NecroBalanceOwned.store(true);
        }
        if (pending && !owned) {
            owned = true;
            g_NecroBalanceOwned.store(true);
        }
        g_NecroRestorePending.store(true);
        std::string detail;
        const bool restored = N1RestoreOwnedState(detail);
        if (restored) InterlockedIncrement(&g_NecroApplyOk);
        else InterlockedIncrement(&g_NecroApplyRejected);
        g_NecroLastStatus = "off: " + detail;
        Out(std::string("necrobal: OFF - ") + detail +
            (restored ? "" : " (retry OFF)"));
    }
}

static void NecroBalanceStatus()
{
    // All calls below are read-only game queries; status never repairs or claims.
    const bool enabled = g_NecroBalanceEnabled.load();
    const bool owned = g_NecroBalanceOwned.load();
    const bool pending = g_NecroRestorePending.load();
    const bool integrity = g_NecroRangeIntegrity.load();
    int auditFields = 0;
    std::string auditWhy;
    const std::string profile = N1AuditTalentProfile(auditFields, auditWhy);
    const N1RangeAudit range = N1AuditWarriorRanges();
    const std::string rangeLabel = N1RangeAuditLabel(range);

    const bool stateOk = enabled ? (owned && !pending)
                                 : (owned ? pending : !pending);
    const bool profileOk = enabled ? profile == "N1"
                                   : (!owned && !pending && profile == "VANILLA");
    const bool rangeOk = enabled ? N1RangeAuditMatches(range, true)
                                 : (!owned && !pending && N1RangeAuditMatches(range, false));
    const bool verified = stateOk && profileOk && rangeOk && integrity &&
                          auditFields == kN1TalentFieldCount &&
                          (!enabled || (g_NecroBalanceHookInstalled &&
                                        g_NecroPostCreateHooksInstalled.load()));

    Out("necrobal: verify=" + std::string(verified ? "PASS" : "FAIL") +
        " profile=" + profile + " fields=" + std::to_string(auditFields) +
        " range=" + rangeLabel + " warriors=" + std::to_string(range.warriors) +
        " owned=" + std::to_string(owned ? 1 : 0) +
        " enabled=" + std::to_string(enabled ? 1 : 0) +
        " postcreate_hooks=" + std::to_string(g_NecroPostCreateHooksInstalled.load() ? 1 : 0) +
        " restore_pending=" + std::to_string(pending ? 1 : 0) +
        " integrity=" + std::to_string(integrity ? 1 : 0));
    char counters[384];
    sprintf_s(counters,
        "  telemetry: hook=%d postcreate_hooks=%d warrior_obj=%d populate=%ld apply_ok=%ld rejected=%ld "
        "loadstats=%ld deferred=%ld range_writes=%ld range_rejected=%ld",
        g_NecroBalanceHookInstalled ? 1 : 0,
        g_NecroPostCreateHooksInstalled.load() ? 1 : 0, g_NecroWarriorObjectIndex,
        g_NecroPopulateCalls, g_NecroApplyOk, g_NecroApplyRejected,
        g_NecroLoadStatsCalls, g_NecroLoadStatsDeferred,
        g_NecroRangeWrites, g_NecroRangeRejected);
    Out(counters);
    if (!auditWhy.empty() || !range.why.empty())
        Out("  audit: talent=" + auditWhy + " range=" + range.why);
    Out("  last: " + g_NecroLastStatus);
}

static void InstallEnemyHooks()
{
    HookOneScript("CreateEnemyFreePos", "bp_freepos", (PVOID)HookFreePos, &g_OrigFreePos);
    HookOneScript("CA_enemyCreate",     "bp_create",  (PVOID)HookCreate,  &g_OrigCreate);
    HookOneScript("CreateEnemyElite",   "bp_elite",   (PVOID)HookElite,   &g_OrigElite);
    // NOTE: CreateOnlineGame hook removed - it crashed the game on startup (the online
    // manager calls it during menu init). Online is already blocked by launching EAC-free
    // (direct Hero_Siege.exe -> EAC never bootstraps -> cannot connect to online servers).
}

static void EnemyStats()
{
    char b[260];
    sprintf_s(b, "enemystats: FreePos calls=%ld mult=%d | Create calls=%ld mult=%d | Elite calls=%ld mult=%d",
        g_cntFreePos, g_MultFreePos, g_cntCreate, g_MultCreate, g_cntElite, g_MultElite);
    Out(b);
}

// Builds a GM struct RValue containing all blood_pact_* modifiers from config.
static RValue BuildModifierStruct()
{
    std::map<std::string, RValue> m;
    for (auto& kv : g_Config) {
        if (kv.first.rfind("blood_pact_", 0) == 0)
            m[kv.first] = RValue(kv.second);
    }
    return RValue(m);
}

// ===== GetBloodPactInfo hook =====
// Signature: RValue& (CInstance* self, CInstance* other, RValue& result, int argc, RValue** args)
static RValue& HookGetBloodPactInfo(CInstance* Self, CInstance* Other, RValue& Result, int argc, RValue** Args)
{
    void* ret = _ReturnAddress();
    InterlockedIncrement(&g_HookCalls);

    // log distinct caller RVA
    if (g_CallerLog.size() < 900 && g_Base) {
        uintptr_t rva = (uintptr_t)ret - g_Base;
        char rb[24]; sprintf_s(rb, "<0x%llX>", (unsigned long long)rva);
        if (g_CallerLog.find(rb) == std::string::npos) g_CallerLog += rb;
    }

    if (argc >= 1 && Args && Args[0]) {
        RValue* a = Args[0];
        bool numeric = (a->m_Kind == VALUE_REAL || a->m_Kind == VALUE_INT32 ||
                        a->m_Kind == VALUE_INT64 || a->m_Kind == VALUE_BOOL);
        if (numeric) {
            // The activation probe (gameplay passes a number). Return the full pact data struct.
            if (g_ProbeStruct && !g_Config.empty()) {
                InterlockedIncrement(&g_HookOverrides);
                if (g_LastKeys.size() < 1200 && g_LastKeys.find("(struct)") == std::string::npos)
                    g_LastKeys += "(struct)";
                Result = BuildModifierStruct();
                return Result;
            }
        } else {
            std::string key;
            try { key = a->ToString(); } catch (...) {}
            auto it = g_Config.find(key);
            if (it != g_Config.end()) {
                InterlockedIncrement(&g_HookOverrides);
                if (g_LastKeys.size() < 1200 && g_LastKeys.find("[" + key + "]") == std::string::npos)
                    g_LastKeys += "[" + key + "]";
                Result = RValue(it->second);
                return Result;
            }
            if (g_LastKeys.size() < 1200 && g_LastKeys.find("{" + key + "}") == std::string::npos)
                g_LastKeys += "{" + key + "}";
        }
    }
    if (g_OrigGetInfo)
        return g_OrigGetInfo(Self, Other, Result, argc, Args);
    return Result;
}

// Auto-decode: log any item struct (return value or arg) that a drop/create passes through.
// The item here is REAL + fully computed (n-array + computed stats + name) -> full decode.
static std::unordered_set<std::string> g_SeenDrop;

#ifndef FORGEPACT_RELEASE
// typemap taramasi sirasinda hangi damla tipinin islendigini soyler (-1 = tarama yok).
// LoadDrops bir sey DONDURMUYOR ve birikme listesi kullanmiyor - esyayi dogrudan
// yaratiyor.  O yuzden tipi esyaya baglamanin tek guvenilir yolu, yaratim
// kancalarini o an aktif olan tiple etiketlemek.
static int g_TypeMapAktifTip = -1;
static std::vector<std::string> g_TypeMapAdlar;
// Kanca hic atesledi mi?  "Esya uretilmedi" ile "yaratim sonraki kareye
// ertelendi" farkini ayirt etmek icin - ikisi de bos liste veriyor ama
// birinde sayac artiyor, digerinde artmiyor.
static int g_TypeMapKancaSayaci = 0;

// Esyanin gorunen ic adi: itemInfoStruct["28"]
static std::string EsyaAdiJson(const std::string& js)
{
    size_t p = js.find("\"28\":");
    if (p == std::string::npos) return "";
    p = js.find('"', p + 5);
    if (p == std::string::npos) return "";
    size_t q = js.find('"', p + 1);
    if (q == std::string::npos) return "";
    return js.substr(p + 1, q - p - 1);
}
#endif

static void LogDrop(const char* fn, RValue& res, int argc, RValue** A)
{
    try {
#ifndef FORGEPACT_RELEASE
        if (g_TypeMapAktifTip >= 0) g_TypeMapKancaSayaci++;
#endif
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        auto tryLog = [&](RValue* v) {
            if (!v || v->m_Kind != VALUE_OBJECT) return;
            RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { *v });
            std::string s = js.ToString();
            if (s.find("itemDefinitionStruct") == std::string::npos) return; // must be an item
#ifndef FORGEPACT_RELEASE
            if (g_TypeMapAktifTip >= 0) {
                // Tarama modu: tekillestirmeyi ATLA.  g_SeenDrop tum oturum
                // boyunca birikiyor; daha once gorulen bir esya elenirse o tip
                // "hicbir sey uretmedi" gibi gorunur ve harita yanlis cikar.
                std::string ad = EsyaAdiJson(s);
                g_TypeMapAdlar.push_back(std::string(fn) + ":" + (ad.empty() ? "(adsiz)" : ad));
                return;
            }
#endif
            if (!g_SeenDrop.insert(s).second) return;
            std::ofstream of(IPC_DIR + "\\itemdrops.jsonl", std::ios::app);
            of << "{\"fn\":\"" << fn << "\",\"it\":" << s << "}\n";
        };
        tryLog(&res);
        for (int i = 0; i < argc && i < 8; i++) if (A && A[i]) tryLog(A[i]);
    } catch (...) {}
}

// ===== Drop-rate hooks (multiply drop calls = more items per drop event) =====
// Gunluk yalnizca gelistirme derlemesinde: itemdrops.jsonl tek oturumda 8 MB'a
// ulasiyordu.  Carpan mantigi her iki derlemede de calisir.
// (BP_LOGDROP itself is defined near BP_DIAG_INCREMENT, at the top of the
// file - ForgePact::DropManager's hook bodies need it before this point.)

// ===== Custom Item Forge ===================================================
//
// Hero Siege does not persist an item's computed affixes in the save file.
// CreateItemNew rebuilds itemStatStruct from the compact item definition every
// time the item is loaded. The editor therefore writes a small, numeric-only
// sidecar and this hook reapplies the requested values after the game's own
// item construction has finished. Unique item mechanics use the same numeric
// stat keys (often as an inseparable skill-id/level/chance bundle), so this is
// functional game data rather than tooltip substitution.
//
// Runtime file (one item per line):
//   HS_CUSTOM_ITEM_FORGE_V1
//   item|t=4;a=123;b=30;c=1;j=0|keep=1|20=4;116=167;117=25;118=15
//
// The player build installs these item hooks only when at least one valid entry
// exists. With no sidecar, ForgePact keeps its original zero-cost release path.
struct CustomForgeEntry
{
    std::map<std::string, double> selector;
    std::map<int, double> stats;
    bool keepNative = true;
    // Optional fifth runtime field, "lore=<percent-encoded>;rarity=<n>":
    // the tooltip's italic description block is drawn from the localization
    // key stored in itemInfoStruct["29"], and the rarity label/colour from
    // itemInfoStruct["27"] (1 common, 3 rare, 5 legendary, 6 satanic,
    // 7 angelic, 9 heroic, 10 unholy).  Both are plain struct fields the game
    // reads at draw time, so they can be overridden like a stat.
    std::string lore;
    int rarity = -1;
    // Optional "tier=<n>" extra: itemInfoStruct["32"], the tooltip's Tier letter and the loot
    // filter's tier: 1 C, 2 B, 3 A, 4 S, 5 SS (measured 2026-09-06 on 98 stash uniques).
    int tier = -1;
    // Optional "mechanic=<name>" extra: a plugin-side behaviour bound to this
    // item (today only "headhunter").  The item struct is tagged with
    // fp_mechanic so the runtime can recognise it while it is equipped.
    std::string mechanic;
    // Optional "name=<percent-encoded>" extra: replaces the display name the
    // game stored in itemInfoStruct["28"] (already localized text by the time
    // CreateItemNew returns) and blanks the magic prefix/suffix fields ["5"]/["4"]
    // so a forged item is shown under exactly this name.
    std::string name;
    // Optional "affix=<percent-encoded>" extra: up to three gold text rows drawn
    // above the item's stat rows in the inventory tooltip (fp_affix on the struct).
    std::string affix;
    // True only for the plugin's own hardcoded recognition seeds (see
    // AddBuiltInSignatureEntries) - never for a line loaded from the Item
    // Editor's runtime file. These exist so a dropped/traded signature item is
    // still recognised and tagged with fp_mechanic even with no sidecar file;
    // they say nothing about whether the player actually owns one. Auto-arm
    // (HeadhunterAutoArm/TyrantAutoArm/BeaconAutoArm) must skip builtin
    // entries when deciding whether a mechanic is "wanted" - otherwise the
    // mechanic they exist to recognise appears "known" on every single
    // launch, for every player, regardless of the ForgePact panel toggle or
    // whether the player has ever seen the item (bug found 2026-09-11: a
    // player with an unmodified, un-forged install still got Headhunter
    // kill-steal tracking and status logging every session because the
    // built-in belt entry alone satisfied the old "is headhunter known"
    // check).
    bool builtin = false;
};

static std::string TrimCopy(const std::string& input)
{
    size_t first = 0;
    while (first < input.size() && std::isspace((unsigned char)input[first])) ++first;
    size_t last = input.size();
    while (last > first && std::isspace((unsigned char)input[last - 1])) --last;
    return input.substr(first, last - first);
}

static std::vector<std::string> SplitText(const std::string& input, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(input);
    std::string part;
    while (std::getline(stream, part, delimiter)) parts.push_back(part);
    return parts;
}

static bool ParseFiniteNumber(const std::string& text, double& out)
{
    try {
        const std::string clean = TrimCopy(text);
        size_t used = 0;
        out = std::stod(clean, &used);
        return used == clean.size() && std::isfinite(out) && std::abs(out) <= 1.0e12;
    } catch (...) { return false; }
}

static std::string PercentDecode(const std::string& text)
{
    std::string out; out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '%' && i + 2 < text.size() && std::isxdigit((unsigned char)text[i + 1]) && std::isxdigit((unsigned char)text[i + 2])) {
            out.push_back((char)std::stoi(text.substr(i + 1, 2), nullptr, 16)); i += 2;
        } else if (text[i] == '+') out.push_back(' ');
        else out.push_back(text[i]);
    }
    return out;
}

static bool ParseCustomForgeExtras(const std::string& text, CustomForgeEntry& entry)
{
    for (const std::string& raw : SplitText(text, ';')) {
        const std::string token = TrimCopy(raw);
        if (token.empty()) continue;
        const size_t eq = token.find('=');
        if (eq == std::string::npos) return false;
        const std::string key = TrimCopy(token.substr(0, eq));
        const std::string value = token.substr(eq + 1);
        if (key == "lore") {
            entry.lore = PercentDecode(value);
            if (entry.lore.size() > 2000) return false;
        } else if (key == "rarity") {
            double number = 0.0;
            if (!ParseFiniteNumber(value, number) || number < 0.0 || number > 20.0) return false;
            entry.rarity = (int)number;
        } else if (key == "affix") {
            entry.affix = TrimCopy(PercentDecode(value));
            if (entry.affix.empty() || entry.affix.size() > 240) return false;
            int rows = 1;
            for (unsigned char ch : entry.affix) {
                if (ch == '\n') { if (++rows > 3) return false; }
                else if (ch < 32) return false;
            }
        } else if (key == "name") {
            entry.name = TrimCopy(PercentDecode(value));
            if (entry.name.empty() || entry.name.size() > 64) return false;
            for (unsigned char ch : entry.name) if (ch < 32) return false;
        } else if (key == "mechanic") {
            std::string m = Lower(TrimCopy(value));
            if (m.empty() || m.size() > 32) return false;
            for (char ch : m) if (!(std::isalnum((unsigned char)ch) || ch == '_')) return false;
            entry.mechanic = m;
        } else if (key == "tier") {
            double number = 0.0;
            if (!ParseFiniteNumber(value, number) || number < 0.0 || number > 9.0) return false;
            entry.tier = (int)number;
        } else {
            return false;
        }
    }
    return true;
}

static std::vector<CustomForgeEntry> g_CustomForgeEntries;

// ---- item stat dump for the Item Editor -------------------------------------------
// The editor cannot decode every rolled affix from the save, but the game hands us the
// finished itemStatStruct of every item it builds.  Keyed by itemTimeStamp (the middle
// part of the editor's item key "0-0-<timestamp>-<n>"), flushed to bp_ipc\itemstats.json.
static std::unordered_map<std::string, std::string> g_ItemStatsDump;
#ifndef FORGEPACT_RELEASE
static std::unordered_map<std::string, std::string> g_ItemInfoDump;   // research: itemInfoStruct per item (tier / level / rarity research)
#endif
static std::atomic<bool> g_ItemStatsDirty{ false };
static uint32_t g_ItemStatsLastFlush = 0;
static void RecordItemStats(const RValue& item, const RValue& stats)
{
    try {
        RValue ts = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemTimeStamp") });
        std::string key = ts.ToString();
        if (key.empty() || key == "undefined" || key.size() > 32) return;
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { stats });
        std::string body = js.ToString();
        if (body.empty() || body[0] != '{' || body.size() > 4000) return;
        auto it = g_ItemStatsDump.find(key);
        if (it != g_ItemStatsDump.end() && it->second == body) return;
        if (it == g_ItemStatsDump.end() && g_ItemStatsDump.size() >= 6000) g_ItemStatsDump.clear();
        g_ItemStatsDump[key] = body;
        g_ItemStatsDirty = true;
#ifndef FORGEPACT_RELEASE
        try {
            RValue info = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemInfoStruct") });
            if (info.m_Kind == VALUE_OBJECT) {
                RValue js2; g_Yytk->CallBuiltinEx(js2, "json_stringify", g, g, { info });
                std::string ib = js2.ToString();
                if (!ib.empty() && ib[0] == '{' && ib.size() <= 6000) g_ItemInfoDump[key] = ib;
            }
        } catch (...) {}
#endif
    } catch (...) {}
}
static std::atomic<bool> g_ItemStatsWriting{ false };
static void FlushItemStats(uint32_t frame)
{
    if (!g_ItemStatsDirty.load() || frame - g_ItemStatsLastFlush < 300) return;
    if (g_ItemStatsWriting.load()) return;   // previous write still on disk; try next interval
    g_ItemStatsLastFlush = frame;
    g_ItemStatsDirty = false;
    try {
        // Assemble on the game thread (the map is only touched here), write on a
        // detached thread so a loot-heavy frame never waits for the disk.
        std::string body;
        body.reserve(g_ItemStatsDump.size() * 160 + 64);
        body += "{\"schemaVersion\":1,\"items\":{";
        bool first = true;
        for (const auto& kv : g_ItemStatsDump) { body += (first ? "" : ","); body += '"'; body += kv.first; body += "\":"; body += kv.second; first = false; }
        body += "}}";
        std::string ibody;
#ifndef FORGEPACT_RELEASE
        ibody.reserve(g_ItemInfoDump.size() * 600 + 64);
        ibody += "{\"schemaVersion\":1,\"items\":{";
        { bool f1 = true; for (const auto& kv : g_ItemInfoDump) { ibody += (f1 ? "" : ","); ibody += '"'; ibody += kv.first; ibody += "\":"; ibody += kv.second; f1 = false; } }
        ibody += "}}";
#endif
        const std::string ipath = IPC_DIR + "\\iteminfo_dump.json";
        const std::string path = IPC_DIR + "\\itemstats.json", tmp = path + ".tmp";
        g_ItemStatsWriting = true;
        std::thread([path, tmp, ipath, body = std::move(body), ibody = std::move(ibody)]() {
            try {
                if (!ibody.empty()) { std::ofstream fi(ipath, std::ios::binary | std::ios::trunc); fi << ibody; }
                { std::ofstream f(tmp, std::ios::binary | std::ios::trunc); f << body; }
                std::error_code ec;
                std::filesystem::rename(tmp, path, ec);
                if (ec) { std::filesystem::copy_file(tmp, path, std::filesystem::copy_options::overwrite_existing, ec); std::filesystem::remove(tmp, ec); }
            } catch (...) {}
            g_ItemStatsWriting = false;
        }).detach();
    } catch (...) { g_ItemStatsWriting = false; }
}
static bool g_CustomForgeHooksAttempted = false;
static bool g_CustomForgeHooksActive = false;
static volatile long g_CustomForgeApplyCount = 0;
static volatile long g_CustomForgeMechanicTags = 0;
// Set once an item struct tagged mechanic=tyrant has been created this session.  Read
// instead of walking g_ForgedItems, whose entries may point at structs the game already
// freed (walking them at the main menu crashed the game, 2026-09-05).
static std::atomic<bool> g_TyItemTagged{ false };
static std::atomic<bool> g_BeItemTagged{ false };   // same, for mechanic=beacon
// Runtime item structs that carry a mechanic tag.  The game re-creates every
// item struct on load, so the registry is refreshed by TryApplyCustomForge and
// entries whose struct pointer reappears are replaced, never duplicated.
static std::vector<RValue> g_ForgedItems;
// Game-owned registry of forged items: a global struct keyed by itemTimeStamp keeps the
// structs alive for the GC, so reading them later (worn state) is safe.
static RValue ForgeRegistryStruct(bool create)
{
    try {
        RValue reg = g_Yytk->CallBuiltin("variable_global_get", { RValue("fp_forged_items") });
        if (reg.m_Kind == VALUE_OBJECT) return reg;
        if (!create) return RValue();
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        RValue fresh; if (g) g_Yytk->CallBuiltinEx(fresh, "json_parse", g, g, { RValue("{}") });
        if (fresh.m_Kind == VALUE_OBJECT) g_Yytk->CallBuiltin("variable_global_set", { RValue("fp_forged_items"), fresh });
        return fresh;
    } catch (...) { return RValue(); }
}
static void RememberForgedItem(const RValue& item)
{
    for (auto& e : g_ForgedItems) if (e.m_Object == item.m_Object) { e = item; break; }
    if (g_ForgedItems.size() >= 64) g_ForgedItems.erase(g_ForgedItems.begin());
    g_ForgedItems.push_back(item);
    try {
        RValue reg = ForgeRegistryStruct(true);
        if (reg.m_Kind != VALUE_OBJECT) return;
        RValue ts = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemTimeStamp") });
        std::string key = ts.ToString();
        if (key.empty() || key == "undefined") return;
        g_Yytk->CallBuiltin("variable_struct_set", { reg, RValue(key), item });
    } catch (...) {}
}

static std::string CustomForgeRuntimePath()
{
    char buf[32768] = { 0 };
    DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", buf, (DWORD)sizeof(buf));
    if (n > 0 && n < sizeof(buf))
        return std::string(buf, n) + "\\Hero_Siege\\hs_custom_item_forge.runtime";
    return IPC_DIR + "\\hs_custom_item_forge.runtime";
}

static bool IsCustomForgeSelectorKey(const std::string& key)
{
    static const std::unordered_set<std::string> allowed = {
        "t", "a", "b", "c", "j", "m", "i", "g", "w", "s"
    };
    return allowed.find(key) != allowed.end();
}

static bool ParseCustomForgeSelector(const std::string& text, CustomForgeEntry& entry)
{
    for (const std::string& raw : SplitText(text, ';')) {
        const std::string token = TrimCopy(raw);
        if (token.empty()) continue;
        const size_t eq = token.find('=');
        if (eq == std::string::npos) return false;
        const std::string key = TrimCopy(token.substr(0, eq));
        double value = 0.0;
        if (!IsCustomForgeSelectorKey(key) || !ParseFiniteNumber(token.substr(eq + 1), value))
            return false;
        entry.selector[key] = value;
    }
    // Item type plus the primary seed and definition id are the minimum safe
    // identity. Additional fields emitted by the editor make the match exact.
    return entry.selector.count("t") && entry.selector.count("a") && entry.selector.count("b");
}

static bool ParseCustomForgeStats(const std::string& text, CustomForgeEntry& entry)
{
    for (const std::string& raw : SplitText(text, ';')) {
        const std::string token = TrimCopy(raw);
        if (token.empty()) continue;
        const size_t eq = token.find('=');
        if (eq == std::string::npos) return false;
        int statKey = -1;
        double value = 0.0;
        try {
            const std::string keyText = TrimCopy(token.substr(0, eq));
            size_t used = 0;
            statKey = std::stoi(keyText, &used);
            if (used != keyText.size()) return false;
        } catch (...) { return false; }
        if (statKey < 0 || statKey > 9999 ||
            !ParseFiniteNumber(token.substr(eq + 1), value)) return false;
        entry.stats[statKey] = value;
        if (entry.stats.size() > 512) return false;
    }
    return !entry.stats.empty();
}

static void WriteCustomForgeStatus(const char* detail)
{
    std::ofstream f(IPC_DIR + "\\customforge_status.json", std::ios::trunc);
    if (!f) return;
    f << "{\"schemaVersion\":1,\"entries\":" << g_CustomForgeEntries.size()
      << ",\"hooksActive\":" << (g_CustomForgeHooksActive ? "true" : "false")
      << ",\"applications\":" << g_CustomForgeApplyCount
      << ",\"detail\":\"" << detail << "\"}\n";
}

// ---- built-in signature items ---------------------------------------------------------
// The two ForgePact signature items exist as fixed Custom Forge entries inside the plugin
// (reserved seeds), so a dropped or traded copy is recognised on every load even without
// the Item Editor's runtime file.  A sidecar entry with the same selector wins.
static const double kSigCrownSeed = 777001.0;   // Great Helm  (type 0, b 7)
static const double kSigBeltSeed  = 777002.0;   // Heavy Belt  (type 8, b 2)
static bool HasCustomForgeSelector(double t, double a, double b)
{
    for (const CustomForgeEntry& e : g_CustomForgeEntries) {
        auto it = e.selector.find("t"), ia = e.selector.find("a"), ib = e.selector.find("b");
        if (it != e.selector.end() && ia != e.selector.end() && ib != e.selector.end()
            && it->second == t && ia->second == a && ib->second == b) return true;
    }
    return false;
}
static void AddBuiltInSignatureEntries()
{
    if (!HasCustomForgeSelector(0.0, kSigCrownSeed, 7.0)) {
        CustomForgeEntry crown;
        crown.selector = { {"t", 0.0}, {"a", kSigCrownSeed}, {"b", 7.0}, {"c", 0.0}, {"j", 0.0} };
        crown.stats = { {20, 4}, {29, 130}, {51, 50}, {52, 250}, {55, 16}, {154, 200}, {173, 25}, {174, 3}, {198, 15}, {201, 3}, {282, 5}, {284, 50}, {288, 1} };
        crown.keepNative = false; crown.rarity = 10; crown.tier = 5; crown.mechanic = "tyrant";
        crown.name = "Tyrant's Crown";
        crown.lore = "Every monster wants the throne. Let them die trying to take it.";
        crown.builtin = true;   // recognition only - must not by itself arm the mechanic, see CustomForgeEntry::builtin
        g_CustomForgeEntries.push_back(std::move(crown));
    }
    if (!HasCustomForgeSelector(8.0, kSigBeltSeed, 2.0)) {
        CustomForgeEntry belt;
        belt.selector = { {"t", 8.0}, {"a", kSigBeltSeed}, {"b", 2.0}, {"c", 0.0}, {"j", 0.0} };
        belt.stats = { {25, 20}, {51, 75}, {69, 20}, {172, 1}, {173, 50}, {197, 20}, {198, 25}, {201, 10}, {284, 50} };
        belt.keepNative = true; belt.rarity = 10; belt.tier = 5; belt.mechanic = "headhunter";
        belt.name = "Headhunter";
        belt.affix = "Steals the affixes of slain rare monsters for 20s";
        belt.lore = "Whoever faces its wearer shall leave all hope behind. For they will never see tomorrow.";
        belt.builtin = true;   // recognition only - must not by itself arm the mechanic, see CustomForgeEntry::builtin
        g_CustomForgeEntries.push_back(std::move(belt));
    }
}

static void RebuildForgeSelectorIndex();   // defined with the forge delta hooks below
static void LoadCustomForgeEntries()
{
    g_CustomForgeEntries.clear();
    std::ifstream f(CustomForgeRuntimePath(), std::ios::binary);
    if (!f) {
        AddBuiltInSignatureEntries();
        RebuildForgeSelectorIndex();
        WriteCustomForgeStatus("no runtime file");
        return;
    }

    std::string line;
    bool headerSeen = false;
    size_t rejected = 0;
    while (std::getline(f, line)) {
        line = TrimCopy(line);
        if (line.empty() || line[0] == '#') continue;
        if (!headerSeen) {
            if (line != "HS_CUSTOM_ITEM_FORGE_V1") {
                WriteCustomForgeStatus("unsupported runtime schema");
                return;
            }
            headerSeen = true;
            continue;
        }
        if (line.size() > 32768 || g_CustomForgeEntries.size() >= 2048) {
            ++rejected;
            continue;
        }
        const std::vector<std::string> parts = SplitText(line, '|');
        if ((parts.size() != 4 && parts.size() != 5) || TrimCopy(parts[0]) != "item") {
            ++rejected;
            continue;
        }
        CustomForgeEntry entry;
        if (!ParseCustomForgeSelector(parts[1], entry)) { ++rejected; continue; }
        const std::string keep = Lower(TrimCopy(parts[2]));
        if (keep == "keep=1") entry.keepNative = true;
        else if (keep == "keep=0") entry.keepNative = false;
        else { ++rejected; continue; }
        if (!ParseCustomForgeStats(parts[3], entry)) { ++rejected; continue; }
        if (parts.size() == 5 && !ParseCustomForgeExtras(parts[4], entry)) { ++rejected; continue; }
        g_CustomForgeEntries.push_back(std::move(entry));
    }
    AddBuiltInSignatureEntries();
    RebuildForgeSelectorIndex();
    const std::string detail = headerSeen
        ? ("loaded " + std::to_string(g_CustomForgeEntries.size()) +
           ", rejected " + std::to_string(rejected))
        : "empty runtime file";
    WriteCustomForgeStatus(detail.c_str());
    if (!g_CustomForgeEntries.empty()) Out("Custom Forge: " + detail);
}

static bool TryStructNumber(const RValue& structure, const char* field, double& value)
{
    if (structure.m_Kind != VALUE_OBJECT) return false;
    try {
        RValue exists = g_Yytk->CallBuiltin("variable_struct_exists", { structure, RValue(field) });
        if (!exists.ToBoolean()) return false;
        RValue member = g_Yytk->CallBuiltin("variable_struct_get", { structure, RValue(field) });
        if (member.m_Kind != VALUE_REAL && member.m_Kind != VALUE_INT32 &&
            member.m_Kind != VALUE_INT64 && member.m_Kind != VALUE_BOOL) return false;
        value = member.ToDouble();
        return std::isfinite(value);
    } catch (...) { return false; }
}

static bool CustomForgeMatches(const CustomForgeEntry& entry,
                               const RValue& item, const RValue& definition)
{
    for (const auto& pair : entry.selector) {
        double actual = 0.0;
        if (pair.first == "t") {
            if (!TryStructNumber(item, "itemType", actual)) return false;
        } else if (!TryStructNumber(definition, pair.first.c_str(), actual)) {
            return false;
        }
        if (std::abs(actual - pair.second) > 0.000001) return false;
    }
    return true;
}

// The game guards socketing, grid moves, stacking, pickup and market sends with
// ItemCheckHash: it re-runs the item's own GenerateItemHash() (sha1 over itemType +
// itemDefinitionStruct + itemInfoStruct + itemStatStruct with a protected salt) and
// refuses the action when the stored itemDataHash differs (ReportClient 49).  Dressing an
// item rewrites those structs, so the stored hash is refreshed right after through the
// same method - the game's own code computes it, the plugin never touches the salt.
static std::string ReadItemHash(const RValue& item)
{
    try { RValue h = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemDataHash") }); if (h.m_Kind == VALUE_STRING) return h.ToString(); } catch (...) {}
    return std::string();
}
// GenerateItemHash is not a plain member of the item struct (variable_struct_get gives
// undefined; live 2026-09-07): the constructor declares it static, and the game's own
// lookup walks the static chain.  Walk it with static_get, rebind the method to the item
// with method(), run it with script_execute.  Last resort: the routine itself with the
// item as self.
static RValue FindStructMethod(const RValue& item, const char* name, std::string& how)
{
    try {
        RValue fn = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue(name) });
        if (fn.m_Kind != VALUE_UNDEFINED && fn.m_Kind != VALUE_UNSET) { how = "member"; return fn; }
        RValue st = item;
        for (int depth = 0; depth < 6; ++depth) {
            st = g_Yytk->CallBuiltin("static_get", { st });
            if (st.m_Kind != VALUE_OBJECT) break;
            fn = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue(name) });
            if (fn.m_Kind != VALUE_UNDEFINED && fn.m_Kind != VALUE_UNSET) { how = "static" + std::to_string(depth + 1); return fn; }
        }
    } catch (...) { how = "lookup-exc"; return RValue(); }
    how = "missing"; return RValue();
}
static bool RefreshItemHash(const RValue& item, std::string* howOut = nullptr)
{
    std::string how;
    // Preferred: the game's own ItemCheckHash(item).  It calls item.GenerateItemHash() with
    // the proper self and STORES the new itemDataHash before comparing, so one call after
    // dressing leaves the item consistent (its bool result is irrelevant here; it never
    // reports by itself - the callers do).  Plain script name, no method plumbing.
    try {
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        if (g) {
            const std::string before = ReadItemHash(item);
            RValue res; AurieStatus st = g_Yytk->CallGameScriptEx(res, "gml_Script_ItemCheckHash", g, g, { item });
            if (AurieSuccess(st) && !ReadItemHash(item).empty()) { if (howOut) *howOut = "itemcheckhash"; return true; }
            how = AurieSuccess(st) ? "itemcheckhash-nohash" : "itemcheckhash-fail";
        }
    } catch (...) { how = "itemcheckhash-exc"; }
    try {
        std::string how2;
        RValue fn = FindStructMethod(item, "GenerateItemHash", how2);
        how += "," + how2;
        if (fn.m_Kind != VALUE_UNDEFINED && fn.m_Kind != VALUE_UNSET) {
            RValue bound = g_Yytk->CallBuiltin("method", { item, fn });   // self = the item
            g_Yytk->CallBuiltin("script_execute", { bound });
            if (howOut) *howOut = how + "+method";
            return true;
        }
    } catch (...) { how += "!exc"; }
    try {
        RValue res;
        CInstance* self = (CInstance*)item.m_Object;
        AurieStatus st = g_Yytk->CallGameScriptEx(res, "gml_Script_GenerateItemHash@anon@4638@s_ItemInstanceStruct@InventoryV2Funcs", self, self, {});
        if (AurieSuccess(st)) { if (howOut) *howOut = how + "+direct"; return true; }
        how += "+direct-fail";
    } catch (...) { how += "+direct-exc"; }
    // Last resort: the compiled routine behind the method, called like a hook trampoline
    // with the item struct as self (the same resolution HookOneScript uses).
    try {
        PVOID p = nullptr;
        if (AurieSuccess(g_Yytk->GetNamedRoutinePointer("gml_Script_GenerateItemHash@anon@4638@s_ItemInstanceStruct@InventoryV2Funcs", &p)) && p) {
            CScript* sc = reinterpret_cast<CScript*>(p);
            PFUNC_YYGMLScript fnp = (sc && sc->m_Functions) ? sc->m_Functions->m_ScriptFunction : nullptr;
            if (fnp) {
                RValue res; CInstance* self = (CInstance*)item.m_Object;
                fnp(self, self, res, 0, nullptr);
                if (!ReadItemHash(item).empty()) { if (howOut) *howOut = how + "+routine"; return true; }
                how += "+routine-nohash";
            } else how += "+routine-nofn";
        } else how += "+routine-notfound";
    } catch (...) { how += "+routine-exc"; }
    if (howOut) *howOut = how;
    return false;
}

// --- Esya dusus sansi (droprate) ve Repository tanimlari ---------------------
// Verified Season 10 repository sizes for the supported build.  Unknown and
// empty categories fail closed.  Do not probe past these bounds:
// GetNormalRepoStruct raises a runner-level array error before C++ can catch it.
static constexpr int kSeason10RepoCounts[] = {
    15, 20, 15, 0, 20, 25, 18, 30, 15, 0,
    60, 27, 44, 65, 74, 200, 156, 0, 16, 7,
};
static constexpr bool RepoIndexValid(int kategori, int indeks)
{
    return kategori >= 0
        && kategori < static_cast<int>(sizeof(kSeason10RepoCounts) / sizeof(kSeason10RepoCounts[0]))
        && indeks >= 0
        && indeks < kSeason10RepoCounts[kategori];
}
static constexpr int kSeason10RelicRepoCount = kSeason10RepoCounts[16];
static_assert(RepoIndexValid(16, 155));
static_assert(!RepoIndexValid(16, 156));
static_assert(RepoIndexValid(15, 199));
static_assert(!RepoIndexValid(15, 200));
static_assert(!RepoIndexValid(3, 0));

static bool RepoStruct(int kategori, int indeks, RValue& out)
{
    if (!RepoIndexValid(kategori, indeks)) return false;
    try {
        out = g_Yytk->CallGameScript("gml_Script_GetNormalRepoStruct",
                                     { RValue((double)kategori), RValue(0.0), RValue((double)indeks) });
        return out.m_Kind == VALUE_OBJECT;
    } catch (...) { return false; }
}

static std::string RepoAd(RValue& st)
{
    try {
        RValue info = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("itemBaseInfoStruct") });
        if (info.m_Kind != VALUE_OBJECT) return "?";
        RValue nm = g_Yytk->CallBuiltin("variable_struct_get", { info, RValue("28") });
        return nm.ToString();
    } catch (...) { return "?"; }
}

static std::map<int, double> g_DropRateVanilya;
static double VanilyaBase(int kategori, int indeks, RValue& st, RValue& drOut)
{
    drOut = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("droprate") });
    if (drOut.m_Kind != VALUE_OBJECT) return -1.0;
    RValue simdi = g_Yytk->CallBuiltin("variable_struct_get", { drOut, RValue("base") });
    int anahtar = kategori * 1000 + indeks;
    auto it = g_DropRateVanilya.find(anahtar);
    if (it == g_DropRateVanilya.end()) {
        // A stale index/offset can hand back garbage (NaN/inf) here; cache -1 so
        // every caller's existing "vanilya <= 0.0 -> skip" guard also rejects it,
        // instead of writing/printing a non-finite droprate.base.
        double v = simdi.ToDouble();
        if (!std::isfinite(v)) v = -1.0;
        g_DropRateVanilya[anahtar] = v;
        return v;
    }
    return it->second;
}

// ---- socket share ----------------------------------------------------------------------
// Socket bonuses reach itemStatStruct only through GenerateItemSpecialStats(item, def),
// which CreateItemNew runs BEFORE this post-process: it AddStat()s the def's damage-type
// table, the rolled random stats and every filled socket's share (def.itemBaseSocketStatStruct
// keyed by the gem's type, rolled from the gem seed).  keep=0 wiped that share, keep=1
// overwrote it on the forged keys ("Skill Haste gem did nothing", 2026-09-07).  Run the
// game's function once more on a scratch stat struct that carries only the socket count,
// with a scratch def that carries only the socket table, and add back what comes out.
// ---- player additions vs dressing (delta accounting) ----------------------------------
// Inside CreateItemNew: CreateItemInit -> GenerateItemRandomStats -> GetRuneword (rune and
// runeword stats) -> GenerateItemSpecialStats (damage types, special tables) -> the socket
// loop (gems, jewels) -> display name -> return.  Dressing therefore runs at the very end
// (the name is written there too), and what the player put into the item is measured on
// the way and added back afterwards: the GetRuneword delta plus everything added after
// GenerateItemSpecialStats.  GenerateItemSpecialStats' own additions belong to the base
// item, so keep=0 drops them like any other native stat.  (Dressing right after
// GenerateItemSpecialStats, tried 2026-09-07, kept gems but lost runes and the custom name.)
struct ForgeDeltas
{
    std::map<std::string, double> rune;          // GetRuneword: after - before
    std::map<std::string, double> afterSpecial;  // snapshot right after GenerateItemSpecialStats
    bool haveAfterSpecial = false;
};
static std::unordered_map<YYObjectBase*, ForgeDeltas> g_ForgeDeltas;
// Selector index of the loaded forge entries ("t|a|b"): an item whose type / seed / base is
// not in it cannot be dressed, so its runeword / special-stat snapshots are skipped.
static std::unordered_set<std::string> g_ForgeSelectorKeys;
static bool g_ForgeSelectorIndexLoose = true;   // an entry without t/a/b -> check everything
static void RebuildForgeSelectorIndex()
{
    g_ForgeSelectorKeys.clear(); g_ForgeSelectorIndexLoose = false;
    for (const CustomForgeEntry& e : g_CustomForgeEntries) {
        auto it = e.selector.find("t"), ia = e.selector.find("a"), ib = e.selector.find("b");
        if (it == e.selector.end() || ia == e.selector.end() || ib == e.selector.end()) { g_ForgeSelectorIndexLoose = true; continue; }
        g_ForgeSelectorKeys.insert(std::to_string((long long)it->second) + "|" + std::to_string((long long)ia->second) + "|" + std::to_string((long long)ib->second));
    }
}
static bool ForgeCouldMatch(const RValue& item)
{
    if (g_ForgeSelectorIndexLoose) return true;
    if (g_ForgeSelectorKeys.empty()) return false;
    try {
        double t = 0, a = 0, b = 0;
        if (!TryStructNumber(item, "itemType", t)) return false;
        RValue def = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemDefinitionStruct") });
        if (def.m_Kind != VALUE_OBJECT || !TryStructNumber(def, "a", a) || !TryStructNumber(def, "b", b)) return false;
        return g_ForgeSelectorKeys.count(std::to_string((long long)t) + "|" + std::to_string((long long)a) + "|" + std::to_string((long long)b)) > 0;
    } catch (...) { return true; }
}
static volatile LONG g_ForgeAdditionsAdded = 0;
static bool FdIsNumber(const RValue& v) { return v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64; }
static std::map<std::string, double> StructNumbers(const RValue& st)
{
    std::map<std::string, double> out;
    try {
        if (st.m_Kind != VALUE_OBJECT) return out;
        RValue names = g_Yytk->CallBuiltin("variable_struct_get_names", { st });
        if (names.m_Kind != VALUE_ARRAY) return out;
        const int count = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
        for (int i = 0; i < count; ++i) {
            RValue name = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            RValue v = g_Yytk->CallBuiltin("variable_struct_get", { st, name });
            if (FdIsNumber(v)) out[name.ToString()] = v.ToDouble();
        }
    } catch (...) {}
    return out;
}
static RValue ItemStatsOf(const RValue& item)
{
    try { return g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemStatStruct") }); } catch (...) { return RValue(); }
}
static PFUNC_YYGMLScript g_Orig_GenerateItemSpecialStats = nullptr, g_Orig_GetRuneword = nullptr;
static RValue& Hook_GenerateItemSpecialStats(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    RValue& r = g_Orig_GenerateItemSpecialStats ? g_Orig_GenerateItemSpecialStats(S, O, R, argc, A) : R;
    PERF_SCOPE(g_PerfDeltas);
    try {
        if (argc >= 1 && A && A[0] && A[0]->m_Kind == VALUE_OBJECT && ForgeCouldMatch(*A[0])) {
            RValue st = ItemStatsOf(*A[0]);
            if (st.m_Kind == VALUE_OBJECT) {
                if (g_ForgeDeltas.size() > 4096) g_ForgeDeltas.clear();
                ForgeDeltas& d = g_ForgeDeltas[A[0]->m_Object];
                d.afterSpecial = StructNumbers(st);
                d.haveAfterSpecial = true;
            }
        }
    } catch (...) {}
    return r;
}
static RValue& Hook_GetRuneword(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    RValue* itemP = (argc >= 1 && A && A[0] && A[0]->m_Kind == VALUE_OBJECT && ForgeCouldMatch(*A[0])) ? A[0] : nullptr;
    PERF_SCOPE(g_PerfDeltas);
    std::map<std::string, double> before; bool track = false;
    if (itemP) { try { RValue st = ItemStatsOf(*itemP); if (st.m_Kind == VALUE_OBJECT) { before = StructNumbers(st); track = true; } } catch (...) { track = false; } }
    RValue& r = g_Orig_GetRuneword ? g_Orig_GetRuneword(S, O, R, argc, A) : R;
    if (track) {
        try {
            std::map<std::string, double> after = StructNumbers(ItemStatsOf(*itemP));
            if (g_ForgeDeltas.size() > 4096) g_ForgeDeltas.clear();
            ForgeDeltas& d = g_ForgeDeltas[itemP->m_Object];
            for (const auto& kv : after) {
                auto b = before.find(kv.first);
                const double dd = kv.second - (b == before.end() ? 0.0 : b->second);
                if (std::fabs(dd) > 1e-9) d.rune[kv.first] += dd;
            }
        } catch (...) {}
    }
    return r;
}
// The final dressing pass takes everything the player added (runes, and whatever came
// after GenerateItemSpecialStats: gems, jewels) to put it back on top of the forged stats.
static std::map<std::string, double> TakeForgeAdditions(const RValue& item, const std::map<std::string, double>& atEntry, size_t* runeKeys, size_t* socketKeys)
{
    std::map<std::string, double> add;
    auto it = g_ForgeDeltas.find(item.m_Object);
    if (it == g_ForgeDeltas.end()) return add;
    add = it->second.rune;
    if (runeKeys) *runeKeys = add.size();
    size_t sk = 0;
    if (it->second.haveAfterSpecial) {
        for (const auto& kv : atEntry) {
            auto b = it->second.afterSpecial.find(kv.first);
            const double dd = kv.second - (b == it->second.afterSpecial.end() ? 0.0 : b->second);
            if (std::fabs(dd) > 1e-9) { add[kv.first] += dd; ++sk; }
        }
    }
    if (socketKeys) *socketKeys = sk;
    g_ForgeDeltas.erase(it);
    return add;
}

static volatile LONG g_CustomForgeHashMisses = 0;
static bool TryApplyCustomForge(RValue* candidate, bool finalPass = false)
{
    if (!candidate || candidate->m_Kind != VALUE_OBJECT || g_CustomForgeEntries.empty())
        return false;
    try {
        RValue hasDefinition = g_Yytk->CallBuiltin(
            "variable_struct_exists", { *candidate, RValue("itemDefinitionStruct") });
        RValue hasStats = g_Yytk->CallBuiltin(
            "variable_struct_exists", { *candidate, RValue("itemStatStruct") });
        if (!hasDefinition.ToBoolean() || !hasStats.ToBoolean()) return false;
        RValue definition = g_Yytk->CallBuiltin(
            "variable_struct_get", { *candidate, RValue("itemDefinitionStruct") });
        RValue stats = g_Yytk->CallBuiltin(
            "variable_struct_get", { *candidate, RValue("itemStatStruct") });
        if (definition.m_Kind != VALUE_OBJECT || stats.m_Kind != VALUE_OBJECT) return false;
        std::map<std::string, double> statsAtEntry;
        if (finalPass) statsAtEntry = StructNumbers(stats);
        {   // record the stat struct once, before any forge entry touches it: the editor wants the item's own values
            RValue recorded = g_Yytk->CallBuiltin("variable_struct_exists", { *candidate, RValue("fp_recorded") });
            if (!recorded.ToBoolean()) { RecordItemStats(*candidate, stats); g_Yytk->CallBuiltin("variable_struct_set", { *candidate, RValue("fp_recorded"), RValue(true) }); }
        }

        for (const CustomForgeEntry& entry : g_CustomForgeEntries) {
            if (!CustomForgeMatches(entry, *candidate, definition)) continue;
            if (!entry.keepNative) {
                RValue names = g_Yytk->CallBuiltin("variable_struct_get_names", { stats });
                if (names.m_Kind == VALUE_ARRAY) {
                    int count = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
                    for (int i = 0; i < count; ++i) {
                        RValue name = g_Yytk->CallBuiltin(
                            "array_get", { names, RValue((double)i) });
                        g_Yytk->CallBuiltin("variable_struct_remove", { stats, name });
                    }
                }
                // keep=0: the rolled affix names describe rows that no longer exist
                // ("Armorer's " = Enhanced Defense, " of the Viper" = Poison Res), so
                // blank ["5"] (prefix) and ["4"] (suffix) as well.  A custom name=
                // rewrites ["28"] later in this pass; ["28"] itself is left alone here
                // because for a plain base it is the localized base name.
                RValue hasInfo0 = g_Yytk->CallBuiltin(
                    "variable_struct_exists", { *candidate, RValue("itemInfoStruct") });
                if (hasInfo0.ToBoolean()) {
                    RValue info0 = g_Yytk->CallBuiltin("variable_struct_get", { *candidate, RValue("itemInfoStruct") });
                    if (info0.m_Kind == VALUE_OBJECT) {
                        for (const char* affixField : { "4", "5" }) {
                            RValue hasAffix = g_Yytk->CallBuiltin("variable_struct_exists", { info0, RValue(affixField) });
                            if (hasAffix.ToBoolean())
                                g_Yytk->CallBuiltin("variable_struct_set", { info0, RValue(affixField), RValue("") });
                        }
                    }
                }
            }
            for (const auto& stat : entry.stats) {
                g_Yytk->CallBuiltin("variable_struct_set", {
                    stats, RValue(std::to_string(stat.first)), RValue(stat.second)
                });
            }
            if (entry.rarity >= 0 || entry.tier >= 0 || !entry.lore.empty() || !entry.name.empty()) {
                RValue hasInfo = g_Yytk->CallBuiltin(
                    "variable_struct_exists", { *candidate, RValue("itemInfoStruct") });
                RValue info = hasInfo.ToBoolean()
                    ? g_Yytk->CallBuiltin("variable_struct_get", { *candidate, RValue("itemInfoStruct") })
                    : RValue();
                if (info.m_Kind == VALUE_OBJECT) {
                    if (entry.rarity >= 0)
                        g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("27"), RValue((double)entry.rarity) });
                    if (entry.tier >= 0)
                        g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("32"), RValue((double)entry.tier) });
                    if (!entry.name.empty()) {
                        // ["28"] holds the finished display text once CreateItemNew
                        // returns (GenerateItemRandomStats still sees the CSV key);
                        // this post-process runs after both, so the last write wins.
                        // ["5"] = magic prefix ("Eagle "), ["4"] = suffix (" of Energy").
                        g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("28"), RValue(entry.name) });
                        for (const char* affixField : { "4", "5" }) {
                            RValue hasAffix = g_Yytk->CallBuiltin("variable_struct_exists", { info, RValue(affixField) });
                            if (hasAffix.ToBoolean())
                                g_Yytk->CallBuiltin("variable_struct_set", { info, RValue(affixField), RValue("") });
                        }
                        // Safety net if a draw path resolves the name through
                        // GetLocalized: the text maps to itself.
                        RValue map = g_Yytk->CallBuiltin("variable_global_get", { RValue("localization") });
                        if (map.m_Kind == VALUE_REAL || map.m_Kind == VALUE_INT32 || map.m_Kind == VALUE_INT64 || map.m_Kind == VALUE_REF)
                            g_Yytk->CallBuiltin("ds_map_replace", { map, RValue(entry.name), RValue(entry.name) });
                    }
                    if (!entry.lore.empty()) {
                        // One private localization key per configured item; the
                        // game draws the description with GetLocalized(info["29"])
                        // straight out of global.localization (a ds_map), so the
                        // text is registered there and never touches the CSVs.
                        std::string key = "lore_forge";
                        for (const auto& pair : entry.selector)
                            key += "_" + pair.first + std::to_string((long long)pair.second);
                        RValue map = g_Yytk->CallBuiltin("variable_global_get", { RValue("localization") });
                        if (map.m_Kind == VALUE_REAL || map.m_Kind == VALUE_INT32 || map.m_Kind == VALUE_INT64 || map.m_Kind == VALUE_REF) {
                            g_Yytk->CallBuiltin("ds_map_replace", { map, RValue(key), RValue(entry.lore) });
                            g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("29"), RValue(key) });
                        }
                    }
                }
            }
            if (!entry.affix.empty())
                g_Yytk->CallBuiltin("variable_struct_set", { *candidate, RValue("fp_affix"), RValue(entry.affix) });
            if (!entry.mechanic.empty()) {
                g_Yytk->CallBuiltin("variable_struct_set", { *candidate, RValue("fp_mechanic"), RValue(entry.mechanic) });
                InterlockedIncrement(&g_CustomForgeMechanicTags);
                if (entry.mechanic == "tyrant") g_TyItemTagged = true;
                if (entry.mechanic == "beacon") g_BeItemTagged = true;
            }
            if (finalPass) {
                size_t rk = 0, sk = 0;
                std::map<std::string, double> add = TakeForgeAdditions(*candidate, statsAtEntry, &rk, &sk);
                int applied = 0;
                for (const auto& kv : add) {
                    int key = -1; try { key = std::stoi(kv.first); } catch (...) { continue; }
                    if (entry.keepNative && !entry.stats.count(key)) continue;   // native keys still carry it
                    double cur = 0.0; TryStructNumber(stats, kv.first.c_str(), cur);
                    g_Yytk->CallBuiltin("variable_struct_set", { stats, RValue(kv.first), RValue(cur + kv.second) });
                    ++applied;
                }
                InterlockedExchangeAdd(&g_ForgeAdditionsAdded, applied);
#ifndef FORGEPACT_RELEASE
                static int s_AddLogs = 0;
                if ((rk || sk) && s_AddLogs < 20) { ++s_AddLogs; Out("forge additions: rune keys " + std::to_string(rk) + ", socket keys " + std::to_string(sk) + ", applied " + std::to_string(applied)); }
#endif
            }
            {
                const std::string before = ReadItemHash(*candidate);
                std::string how;
                const bool ok = RefreshItemHash(*candidate, &how);
                const std::string after = ReadItemHash(*candidate);
#ifndef FORGEPACT_RELEASE
                static LONG s_HashLogs = 0;
                if (!ok || after.empty() || after == before || InterlockedIncrement(&s_HashLogs) <= 24)
                    Out("forge hash: " + before.substr(0, 8) + " -> " + (after.empty() ? std::string("(none)") : after.substr(0, 8)) + " via " + how + (ok ? "" : " (FAILED)"));
#endif
                if (!ok) InterlockedIncrement(&g_CustomForgeHashMisses);
            }
            RememberForgedItem(*candidate);
            // This counter is part of the release runtime contract, not merely
            // development diagnostics. Persist the first confirmed match so
            // the editor can distinguish an installed hook from an item that
            // was actually modified by it.
            if (InterlockedIncrement(&g_CustomForgeApplyCount) == 1)
                WriteCustomForgeStatus("runtime stats applied");
            return true;
        }
    } catch (...) {}
    return false;
}

// Relic filter (RelicFilterManager, g_FilterMaxRelics, g_RelicFilterPending,
// and the GetPlayerMaxedRelics wrapper) moved to ForgePact::RelicFilterMod
// (module includes anchor near the top of the file, after FirstToken).
// HhResolveLocalPlayer's `how = nullptr` default is declared there now, not
// here - a default argument may only be specified once per translation unit.

static void CustomForgePostProcess(RValue& result, int argc, RValue** args, bool finalPass)
{
    PERF_SCOPE(g_PerfForge);
    if (TryApplyCustomForge(&result, finalPass)) return;
    // Some constructors mutate an argument and return undefined. Scan only a
    // small, bounded prefix; TryApply requires all three canonical item fields.
    for (int i = 0; i < argc && i < 8; ++i)
        if (args && TryApplyCustomForge(args[i], finalPass)) return;
}


// ===== Headhunter mechanic ==================================================
//
// Path-of-Exile style belt: when the player kills a rare/champion monster the
// monster's affixes are translated into timed player buffs. Later native
// captures use enemy self and a killer hint in argument 2; earlier notes used
// the reverse shape. Resolve the roles by object type instead of assuming
// that either shape applies to every build. The enemy carries `affixList`
// (array) and `enemyRarity`; BuffAdd(playerIdx, buffId, [v0,v1], frames, ...)
// creates one Draw_Player_Buff_obj per buff id and stores it in
// global.playerBuff[playerIdx][0][buffId].  The belt is recognised by the
// fp_mechanic tag TryApplyCustomForge puts on forged items whose sidecar entry
// carries mechanic=headhunter; equipment is read through the game's own
// GetSlot() (needs an instance self, so it is called from inside the hook).
struct HhBuff { int64_t id; double v0; double v1; };
static std::atomic<bool> g_HhEnabled{ false };
static std::atomic<bool> g_HhForced{ false };          // "headhunter force": ignore the belt check (testing)
static double g_HhDurationSec = 20.0;
static bool g_HhTrace = false;
// enemyAffix index -> affix key, in the game's own order (translationsEnemy.csv [Affixes];
// live-confirmed 2026-09-04: index 21 = Fallen Angel on a rarity-3 rare).
static const char* const kHhAffixNames[] = {
    // 0..21 confirmed live (hhscan pairs): CSV order.
    "champion", "fractal", "raging", "enraged", "haunted", "vampiric", "burstshot", "possessed",
    "extrafast", "extrastrong", "stoneskin", "coldenchanted", "fireenchanted", "lightningenchanted",
    "magicresistant", "manaburn", "multishot", "treasuregobbler", "arcanascurse", "venomous",
    "punisher", "fallenangel",
    // 22..24: three of Commander / Guardian of Hell / Bloating / Sharpshooter (one CSV entry is
    // absent at runtime: live 25 = Pyromaniac = CSV 26).  Unconfirmed order.
    "commander", "guardianofhell", "bloating",
    // 25 Pyromaniac confirmed live; 26 Berserker inferred (CSV 27 - 1).
    "pyromaniac", "berserker",
    // 27..29: one more CSV entry missing before 30 (live 30 = Thick Skin = CSV 32).  Unconfirmed.
    "sharpshooter", "shielding", "fearless",
    // 30 Thick Skin, 31 Antimagus confirmed live; 36 Blazing, 38 Meteoric confirmed live.
    "thickskin", "antimagus", "colossal", "stealthy", "timelapsing", "wasped", "blazing",
    "thundercaller", "meteoric"
};
static std::string HhAffixName(int idx)
{
    if (idx >= 0 && idx < (int)(sizeof(kHhAffixNames) / sizeof(kHhAffixNames[0]))) return kHhAffixNames[idx];
    return "";   // slots past the affix list (e.g. 45 = Shadow Realm zone flag) are not affixes
}
// affix key -> buff.  Keys are the affix names above (or "#<index>" for unknown slots).
// Buff ids measured live 2026-09-04 with the game's own BuffAdd: 44 = movement speed
// (Burst of Speed), 177 = attack speed, 178 = faster cast rate, 144 = life replenish.
// Everything else falls back to g_HhDefault until its id is measured.  `hhmap` overrides.
static std::map<std::string, HhBuff> g_HhMap = {
    // Buff ids measured live 2026-09-04 (distinct-value probe, value = the [v0,v1] passed to BuffAdd):
    //  44 movement speed (x4)   177 attack speed (x1.25)   178 faster cast rate   144 life replenish + life/kill
    //  27 fire skill damage     71 lightning skill damage  85 arcane skill damage 20 mana replenish + arcane
    //   2 phys+magic damage reduction (cap 75)   297 defense (flat)   10 max life + max mana   82 dodge (cap 90)
    //  67 magic find (x100 in the stat array)  309 experience gain
    { "extrafast",          { 44,   25.0,  25.0 } },   // +100 movement
    { "raging",             { 177,  40.0,  40.0 } },   // +50 attack speed
    { "enraged",            { 177,  40.0,  40.0 } },
    { "berserker",          { 177,  60.0,  60.0 } },
    { "extrastrong",        { 177,  40.0,  40.0 } },   // no plain damage buff id found yet; attack speed stands in
    { "vampiric",           { 144,  50.0,  50.0 } },
    { "fireenchanted",      { 27,   40.0,  40.0 } },
    { "pyromaniac",         { 27,   40.0,  40.0 } },
    { "blazing",            { 27,   40.0,  40.0 } },
    { "meteoric",           { 27,   40.0,  40.0 } },
    { "lightningenchanted", { 71,   40.0,  40.0 } },
    { "thundercaller",      { 71,   40.0,  40.0 } },
    { "coldenchanted",      { 178,  40.0,  40.0 } },   // no cold-damage buff id found yet; cast rate stands in
    { "arcanascurse",       { 85,   40.0,  40.0 } },
    { "manaburn",           { 20,   40.0,  40.0 } },
    { "stoneskin",          { 2,    25.0,  25.0 } },
    { "thickskin",          { 2,    25.0,  25.0 } },
    { "magicresistant",     { 2,    25.0,  25.0 } },
    { "antimagus",          { 2,    25.0,  25.0 } },
    { "shielding",          { 297, 150.0, 150.0 } },
    { "fearless",           { 297, 150.0, 150.0 } },
    { "divine",             { 297, 150.0, 150.0 } },
    { "colossal",           { 10,  200.0, 200.0 } },
    { "champion",           { 10,  150.0, 150.0 } },
    { "commander",          { 10,  150.0, 150.0 } },
    { "guardianofhell",     { 10,  200.0, 200.0 } },
    { "stealthy",           { 82,   20.0,  20.0 } },
    { "timelapsing",        { 82,   20.0,  20.0 } },
    { "wasped",             { 82,   20.0,  20.0 } },
    { "treasuregobbler",    { 67,   50.0,  50.0 } },
    { "fractal",            { 309,  50.0,  50.0 } },   // experience gain
    { "possessed",          { 85,   40.0,  40.0 } },
    { "haunted",            { 82,   20.0,  20.0 } },
    { "venomous",           { 144,  50.0,  50.0 } },
    { "punisher",           { 177,  40.0,  40.0 } },
    { "sharpshooter",       { 177,  40.0,  40.0 } },
    { "multishot",          { 177,  40.0,  40.0 } },
    { "burstshot",          { 177,  40.0,  40.0 } },
    { "bloating",           { 10,  150.0, 150.0 } },
    { "fallenangel",        { 297, 150.0, 150.0 } },
};
static bool g_HhDefaultOn = true;                       // unmapped affix -> default buff
static HhBuff g_HhDefault{ 44, 25.0, 25.0 };          // id 44 = the game's Burst of Speed buff (visible test buff)
static volatile long g_HhKills = 0, g_HhRareKills = 0, g_HhBuffsApplied = 0, g_HhSkippedNotEquipped = 0;
// Diagnostics for "the mechanic does nothing" reports: how often the game's kill script ran
// and with how many arguments (HhOnKill needs three, the third being the killer).
static volatile long g_HhHookCalls = 0; static volatile long g_HhLastArgc = -1;
static volatile long g_HhRarityKills = 0;   // kills with enemyRarity >= 2 (rare/champion by the game's own flag)
static volatile long g_HhAffixKills = 0;    // kills where affix data (affixList or enemyAffix flags) was present
static PFUNC_YYGMLScript g_Orig_EnemyDestroyKillProc = nullptr;
static bool g_HhHookInstalled = false;
static bool g_HhEquippedCache = false;
static unsigned long long g_HhEquippedStamp = 0;
static unsigned long long g_HhFrame = 0;
static std::string g_HhLastShape;

static std::string HhDescribeList(const RValue& arr)
{
    std::string s = "[";
    try {
        int n = (int)g_Yytk->CallBuiltin("array_length", { arr }).ToDouble();
        for (int i = 0; i < n && i < 16; ++i) {
            RValue e = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)i) });
            if (i) s += ", ";
            s += Describe(e);
        }
        if (n > 16) s += ", ...";
    } catch (...) { s += "?"; }
    return s + "]";
}

static std::string HhKeyOf(const RValue& v)
{
    if (v.m_Kind == VALUE_STRING) return Lower(v.ToString());
    if (v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64 || v.m_Kind == VALUE_BOOL)
        return "#" + std::to_string((long long)v.ToDouble());
    return "";
}

// Depth-limited search for an item struct tagged fp_mechanic == "headhunter".
static bool HhFindTagged(const RValue& node, int depth)
{
    if (depth > 4) return false;
    try {
        if (node.m_Kind == VALUE_ARRAY) {
            int n = (int)g_Yytk->CallBuiltin("array_length", { node }).ToDouble();
            if (n > 64) n = 64;
            for (int i = 0; i < n; ++i) {
                RValue e = g_Yytk->CallBuiltin("array_get", { node, RValue((double)i) });
                if (HhFindTagged(e, depth + 1)) return true;
            }
            return false;
        }
        if (node.m_Kind == VALUE_OBJECT) {
            RValue has = g_Yytk->CallBuiltin("variable_struct_exists", { node, RValue("fp_mechanic") });
            if (has.ToBoolean()) {
                RValue m = g_Yytk->CallBuiltin("variable_struct_get", { node, RValue("fp_mechanic") });
                return Lower(m.ToString()) == "headhunter";
            }
            // an untagged item struct is a leaf; only descend into non-item structs
            RValue isItem = g_Yytk->CallBuiltin("variable_struct_exists", { node, RValue("itemDefinitionStruct") });
            if (isItem.ToBoolean()) return false;
            RValue names = g_Yytk->CallBuiltin("variable_struct_get_names", { node });
            int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
            if (n > 64) n = 64;
            for (int i = 0; i < n; ++i) {
                RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
                RValue e = g_Yytk->CallBuiltin("variable_struct_get", { node, nm });
                if (HhFindTagged(e, depth + 1)) return true;
            }
        }
    } catch (...) {}
    return false;
}

// Is a Headhunter belt equipped?  The runtime item struct carries
// itemEquippedPlayer (read by RunItemEquipped): the index of the player wearing
// it, or a negative value when it sits in a bag/stash.  Registry entries whose
// struct died are skipped (every read is guarded).  Re-evaluated every 30 frames.
static double HhReadNumber(const RValue& item, const char* field, double fallback)
{
    try {
        RValue has = g_Yytk->CallBuiltin("variable_struct_exists", { item, RValue(field) });
        if (!has.ToBoolean()) return fallback;
        RValue v = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue(field) });
        if (v.m_Kind == VALUE_BOOL) return v.ToBoolean() ? 1.0 : 0.0;
        if (v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64) return v.ToDouble();
    } catch (...) {}
    return fallback;
}

static bool HhItemIsHeadhunter(const RValue& item)
{
    try {
        if (item.m_Kind != VALUE_OBJECT || !item.m_Object) return false;
        RValue has = g_Yytk->CallBuiltin("variable_struct_exists", { item, RValue("fp_mechanic") });
        if (!has.ToBoolean()) return false;
        RValue m = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("fp_mechanic") });
        return Lower(m.ToString()) == "headhunter";
    } catch (...) { return false; }
}

// ---- worn signature items --------------------------------------------------------------
// The game keeps the local player's worn gear in the instance struct `equippedItems`
// (ItemEquip / ItemUnequip / RunItemEquipped read and write it; ItemUnequip also clears the
// item's itemEquippedPlayer).  Every 30 frames the struct is walked and each forged item
// carrying fp_mechanic marks that mechanic as worn.  No registry pointers are dereferenced.
static bool HhResolveLocalPlayer(RValue& out, std::string* how);   // defined below
static std::set<std::string> g_WornMechanics;
static uint32_t g_WornStamp = 0;
static std::string g_WornShape = "not read yet";
static void WornCollect(const RValue& item)
{
    try {
        if (item.m_Kind != VALUE_OBJECT || !item.m_Object) return;
        RValue has = g_Yytk->CallBuiltin("variable_struct_exists", { item, RValue("fp_mechanic") });
        if (!has.ToBoolean()) return;
        RValue m = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("fp_mechanic") });
        if (m.m_Kind == VALUE_STRING) g_WornMechanics.insert(Lower(m.ToString()));
    } catch (...) {}
}
static RValue ForgeRegistryStruct(bool create);
static std::string g_WornDetail;
static void RefreshWornMechanics(bool force = false)
{
    if (!force && g_WornStamp != 0 && g_HhFrame - g_WornStamp < 30) return;
    g_WornStamp = g_HhFrame ? g_HhFrame : 1;
    g_WornMechanics.clear(); g_WornDetail.clear();
    try {
        RValue reg = ForgeRegistryStruct(false);
        if (reg.m_Kind != VALUE_OBJECT) { g_WornShape = "no registry yet"; return; }
        RValue names = g_Yytk->CallBuiltin("variable_struct_get_names", { reg });
        const int n = names.m_Kind == VALUE_ARRAY ? (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble() : 0;
        int worn = 0;
        for (int i = 0; i < n; ++i) {
            RValue name = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            RValue item = g_Yytk->CallBuiltin("variable_struct_get", { reg, name });
            if (item.m_Kind != VALUE_OBJECT || !item.m_Object) continue;
            RValue hasM = g_Yytk->CallBuiltin("variable_struct_exists", { item, RValue("fp_mechanic") });
            if (!hasM.ToBoolean()) continue;
            RValue m = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("fp_mechanic") });
            if (m.m_Kind != VALUE_STRING) continue;
            const std::string mech = Lower(m.ToString());
            // ItemEquip writes the wearer's player index here, ItemUnequip clears it.
            double who = -1.0;
            RValue hasE = g_Yytk->CallBuiltin("variable_struct_exists", { item, RValue("itemEquippedPlayer") });
            if (hasE.ToBoolean()) {
                RValue e = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemEquippedPlayer") });
                if (e.m_Kind == VALUE_REAL || e.m_Kind == VALUE_INT32 || e.m_Kind == VALUE_INT64) who = e.ToDouble();
            }
            const bool isWorn = who >= 0.0;
            if (isWorn) { g_WornMechanics.insert(mech); ++worn; }
            if (g_WornDetail.size() < 600) g_WornDetail += mech + "=" + (hasE.ToBoolean() ? std::to_string((long long)who) : std::string("unset")) + " ";
        }
        g_WornShape = "registry " + std::to_string(n) + " items, " + std::to_string(worn) + " worn";
    } catch (...) { g_WornShape = "exception"; }
}
static bool MechanicWorn(const char* mechanic)
{
    RefreshWornMechanics();
    return g_WornMechanics.count(mechanic) > 0;
}

static bool HhEquipped(CInstance* player)
{
    (void)player;
    if (g_HhForced.load()) return true;
    if (MechanicWorn("headhunter")) { g_HhEquippedCache = true; return true; }
    if (g_HhEquippedStamp != 0 && g_HhFrame - g_HhEquippedStamp < 30) return g_HhEquippedCache;
    g_HhEquippedStamp = g_HhFrame;
    bool found = false;
#ifndef FORGEPACT_RELEASE
    // Research build only: the registry holds raw struct pointers that the GC
    // may reclaim, so dereferencing them is not acceptable in a player build
    // until equipped-state detection is finished (planned for 1.3.8).
    // The item struct carries no equipped flag (live 2026-09-04), so while the
    // real check is pending a Headhunter item that was CREATED for this character
    // counts as active.  This makes the research build independent of the panel's
    // "headhunter force" command.
    for (const RValue& item : g_ForgedItems) {
        if (!HhItemIsHeadhunter(item)) continue;
        double who = HhReadNumber(item, "itemEquippedPlayer", -1.0);
        if (g_HhTrace && g_HhLastShape.empty()) { g_HhLastShape = "seen"; Out("hh: headhunter item present (itemEquippedPlayer=" + std::to_string(who) + "), treating as equipped"); }
        found = true; break;
    }
#endif
    g_HhEquippedCache = found;
    return found;
}

// ===== Headhunter head labels ===================================================
// Every stolen affix is remembered with its expiry (wall clock, current_time ms) and
// drawn above the player's head right after the game's own Player_obj Draw event, in
// the affix-row gold with a 1 px dark outline: "Extra Fast 12s   Vampiric 12s".
struct HhStolen { std::string name; double expiryMs; int64_t buffId; double bornMs; };
static std::vector<HhStolen> g_HhStolen;
static std::atomic<bool> g_HhLabelOn{ true };
static size_t g_HhLabelMax = 12;           // oldest label drops when more affixes are active
static std::string g_HhLabelFont;          // font asset name override ("" = current font)
static int g_HhLabelPlayerId = -1;         // instance id of the player who received the buffs
static bool g_HhObjectCallbackInstalled = false;
static const std::pair<const char*, const char*> kHhAffixDisplay[] = {
    {"antimagus","Antimagus"}, {"arcanascurse","Arcana's Curse"}, {"berserker","Berserker"}, {"blazing","Blazing"},
    {"bloating","Bloating"}, {"burstshot","Burst Shot"}, {"champion","Champion"}, {"coldenchanted","Cold Enchanted"},
    {"colossal","Colossal"}, {"commander","Commander"}, {"divine","Divine"}, {"enraged","Enraged"},
    {"extrafast","Extra Fast"}, {"extrastrong","Extra Strong"}, {"fallenangel","Fallen Angel"}, {"fearless","Fearless"},
    {"fireenchanted","Fire Enchanted"}, {"fractal","Fractal"}, {"guardianofhell","Guardian of Hell"}, {"haunted","Haunted"},
    {"lightningenchanted","Lightning Enchanted"}, {"magicresistant","Magic Resistant"}, {"manaburn","Manaburn"},
    {"meteoric","Meteoric"}, {"multishot","Multishot"}, {"possessed","Possessed"}, {"punisher","Punisher"},
    {"pyromaniac","Pyromaniac"}, {"raging","Raging"}, {"sharpshooter","Sharpshooter"}, {"shielding","Shielding"},
    {"stealthy","Stealthy"}, {"stoneskin","Stoneskin"}, {"thickskin","Thick Skin"}, {"thundercaller","Thunder Caller"},
    {"timelapsing","Time Lapsing"}, {"treasuregobbler","Treasure Gobbler"}, {"vampiric","Vampiric"},
    {"venomous","Venomous"}, {"wasped","Wasped"},
};
static std::string HhDisplayName(const std::string& key)
{
    for (const auto& e : kHhAffixDisplay) if (key == e.first) return e.second;
    std::string s = key; if (!s.empty()) s[0] = (char)std::toupper((unsigned char)s[0]);
    return s;
}
static double HhNowMs()
{
    using namespace std::chrono;
    return (double)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
// Is the buff still on the local player?  global.playerBuff[1][0][buffId] holds the
// Draw_Player_Buff_obj instance (or -4).  Unknown layout -> assume alive.
static bool HhBuffAlive(int64_t buffId)
{
    try {
        RValue pb = g_Yytk->CallBuiltin("variable_global_get", { RValue("playerBuff") });
        if (pb.m_Kind != VALUE_ARRAY) return true;
        RValue a1 = g_Yytk->CallBuiltin("array_get", { pb, RValue(1.0) });
        if (a1.m_Kind != VALUE_ARRAY) return true;
        RValue a0 = g_Yytk->CallBuiltin("array_get", { a1, RValue(0.0) });
        if (a0.m_Kind != VALUE_ARRAY) return true;
        RValue len = g_Yytk->CallBuiltin("array_length", { a0 });
        if ((double)buffId >= len.ToDouble()) return true;
        RValue ref = g_Yytk->CallBuiltin("array_get", { a0, RValue((double)buffId) });
        if (ref.m_Kind == VALUE_UNDEFINED) return false;
        if ((ref.m_Kind == VALUE_REAL || ref.m_Kind == VALUE_INT32 || ref.m_Kind == VALUE_INT64) && ref.ToDouble() < 0) return false;
        RValue ex = g_Yytk->CallBuiltin("instance_exists", { ref });
        return ex.ToBoolean();
    } catch (...) { return true; }
}
static void HhRememberStolen(CInstance* player, const std::string& key, double seconds, int64_t buffId)
{
    try {
        if (player) {
            RValue pid = g_Yytk->CallBuiltin("variable_instance_get", { player->ToRValue(), RValue("id") });
            g_HhLabelPlayerId = (int)pid.ToDouble();
        }
        const double expiry = HhNowMs() + seconds * 1000.0;
        const std::string name = HhDisplayName(key);
        for (auto& s : g_HhStolen) if (s.name == name) { s.expiryMs = expiry; s.buffId = buffId; s.bornMs = HhNowMs(); return; }
        while (g_HhStolen.size() >= g_HhLabelMax && !g_HhStolen.empty()) g_HhStolen.erase(g_HhStolen.begin());
        g_HhStolen.push_back({ name, expiry, buffId, HhNowMs() });
    } catch (...) {}
}
static void HhDrawOutlinedWorld(double x, double y, const std::string& text, const RValue& colour)
{
    g_Yytk->CallBuiltin("draw_set_colour", { RValue(0.0) });
    for (double dx = -1; dx <= 1; dx += 2) g_Yytk->CallBuiltin("draw_text", { RValue(x + dx), RValue(y), RValue(text) });
    for (double dy = -1; dy <= 1; dy += 2) g_Yytk->CallBuiltin("draw_text", { RValue(x), RValue(y + dy), RValue(text) });
    g_Yytk->CallBuiltin("draw_set_colour", { colour });
    g_Yytk->CallBuiltin("draw_text", { RValue(x), RValue(y), RValue(text) });
}
static double g_HhLabelOffsetPx = 150.0;  // GUI pixels above the player's bounding box top (tuned live 2026-09-05)
// The local player as an RValue usable with variable_instance_get: the game's own
// GetMyPlayer() first, then the first Player_obj instance.  Returns false if none.
// Is this RValue something the callers can actually read variables from?
// Proved by doing exactly what they will do, rather than by trusting a kind tag.
static bool HhUsableInstance(const RValue& v)
{
    if (v.m_Kind != VALUE_OBJECT && v.m_Kind != VALUE_REF) return false;
    try {
        RValue x = g_Yytk->CallBuiltin("variable_instance_get", { v, RValue("x") });
        return x.m_Kind == VALUE_REAL || x.m_Kind == VALUE_INT32 || x.m_Kind == VALUE_INT64;
    } catch (...) { return false; }
}

static bool HhResolveLocalPlayer(RValue& out, std::string* how)
{
    try {
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        RValue p;
        AurieStatus st = g_Yytk->CallGameScriptEx(p, "gml_Script_GetMyPlayer", g, g, {});
        if (AurieSuccess(st) && HhUsableInstance(p)) {
            out = p;
            if (how) *how = "GetMyPlayer";
            return true;
        }
    } catch (...) {}
    try {
        RValue obj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        if (obj.ToDouble() >= 0) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, RValue(0.0) });
            // MEASURED 2026-09-10: this runner returns an instance REFERENCE
            // (VALUE_REF, kind 15) here, not a number.  The old code accepted
            // only VALUE_REAL/INT32/INT64, so this fallback ALWAYS failed and
            // every feature gated on the local player silently did nothing -
            // orbpickup logged seen=176993 with noplayer=176993, and the relic
            // filter never armed.  A reference is passed straight through;
            // variable_instance_get accepts it.
            if (HhUsableInstance(inst)) {
                out = inst;
                if (how) *how = "instance_find(Player_obj)";
                return true;
            }
            // Older runners hand back a bare instance id.
            if (inst.m_Kind == VALUE_REAL || inst.m_Kind == VALUE_INT32 || inst.m_Kind == VALUE_INT64) {
                CInstance* player = nullptr;
                if (AurieSuccess(g_Yytk->GetInstanceObject((int32_t)inst.ToDouble(), player)) && player) {
                    out = player->ToRValue();
                    if (how) *how = "instance_find(Player_obj) id";
                    return true;
                }
            }
        }
    } catch (...) {}
    if (how) *how = "none";
    return false;
}

static long g_HhHudCalls = 0, g_HhLabelDraws = 0;
static std::string g_HhLabelLastErr;
// Draw GUI phase: project the player's position through the active camera and draw the
// stolen-affix labels above the head, centred, in the affix-row gold with a dark outline.
static void HhDrawHeadLabels()
{
    if (!g_HhLabelOn.load() || g_HhStolen.empty()) return;
    const double now = HhNowMs();
    g_HhStolen.erase(std::remove_if(g_HhStolen.begin(), g_HhStolen.end(),
        [&](const HhStolen& s) { return s.expiryMs <= now || (now - s.bornMs > 500.0 && !HhBuffAlive(s.buffId)); }), g_HhStolen.end());
    if (g_HhStolen.empty()) return;
    try {
        RValue id;
        if (!HhResolveLocalPlayer(id)) { g_HhLabelLastErr = "local player not found"; return; }
        const double x = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("x") }).ToDouble();
        const double top = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("bbox_top") }).ToDouble();
        RValue cam = g_Yytk->CallBuiltin("view_get_camera", { RValue(0.0) });
        const double vx = g_Yytk->CallBuiltin("camera_get_view_x", { cam }).ToDouble();
        const double vy = g_Yytk->CallBuiltin("camera_get_view_y", { cam }).ToDouble();
        const double vw = g_Yytk->CallBuiltin("camera_get_view_width", { cam }).ToDouble();
        const double vh = g_Yytk->CallBuiltin("camera_get_view_height", { cam }).ToDouble();
        const double gw = g_Yytk->CallBuiltin("display_get_gui_width", {}).ToDouble();
        const double gh = g_Yytk->CallBuiltin("display_get_gui_height", {}).ToDouble();
        if (vw <= 0 || vh <= 0) { g_HhLabelLastErr = "camera view size 0 (vw=" + std::to_string(vw) + " vh=" + std::to_string(vh) + ")"; return; }
        const double sx = (x - vx) * gw / vw;
        const double sy = (top - vy) * gh / vh - g_HhLabelOffsetPx;
        std::vector<std::string> lines; std::string line; int inLine = 0;
        for (const HhStolen& s : g_HhStolen) {
            const int left = (int)std::ceil((s.expiryMs - now) / 1000.0);
            std::string label = s.name + " " + std::to_string(left < 0 ? 0 : left) + "s";
            if (inLine == 3) { lines.push_back(line); line.clear(); inLine = 0; }
            line += (inLine ? "   " : "") + label; ++inLine;
        }
        if (!line.empty()) lines.push_back(line);
        RValue prevFont = g_Yytk->CallBuiltin("draw_get_font", {});
        RValue prevHalign = g_Yytk->CallBuiltin("draw_get_halign", {});
        RValue prevValign = g_Yytk->CallBuiltin("draw_get_valign", {});
        RValue prevColour = g_Yytk->CallBuiltin("draw_get_colour", {});
        RValue prevAlpha = g_Yytk->CallBuiltin("draw_get_alpha", {});
        if (!g_HhLabelFont.empty()) {
            try {
                RValue f = g_Yytk->CallBuiltin("asset_get_index", { RValue(g_HhLabelFont) });
                if (f.ToDouble() < 0) f = RValue(std::stod(g_HhLabelFont));
                if (f.ToDouble() >= 0) g_Yytk->CallBuiltin("draw_set_font", { f });
            } catch (...) {}
        }
        g_Yytk->CallBuiltin("draw_set_halign", { RValue(1.0) });
        g_Yytk->CallBuiltin("draw_set_valign", { RValue(2.0) });   // bottom-aligned: stack upwards from sy
        g_Yytk->CallBuiltin("draw_set_alpha", { RValue(1.0) });
        const double lineH = g_Yytk->CallBuiltin("string_height", { RValue("Ag") }).ToDouble();
        RValue gold = g_Yytk->CallBuiltin("make_colour_rgb", { RValue(242.0), RValue(196.0), RValue(98.0) });
        double y = sy - lineH * (double)(lines.size() - 1);
        for (const std::string& l : lines) { HhDrawOutlinedWorld(sx, y, l, gold); y += lineH; }
        ++g_HhLabelDraws;
        g_Yytk->CallBuiltin("draw_set_alpha", { prevAlpha });
        g_Yytk->CallBuiltin("draw_set_colour", { prevColour });
        g_Yytk->CallBuiltin("draw_set_valign", { prevValign });
        g_Yytk->CallBuiltin("draw_set_halign", { prevHalign });
        g_Yytk->CallBuiltin("draw_set_font", { prevFont });
    } catch (...) { g_HhLabelLastErr = "exception while drawing"; }
}
// DrawHudBuffs runs once per frame in the Draw GUI phase (the buff icon row); the labels
// are drawn right after it so they sit on top of the world and under nothing.
static PFUNC_YYGMLScript g_Orig_DrawHudBuffs = nullptr;
static RValue& Hook_DrawHudBuffs(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    PERF_SCOPE(g_PerfHud);
    RValue& r = g_Orig_DrawHudBuffs ? g_Orig_DrawHudBuffs(S, O, R, argc, A) : R;
    ++g_HhHudCalls;
    HhDrawHeadLabels();
    return r;
}
static bool g_HhLabelHookAttempted = false;
static void InstallHeadLabelHook()
{
    if (g_HhLabelHookAttempted) return;
    g_HhLabelHookAttempted = true;
    g_HhObjectCallbackInstalled = HookOneScript("DrawHudBuffs", "fp_hh_hudlabels", (PVOID)Hook_DrawHudBuffs, &g_Orig_DrawHudBuffs);
}

// ===== Tyrant's Crown mechanic ====================================================
// EnemyRaritySettings(typeId) runs from Enemy_Parent_obj Alarm_4 with self = the enemy
// AFTER the spawner decided enemyRarity (1 normal / 2 champion / 3 rare / 4 ancient) and
// filled enemyAffix / affixList, and BEFORE stats, affix effects and the health bar are
// built (live-traced 2026-09-05: entry and exit state identical, myHealthBar still -4).
// Changing the rarity and the affix flags at its entry therefore makes the game build the
// monster exactly as if it had rolled that way.
static void InstallBeaconHook();   // defined with the Beacon module below; the crown shares its hunt hooks
static std::atomic<bool> g_TyEnabled{ false };
static std::atomic<bool> g_TyForced{ false };
static double g_TyRarePct = 30.0;      // chance a normal monster rises to rare (15 was too subtle to notice; 30 = 2-3 rares per pack)
static double g_TyAffixPct = 100.0;    // chance a rare / champion carries one more affix
static long g_TySeen = 0, g_TyUpgraded = 0, g_TyAffixed = 0;
// Monster Rarity sliders: of the normal monsters, `g_RarAncientPct` percent are
// raised to Ancient (4) and `g_RarRarePct` percent to Rare (3) at the same hook,
// one die per monster so the shares are exclusive (25 rare + 15 ancient leaves
// 60 normal).  The game itself builds ordinary monster objects at rarity 4
// (traced 2026-09-05: Scorching_Legion_obj at enemyRarity 4 with 2-4 affixes),
// so a raised monster is exactly a state the game produces on its own.
static double g_RarRarePct = 0.0;
static double g_RarAncientPct = 0.0;
static long g_RarRaisedRare = 0, g_RarRaisedAncient = 0;
static bool RarityFloorActive() { return g_RarRarePct > 0.0 || g_RarAncientPct > 0.0; }
static bool g_TyHookInstalled = false, g_TyHookAttempted = false;
static PFUNC_YYGMLScript g_Orig_EnemyRaritySettings = nullptr;
// enemyAffix indices whose meaning is live-confirmed (see kHhAffixNames); 0 = champion marker.
// Left out on purpose (2026-09-07 player reports "the more I kill, the more there are"):
// 1 fractal (copies), 4 haunted, 7 possessed, 21 fallenangel (rises again on death).  A raised
// monster with one of those spawns new monsters, those get raised too, and the zone snowballs.
static const int kTyAffixPool[] = { 2, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 25, 30, 31, 36, 38 };
static std::mt19937& TyRng() { static std::mt19937 rng{ std::random_device{}() }; return rng; }
static bool TyRoll(double pct) { if (pct <= 0.0) return false; if (pct >= 100.0) return true; return std::uniform_real_distribution<double>(0.0, 100.0)(TyRng()) < pct; }

static bool ForgedItemMechanicIs(const RValue& item, const char* mech)
{
    try {
        if (item.m_Kind != VALUE_OBJECT || !item.m_Object) return false;
        RValue has = g_Yytk->CallBuiltin("variable_struct_exists", { item, RValue("fp_mechanic") });
        if (!has.ToBoolean()) return false;
        RValue m = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("fp_mechanic") });
        return Lower(m.ToString()) == mech;
    } catch (...) { return false; }
}
static bool TyrantItemLoaded()
{
    return g_TyItemTagged.load();
}
static bool MechanicWorn(const char* mechanic);   // defined with the Headhunter module below
static bool TyrantActive()
{
    if (!g_TyEnabled.load()) return false;
    if (g_TyForced.load()) return true;
    return MechanicWorn("tyrant");   // the crown counts while it is worn
}
// Adds `count` random affixes the monster does not have yet: flag in enemyAffix, index in affixList.
static int TyAddAffixes(const RValue& inst, int count)
{
    int added = 0;
    try {
        RValue ea = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("enemyAffix") });
        if (ea.m_Kind != VALUE_ARRAY) return 0;
        const int n = (int)g_Yytk->CallBuiltin("array_length", { ea }).ToDouble();
        RValue al = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("affixList") });
        std::vector<int> pool;
        for (int k : kTyAffixPool) {
            if (k >= n) continue;
            RValue v = g_Yytk->CallBuiltin("array_get", { ea, RValue((double)k) });
            if (v.ToDouble() == 0.0) pool.push_back(k);
        }
        std::shuffle(pool.begin(), pool.end(), TyRng());
        for (int i = 0; i < count && i < (int)pool.size(); ++i) {
            g_Yytk->CallBuiltin("array_set", { ea, RValue((double)pool[i]), RValue(1.0) });
            if (al.m_Kind == VALUE_ARRAY) g_Yytk->CallBuiltin("array_push", { al, RValue((double)pool[i]) });
            ++added;
        }
    } catch (...) {}
    return added;
}
static int TyCountAffixes(const RValue& inst)
{
    try {
        RValue ea = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("enemyAffix") });
        if (ea.m_Kind != VALUE_ARRAY) return 0;
        const int n = (int)g_Yytk->CallBuiltin("array_length", { ea }).ToDouble();
        int c = 0;
        for (int k = 0; k < n; ++k) if (g_Yytk->CallBuiltin("array_get", { ea, RValue((double)k) }).ToDouble() != 0.0) ++c;
        return c;
    } catch (...) { return 0; }
}
#ifndef FORGEPACT_RELEASE
static std::string TyInstName(const RValue& inst)
{
    try {
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("object_index") });
        RValue nm = g_Yytk->CallBuiltin("object_get_name", { oi });
        RValue id = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("id") });
        return nm.ToString() + "#" + std::to_string((long long)id.ToDouble());
    } catch (...) { return "?"; }
}
static std::string TyArgs(int argc, RValue** A)
{
    std::string a;
    for (int i = 0; i < argc && i < 8; ++i) a += " a" + std::to_string(i) + "=" + (A && A[i] ? Describe(*A[i]) : std::string("?"));
    return a;
}
static int g_RarTraceLeft = 0, g_RarForceLeft = 0, g_RarPreLeft = 0;
static double g_RarForceVal = 3, g_RarPreVal = 3;
static std::string RarState(const RValue& inst)
{
    std::string s;
    auto get = [&](const char* nm) -> std::string {
        try { RValue v = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue(nm) }); return Describe(v); } catch (...) { return "?"; }
    };
    s += " enemyRarity=" + get("enemyRarity") + " forceRarity=" + get("forceRarity");
    try {
        RValue al = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("affixList") });
        s += " affixList=" + (al.m_Kind == VALUE_ARRAY ? std::to_string((int)g_Yytk->CallBuiltin("array_length", { al }).ToDouble()) + HhDescribeList(al) : Describe(al));
        RValue ea = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("enemyAffix") });
        if (ea.m_Kind == VALUE_ARRAY) {
            int n = (int)g_Yytk->CallBuiltin("array_length", { ea }).ToDouble(); std::string idx;
            for (int i = 0; i < n; ++i) { RValue v = g_Yytk->CallBuiltin("array_get", { ea, RValue((double)i) }); if (v.ToDouble() != 0.0) idx += (idx.empty() ? "" : ",") + std::to_string(i); }
            s += " enemyAffix[" + std::to_string(n) + "] set=" + (idx.empty() ? "-" : idx);
        } else s += " enemyAffix=" + Describe(ea);
        RValue hb = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("myHealthBar") });
        s += " myHealthBar=" + Describe(hb);
    } catch (...) { s += " (state exc)"; }
    return s;
}
#endif
static RValue& Hook_EnemyRaritySettings(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    PERF_SCOPE(g_PerfRarity);
    ++g_TySeen;
    RValue inst; try { if (S) inst = S->ToRValue(); } catch (...) { S = nullptr; }
#ifndef FORGEPACT_RELEASE
    bool trace = g_RarTraceLeft > 0;
    if (trace) { --g_RarTraceLeft; Out("rarity #" + std::to_string(g_TySeen) + " ENTRY self=" + TyInstName(inst) + " argc=" + std::to_string(argc) + TyArgs(argc, A) + RarState(inst)); }
    if (g_RarForceLeft > 0 && S) { --g_RarForceLeft; try { g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("forceRarity"), RValue(g_RarForceVal) }); Out("   -> forceRarity set to " + std::to_string((int)g_RarForceVal)); } catch (...) {} }
    if (g_RarPreLeft > 0 && S) { --g_RarPreLeft; try { g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("enemyRarity"), RValue(g_RarPreVal) }); Out("   -> enemyRarity pre-set to " + std::to_string((int)g_RarPreVal)); } catch (...) {} }
#endif
    bool enemyBorn = g_CreatingFromEnemy;
    if (!enemyBorn && S && (RarityFloorActive() || TyrantActive())) {
        try { const double id = InstanceIdOf(inst); if (id >= 0.0 && g_EnemyBornIds.erase((int)id)) enemyBorn = true; } catch (...) {}
    }
    if (enemyBorn && S && (RarityFloorActive() || TyrantActive())) InterlockedIncrement(&g_RarSkippedEnemyBorn);
    if (S && !enemyBorn && RarityFloorActive()) {
        try {
            RValue rv = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("enemyRarity") });
            const double rar = (rv.m_Kind == VALUE_REAL || rv.m_Kind == VALUE_INT32 || rv.m_Kind == VALUE_INT64) ? rv.ToDouble() : -1.0;
            if (rar == 1.0) {
                const double roll = std::uniform_real_distribution<double>(0.0, 100.0)(TyRng());
                int tier = 0;
                if (roll < g_RarAncientPct) tier = 4;
                else if (roll < g_RarAncientPct + g_RarRarePct) tier = 3;
                if (tier) {
                    g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("enemyRarity"), RValue((double)tier) });
                    // the game's own ancients carry 2-4 affixes, its rares 1-2
                    const int want = (tier == 4) ? 3 : 2;
                    const int have = TyCountAffixes(inst);
                    if (want > have) TyAddAffixes(inst, want - have);
                    if (tier == 4) ++g_RarRaisedAncient; else ++g_RarRaisedRare;
                }
            }
        } catch (...) {}
    }
    if (S && !enemyBorn && TyrantActive()) {
        try {
            RValue rv = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("enemyRarity") });
            const double rar = (rv.m_Kind == VALUE_REAL || rv.m_Kind == VALUE_INT32 || rv.m_Kind == VALUE_INT64) ? rv.ToDouble() : -1.0;
            if (rar == 1.0 && TyRoll(g_TyRarePct)) {
                g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("enemyRarity"), RValue(3.0) });
                const int have = TyCountAffixes(inst);
                if (have < 2) TyAddAffixes(inst, 2 - have);
                ++g_TyUpgraded;
            } else if ((rar == 2.0 || rar == 3.0) && TyRoll(g_TyAffixPct)) {
                if (TyAddAffixes(inst, 1) > 0) ++g_TyAffixed;
            }
        } catch (...) {}
    }
    RValue& r = g_Orig_EnemyRaritySettings ? g_Orig_EnemyRaritySettings(S, O, R, argc, A) : R;
#ifndef FORGEPACT_RELEASE
    if (trace) Out("rarity #" + std::to_string(g_TySeen) + " EXIT  -> " + Describe(r) + RarState(inst));
#endif
    return r;
}
static void InstallCreateHooks();
static void InstallTyrantHook()
{
    if (g_TyHookAttempted) return;
    g_TyHookAttempted = true;
    g_TyHookInstalled = HookOneScript("EnemyRaritySettings", "fp_tyrant_rarity", (PVOID)Hook_EnemyRaritySettings, &g_Orig_EnemyRaritySettings);
    InstallCreateHooks();   // enemy-born tracking rides the instance_create hooks
}
static void TyrantAutoArm()
{
    bool wanted = g_TyForced.load();
    // Skip builtin (recognition-only) entries: the plugin's own hardcoded Tyrant's
    // Crown seed always satisfies this loop otherwise, arming the mechanic for
    // every player on every launch regardless of the panel toggle or whether they
    // have ever seen the item. Only a sidecar entry the player actually forged
    // counts as "wanted" here (bug found 2026-09-11).
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (!e.builtin && e.mechanic == "tyrant") { wanted = true; break; }   // active only while the crown is worn
    if (!wanted) return;
    InstallTyrantHook();
    InstallBeaconHook();   // "Rare monsters hunt you": rares use the Beacon's scan/leash/wake hooks
    g_TyEnabled.store(g_TyHookInstalled);
    Out(std::string("tyrant: ") + (g_TyHookInstalled ? "armed" : "hook failed") + " (rare " + std::to_string((int)g_TyRarePct) + " pct, extra affix " + std::to_string((int)g_TyAffixPct) + " pct)");
}
static void TyrantStatus()
{
    Out(std::string("tyrant: ") + (g_TyEnabled.load() ? "ON" : "off") + (g_TyForced.load() ? " (forced)" : "")
        + " hook=" + (g_TyHookInstalled ? "yes" : "no") + " active=" + (TyrantActive() ? "yes" : "no")
        + " rarePct=" + std::to_string((int)g_TyRarePct) + " affixPct=" + std::to_string((int)g_TyAffixPct)
        + " seen=" + std::to_string(g_TySeen) + " upgraded=" + std::to_string(g_TyUpgraded) + " extraAffix=" + std::to_string(g_TyAffixed)
        + " itemLoaded=" + (TyrantItemLoaded() ? "yes" : "no") + " worn=" + (MechanicWorn("tyrant") ? "yes" : "no"));
}

#ifndef FORGEPACT_RELEASE
// --- monster AI target trace (Beacon amulet groundwork) ------------------------------
static int g_AggroTraceLeft = 0;
static std::set<std::string> g_AggroScanSeen;
static std::string AggroArgs(int argc, RValue** A)
{
    std::string a;
    for (int i = 0; i < argc && i < 8; ++i) {
        std::string d = A && A[i] ? Describe(*A[i]) : std::string("?");
        if (A && A[i] && A[i]->m_Kind == VALUE_OBJECT) {
            try { CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g); RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { *A[i] }); d = "struct" + js.ToString(); } catch (...) {}
        }
        if (d.size() > 200) d = d.substr(0, 200) + "...";
        a += " a" + std::to_string(i) + "=" + d;
    }
    return a;
}
static std::string AggroVars(CInstance* S)
{
    std::string line;
    try {
        RValue id = S->ToRValue();
        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { id });
        int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
        for (int i = 0; i < n; ++i) {
            RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            std::string s = nm.ToString(), ls = Lower(s);
            if (ls.find("target") != std::string::npos || ls.find("socket") != std::string::npos || ls.find("scan") != std::string::npos || ls.find("aggro") != std::string::npos
                || ls.find("range") != std::string::npos || ls.find("radius") != std::string::npos || ls.find("leash") != std::string::npos || ls.find("home") != std::string::npos
                || ls.find("state") != std::string::npos || ls.find("idle") != std::string::npos || ls.find("chase") != std::string::npos || ls.find("sight") != std::string::npos
                || ls.find("detect") != std::string::npos || ls.find("spawn") != std::string::npos || ls.find("taunt") != std::string::npos) {
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, nm });
                std::string d = Describe(v); if (d.size() > 60) d = d.substr(0, 60) + "...";
                line += " " + s + "=" + d;
            }
        }
    } catch (...) { line += " (exc)"; }
    return line;
}
#define AGGRO_TRACE_HOOK(NAME) \
    static PFUNC_YYGMLScript g_OrigAggro_##NAME = nullptr; \
    static RValue& Hook_Trace##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        bool tr = g_AggroTraceLeft > 0; if (tr) --g_AggroTraceLeft; \
        std::string before = tr ? AggroArgs(argc, A) : std::string(); \
        RValue& r = g_OrigAggro_##NAME ? g_OrigAggro_##NAME(S, O, R, argc, A) : R; \
        if (tr) Out(std::string("aggro " #NAME " self=") + TyInstName(S ? S->ToRValue() : RValue()) + " other=" + TyInstName(O ? O->ToRValue() : RValue()) + " argc=" + std::to_string(argc) + before + " -> " + Describe(r)); \
        return r; \
    }
AGGRO_TRACE_HOOK(PathFindTakeTarget)
AGGRO_TRACE_HOOK(SocketSetTarget)
AGGRO_TRACE_HOOK(PathFindAggroBroadcast)
static bool g_AggroHooksInstalled = false;
static void InstallAggroTraceHooks()
{
    if (g_AggroHooksInstalled) return; g_AggroHooksInstalled = true;
    HookOneScriptTable("PathFindTakeTarget",     "bp_tr_take",  (PVOID)Hook_TracePathFindTakeTarget,     &g_OrigAggro_PathFindTakeTarget);
    HookOneScriptTable("SocketSetTarget",        "bp_tr_sst",   (PVOID)Hook_TraceSocketSetTarget,        &g_OrigAggro_SocketSetTarget);
    HookOneScriptTable("PathFindAggroBroadcast", "bp_tr_bc",    (PVOID)Hook_TracePathFindAggroBroadcast, &g_OrigAggro_PathFindAggroBroadcast);
}
#endif
// ===== Beacon amulet mechanic =====================================================
// Live-traced 2026-09-05: idle monsters run PathFindScanTick (self = monster), which takes
// the player as target once it is inside `aggroRange` (300 px vanilla) through
// PathFindTakeTarget(playerRef) -> SocketSetTarget + PathFindAggroBroadcast(1) (pack mates
// follow).  In chase state PathFindLeashCheck releases the target (SocketSetTarget(-4)) when
// the monster strays too far from home.  The Beacon hands every scanning monster a
// map-sized aggroRange (vanilla value kept in fp_aggroRange, restored when the amulet is
// off) and skips the leash check, so the game's own scan / target / broadcast code does
// everything else: monsters come from anywhere and never turn back.
static std::atomic<bool> g_BeEnabled{ false };
static std::atomic<bool> g_BeForced{ false };
static double g_BeRange = 1000000.0;   // aggroRange handed out while the amulet is on
static bool g_BeRareOnly = false;      // beaconmode rare: only rares / champions hunt you
static long g_BeScans = 0, g_BeRanged = 0, g_BeLeashSkips = 0;
static bool g_BeHookInstalled = false, g_BeHookAttempted = false;
static PFUNC_YYGMLScript g_Orig_PathFindScanTick = nullptr;
static PFUNC_YYGMLScript g_Orig_PathFindLeashCheck = nullptr;
static bool BeaconActive()
{
    if (!g_BeEnabled.load()) return false;
    if (g_BeForced.load()) return true;
    return MechanicWorn("beacon");   // the amulet counts while it is worn
}
// Who hunts the player right now: 0 = nobody, 1 = rares and champions only, 2 = everyone.
// The Beacon amulet decides all/rare through beaconmode; a Tyrant's Crown alone means rares.
static int HuntPolicy()
{
    if (BeaconActive()) return g_BeRareOnly ? 1 : 2;
    if (TyrantActive()) return 1;
    return 0;
}
static bool HuntWants(const RValue& inst, int policy)
{
    if (policy == 2) return true;
    if (policy != 1) return false;
    try {
        RValue rv = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("enemyRarity") });
        return (rv.m_Kind == VALUE_REAL || rv.m_Kind == VALUE_INT32 || rv.m_Kind == VALUE_INT64) && rv.ToDouble() >= 2.0;
    } catch (...) { return false; }
}
static long g_BeScanNear = 0, g_BeScanMid = 0, g_BeScanFar = 0;   // scanning monsters by distance to the player
static RValue& Hook_PathFindScanTick(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    ++g_BeScans;
#ifndef FORGEPACT_RELEASE
    // research telemetry: distance histogram of scans
    if (S && (g_BeScans % 10) == 0) {
        try {
            RValue player; if (HhResolveLocalPlayer(player)) {
                RValue inst = S->ToRValue();
                double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble(), py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
                double ex = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble(), ey = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
                double d = std::sqrt((ex - px) * (ex - px) + (ey - py) * (ey - py));
                if (d < 1500) ++g_BeScanNear; else if (d < 3000) ++g_BeScanMid; else ++g_BeScanFar;
            }
        } catch (...) {}
    }
#endif
    if (S) {
        try {
            RValue inst = S->ToRValue();
            const int policy = HuntPolicy();
            if (policy != 0) {
                if (HuntWants(inst, policy)) {
                    // PathFindScanTick (decompiled): nearest = instance_nearest(x, y, <player obj>);
                    // if point_distance(...) < self.distance -> PathFindTakeTarget(nearest).
                    // `distance` is the detection radius; aggroRange is widened too for the
                    // broadcast / attack code that reads it.
                    for (const char* field : { "distance", "aggroRange" }) {
                        RValue cur = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue(field) });
                        if (cur.m_Kind == VALUE_UNDEFINED || cur.ToDouble() < g_BeRange) {
                            const std::string backup = std::string("fp_") + field;
                            RValue has = g_Yytk->CallBuiltin("variable_instance_exists", { inst, RValue(backup) });
                            if (!has.ToBoolean()) g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue(backup), cur.m_Kind == VALUE_UNDEFINED ? RValue(300.0) : cur });
                            g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue(field), RValue(g_BeRange) });
                            if (field[0] == 'd') ++g_BeRanged;
                        }
                    }
                }
            } else if (g_BeRanged > 0) {   // nothing to restore until a hunt widened a monster
                for (const char* field : { "distance", "aggroRange" }) {
                    const std::string backup = std::string("fp_") + field;
                    RValue has = g_Yytk->CallBuiltin("variable_instance_exists", { inst, RValue(backup) });
                    if (!has.ToBoolean()) continue;
                    RValue orig = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue(backup) });
                    RValue cur = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue(field) });
                    if (cur.ToDouble() != orig.ToDouble()) g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue(field), orig });
                }
            }
        } catch (...) {}
    }
    RValue& r = g_Orig_PathFindScanTick ? g_Orig_PathFindScanTick(S, O, R, argc, A) : R;
#ifndef FORGEPACT_RELEASE
    if (g_AggroTraceLeft > 0 && S) {
        std::string nm = TyInstName(S->ToRValue()); std::string obj = nm.substr(0, nm.find('#'));
        if (g_AggroScanSeen.insert(obj).second) { --g_AggroTraceLeft; Out("aggro ScanTick self=" + nm + " argc=" + std::to_string(argc) + AggroArgs(argc, A) + " -> " + Describe(r) + " | vars:" + AggroVars(S)); }
    }
#endif
    return r;
}
static RValue& Hook_PathFindLeashCheck(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (S) {
        const int policy = HuntPolicy();
        if (policy != 0 && HuntWants(S->ToRValue(), policy)) { ++g_BeLeashSkips; return R; }   // no leash: they never turn back
    }
    RValue& r = g_Orig_PathFindLeashCheck ? g_Orig_PathFindLeashCheck(S, O, R, argc, A) : R;
#ifndef FORGEPACT_RELEASE
    if (g_AggroTraceLeft > 0) { --g_AggroTraceLeft; Out("aggro PathFindLeashCheck self=" + TyInstName(S ? S->ToRValue() : RValue()) + " argc=" + std::to_string(argc) + " -> " + Describe(r)); }
#endif
    return r;
}
// Wake radius: the game freezes monsters outside the view boxes every frame
// ((Local)ActivateDeactivateProps from Controller_obj Step).  After that call the Beacon
// re-activates every Enemy_Parent_obj and sends the ones farther than g_BeWakeRadius from
// the player back to sleep, so monsters within the radius keep stepping, scanning and
// hunting.  0 = leave the game's own freezing alone, < 0 = the whole map.
static double g_BeWakeRadius = 4000.0;
static long g_BeWakeCalls = 0, g_BeWoken = 0, g_BeCreatorsAwake = 0;
static bool g_BeWakeCreators = true;   // also wake Enemy_Creator* spawners inside the radius
static const char* const kBeCreatorObjects[] = {
    "Enemy_Creator_obj", "Enemy_Creator_Ambush_obj", "Enemy_Creator_Ancient_obj", "Enemy_Creator_Champion_obj",
    "Enemy_Creator_Colossal_Chest_obj", "Enemy_Creator_Legion_obj", "Enemy_Creator_Miniboss_obj",
};
// Re-activate every instance of `obj`, then put the ones beyond `radius` of (px,py) back to
// sleep.  radius < 0 keeps them all awake.  Returns the number left awake.
// policy: 2 = wake every instance of obj inside the radius, 1 = only enemyRarity >= 2.
// Instances the game itself left active are never touched.
static long BeWakeObject(const RValue& obj, double px, double py, double radius, int policy)
{
    std::unordered_set<int> gameActive;
    {
        const int n0 = (int)g_Yytk->CallBuiltin("instance_number", { obj }).ToDouble();
        for (int i = 0; i < n0; ++i) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, RValue((double)i) });
            gameActive.insert((int)g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("id") }).ToDouble());
        }
    }
    g_Yytk->CallBuiltin("instance_activate_object", { obj });
    const int n = (int)g_Yytk->CallBuiltin("instance_number", { obj }).ToDouble();
    long awake = 0;
    for (int i = n - 1; i >= 0; --i) {
        RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, RValue((double)i) });
        const int id = (int)g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("id") }).ToDouble();
        if (gameActive.count(id)) { ++awake; continue; }
        bool keep = HuntWants(inst, policy);
        if (keep && radius >= 0.0) {
            const double ex = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
            const double ey = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
            const double dx = ex - px, dy = ey - py;
            keep = dx * dx + dy * dy <= radius * radius;
        }
        if (keep) ++awake; else g_Yytk->CallBuiltin("instance_deactivate_object", { inst });
    }
    return awake;
}
static PFUNC_YYGMLScript g_Orig_ActivateDeactivateProps = nullptr;
static PFUNC_YYGMLScript g_Orig_LocalActivateDeactivateProps = nullptr;
static void BeaconWakeEnemies()
{
    const int policy = HuntPolicy();
    if (policy == 0 || g_BeWakeRadius == 0.0) return;
    try {
        RValue obj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
        if (obj.ToDouble() < 0) return;
        RValue player;
        if (!HhResolveLocalPlayer(player)) return;
        const double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
        const double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
        ++g_BeWakeCalls;
        g_BeWoken = BeWakeObject(obj, px, py, g_BeWakeRadius, policy);
        if (g_BeWakeCreators && BeaconActive()) {   // spawners only matter for the amulet
            long awake = 0;
            for (const char* nm : kBeCreatorObjects) {
                RValue cobj = g_Yytk->CallBuiltin("asset_get_index", { RValue(nm) });
                if (cobj.ToDouble() >= 0) awake += BeWakeObject(cobj, px, py, g_BeWakeRadius, 2);
            }
            g_BeCreatorsAwake = awake;
        }
    } catch (...) {}
}
// An instance activated inside the controller's Step does not step in that frame, and the
// next frame the controller freezes it again before it gets a turn - so monsters woken every
// frame never move (measured: zero scans from beyond 1500 px).  While a hunt is on, the
// game's freeze pass therefore runs only every g_BeWakeEvery frames; in between, the monsters
// woken by the last pass keep stepping.  Props at the screen edge appear a few frames late.
static int g_BeWakeEvery = 6;
static long g_BeFreezeCalls = 0, g_BeFreezeSkipped = 0;
static bool BeaconFreezeGate()
{
    ++g_BeFreezeCalls;
    if (HuntPolicy() == 0 || g_BeWakeRadius == 0.0 || g_BeWakeEvery <= 1) return true;
    if ((g_BeFreezeCalls % g_BeWakeEvery) != 0) { ++g_BeFreezeSkipped; return false; }
    return true;
}
// The game steps monsters through Controller_obj -> EnemyStepHandleNew -> monsterHandleArray,
// and far monsters never get their turn (measured: zero scans beyond 1500 px even while
// active).  While a hunt is on, hunted monsters farther than g_BeFarFrom px get their AI
// tick (PathFindStep: scan -> take target -> chase -> path_start) from the plugin every
// frame; GameMaker moves them along the started path by itself.  Near monsters are left to
// the game so nothing is stepped twice.
static bool g_BeFarStep = false;   // EXPERIMENTAL, off: forcing PathFindStep on far monsters crashed the game on zone entry (0xc0000005 @ Hero_Siege+0x4d967ab, 2026-09-05)
static double g_BeFarFrom = 1500.0;
static long g_BeFarSteps = 0, g_BeFarStepErrors = 0;
static void BeaconStepFarHunters()
{
    if (!g_BeFarStep) return;
    const int policy = HuntPolicy();
    if (policy == 0) return;
    try {
        RValue eobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
        RValue player; if (!HhResolveLocalPlayer(player)) return;
        const double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
        const double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
        const int n = (int)g_Yytk->CallBuiltin("instance_number", { eobj }).ToDouble();
        for (int i = 0; i < n; ++i) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { eobj, RValue((double)i) });
            const double ex = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
            const double ey = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
            const double dx = ex - px, dy = ey - py;
            if (dx * dx + dy * dy <= g_BeFarFrom * g_BeFarFrom) continue;
            if (!HuntWants(inst, policy)) continue;
            CInstance* ci = inst.ToInstance();
            if (!ci) { ++g_BeFarStepErrors; continue; }
            RValue res;
            AurieStatus st = g_Yytk->CallGameScriptEx(res, "gml_Script_PathFindStep", ci, ci, {});
            if (AurieSuccess(st)) ++g_BeFarSteps; else ++g_BeFarStepErrors;
        }
    } catch (...) { ++g_BeFarStepErrors; }
}
static RValue& Hook_ActivateDeactivateProps(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    BeaconStepFarHunters();
    if (!BeaconFreezeGate()) return R;
    RValue& r = g_Orig_ActivateDeactivateProps ? g_Orig_ActivateDeactivateProps(S, O, R, argc, A) : R;
    BeaconWakeEnemies();
    return r;
}
static RValue& Hook_LocalActivateDeactivateProps(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    BeaconStepFarHunters();
    if (!BeaconFreezeGate()) return R;
    RValue& r = g_Orig_LocalActivateDeactivateProps ? g_Orig_LocalActivateDeactivateProps(S, O, R, argc, A) : R;
    BeaconWakeEnemies();
    return r;
}
// Spawn as if approached: Enemy_Creator_obj's periodic check (anon@849, decompiled
// 2026-09-05) calls distance_to_object(Player_obj) and spawns its pack (alarm[2]) below
// 1050 px.  With the Beacon on, the builtin answers 0 to every creator inside the wake
// radius, so awake spawners give birth at once and the newborns join the hunt.
static bool g_BeSpawnNear = false;   // experimental: spawners in this zone type had already given birth at load; off by default
static long g_BeSpawnLies = 0;
static long g_RevealSpawnLies = 0;   // same lie, asked for by map-reveal's pack pass
static TRoutine g_OrigDistanceToObject = nullptr;
static bool g_DistanceHookAttempted = false;
static std::atomic<bool> g_OrbPickupRadius{ false };
static constexpr double kOrbPickupFactor = 10.0;

// MEASURED 2026-09-09: the globes do NOT reach the player through
// distance_to_object - with the player and all four globe objects resolved
// correctly, a full session shortened exactly 0 distance checks.  Each globe
// instead runs its own step script (gml_Script_ExpGlobeStepMain /
// gml_Script_MFGlobeStepMain, SDK script indices 1804 / 1805), so the pickup
// radius is widened by hooking those and pulling the globe toward the player
// ourselves.  That needs no knowledge of the script's internal variables: once
// the globe is close enough, the game's own pickup logic fires normally.
static volatile long g_OrbPickupHits = 0;   // globes pulled
// "pulled 0 globes" has several very different causes, so each one is counted:
// the hook never firing is a different bug from the hook firing with no player
// or with every globe out of reach.
static volatile long g_OrbNoPlayer  = 0;    // globe found, but no player position
static std::string g_OrbPlayerHow = "(not tried)";   // which resolver found the player
static volatile long g_OrbOutOfReach = 0;   // ...but the globe was too far
static volatile long g_OrbNearestPx = -1;   // closest globe seen, in px
// The player position, refreshed once per frame by the frame callback, so a
// globe's step hook costs no player lookup of its own.
static std::atomic<bool> g_PlayerPosValid{ false };
static double g_PlayerX = 0.0, g_PlayerY = 0.0;
// Vanilla globe pickup radius is roughly a tile; x10 is what the panel asks for.
static constexpr double kGlobeBaseRadius = 48.0;
// Constant velocity, not an accelerating ramp: the earlier version sped up as the
// globe closed in and snapped the last stretch in a single frame once step >= d,
// which read as an unnatural teleport right before pickup (user report
// 2026-09-10: "make the orbs come a little slower ... like it is done normally").
// A fixed px/frame speed glides in steadily instead, arriving smoothly (the same
// step >= d check now only prevents overshooting past the player).
static constexpr double kGlobePullSpeed  = 6.0;    // px per frame, constant

static void Hook_distance_to_object(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigDistanceToObject) g_OrigDistanceToObject(Result, S, O, argc, Args);

    // Two independent callers want the same lie, for different reasons:
    //   - the Beacon, continuously, so awake spawners inside the wake radius
    //     give birth and the newborns join the hunt;
    //   - map-reveal, for a bounded window after each new zone, so the whole
    //     map's packs exist and therefore appear on the revealed minimap
    //     (measured 2026-09-11: 208 -> 1273 enemies in one zone).
    // Map-reveal deliberately ignores the wake radius: its whole point is the
    // far side of the map, and the spawners there are already awake anyway
    // (measured: 310/310 awake before anything was enabled).
    const bool beaconWants = g_BeSpawnNear && BeaconActive();
    const bool revealWants = ForgePact::MapRevealManager::Instance().WantsPackSpawn();
    if ((!beaconWants && !revealWants) || !S) return;   // native pass-through
    try {
        RValue inst = S->ToRValue();
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("object_index") });
        if (!IsCreatorObject((int)oi.ToDouble())) return;

        // Never answer 0 to a creator that has not finished initialising: it
        // takes its spawn branch once, early, and comes out inert, leaving
        // the zone emptier than vanilla and permanently so (measured
        // 2026-09-11, docs/map-reveal-research.md).
        //
        // This is checked HERE, at the moment the result would be changed,
        // rather than at a frame boundary. EVENT_FRAME is dispatched from
        // HkPresent - the end of the frame - and creators run their step
        // events before that, so a window invalidated at Present is already
        // too late for the first call in a new zone (reported 2026-09-12).
        // The creator in hand is the only thing that can answer this
        // question at the only moment it matters.
        if (!ForgePact::MapRevealManager::CreatorIsReady(inst)) return;

        const bool revealOk = revealWants
            && ForgePact::MapRevealManager::Instance().MayPopulate(inst);
        if (!revealOk) {
            // Reveal declined (or was never asking). The Beacon's own lie is
            // unchanged, wake radius and all.
            if (!beaconWants) return;
            if (g_BeWakeRadius > 0.0) {
                RValue player; if (!HhResolveLocalPlayer(player)) return;
                const double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
                const double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
                const double cx = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
                const double cy = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
                const double dx = cx - px, dy = cy - py;
                if (dx * dx + dy * dy > g_BeWakeRadius * g_BeWakeRadius) return;
            }
        }
        Result = RValue(0.0);
        if (revealOk) ++g_RevealSpawnLies; else ++g_BeSpawnLies;
    } catch (...) {}
}
// Map-reveal's pack pass needs the same builtin detour the Beacon uses, but
// must not drag in the Beacon's hunt hooks (map-sized aggroRange, skipped
// leash) - revealing a map is not supposed to change how monsters behave.
// Both paths share one detour, installed at most once by whichever asks first.
static void InstallDistanceLieHook()
{
    if (g_DistanceHookAttempted) return;
    g_DistanceHookAttempted = true;
    HookBuiltin("distance_to_object", "fp_beacon_dist", (PVOID)Hook_distance_to_object, &g_OrigDistanceToObject);
}
static void InstallBeaconHook()
{
    if (g_BeHookAttempted) return;
    g_BeHookAttempted = true;
    InstallDistanceLieHook();
    bool a = HookOneScript("PathFindScanTick",   "fp_beacon_scan",  (PVOID)Hook_PathFindScanTick,   &g_Orig_PathFindScanTick);
    bool b = HookOneScript("PathFindLeashCheck", "fp_beacon_leash", (PVOID)Hook_PathFindLeashCheck, &g_Orig_PathFindLeashCheck);
    HookOneScript("ActivateDeactivateProps",      "fp_beacon_wake",  (PVOID)Hook_ActivateDeactivateProps,      &g_Orig_ActivateDeactivateProps);
    HookOneScript("LocalActivateDeactivateProps", "fp_beacon_wakel", (PVOID)Hook_LocalActivateDeactivateProps, &g_Orig_LocalActivateDeactivateProps);
    g_BeHookInstalled = a && b;
}

// ---- orb (globe) pickup radius --------------------------------------------
// One hook body for both globe types: run the game's own step first, then pull
// the globe toward the player if it is inside the widened radius.  Globes are
// few (tens on screen at most), so the four runner calls per globe per step are
// nothing like hooking distance_to_object, which every spawner calls.
// MEASURED 2026-09-10: hooking the globe step scripts does NOT work either -
// both hooks installed (exp=yes, mf=yes) and the body ran 0 times, so the globes'
// step logic is not dispatched through those script-table entries.  Two failed
// interception points is enough: the pull is now driven from the frame callback,
// which is known to run (the stall watchdog heartbeats prove it), by enumerating
// the globe instances directly.  Nothing about how the game dispatches globe
// behaviour matters any more.
static int g_PlayerObjIdx = -1;
static std::vector<int> g_GlobeObjIdx;
static bool g_OrbAssetsResolved = false;
static volatile long g_OrbGlobesSeen = 0;

// The name is what gets looked up at runtime, so an index shift after a game
// patch cannot break it; the SDK constant alongside makes a rename a build error.
struct OrbAsset { const char* name; HeroSiege::Objects::GameObject sdk; };
static constexpr OrbAsset kOrbAssets[] = {
    { "Experienceglobe_obj",        HeroSiege::Objects::GameObject::Experienceglobe_obj },
    { "Magic_Find_Globe_obj",       HeroSiege::Objects::GameObject::Magic_Find_Globe_obj },
};

static void ResolveOrbAssets()
{
    if (g_OrbAssetsResolved) return;
    g_OrbAssetsResolved = true;
    auto assetIndex = [](const char* name, HeroSiege::Objects::GameObject fallback) -> int {
        try {
            const int i = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue(std::string(name)) }).ToDouble();
            if (i >= 0) return i;
        } catch (...) {}
        return (int)fallback;   // name gone: the SDK's index is the best guess left
    };
    g_PlayerObjIdx = assetIndex("Player_obj", HeroSiege::Objects::GameObject::Player_obj);
    for (const OrbAsset& a : kOrbAssets) {
        const int i = assetIndex(a.name, a.sdk);
        if (i >= 0) g_GlobeObjIdx.push_back(i);
    }
}

static void PullOneGlobe(const RValue& inst)
{
    if (!g_PlayerPosValid.load()) { InterlockedIncrement(&g_OrbNoPlayer); return; }
    try {
        const double gx = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
        const double gy = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
        const double dx = g_PlayerX - gx, dy = g_PlayerY - gy;
        const double d2 = dx * dx + dy * dy;
        const double reach = kGlobeBaseRadius * kOrbPickupFactor;
        if (std::isfinite(d2)) {
            const long px = (long)std::sqrt(d2);
            if (g_OrbNearestPx < 0 || px < g_OrbNearestPx) g_OrbNearestPx = px;
        }
        if (d2 > reach * reach) { InterlockedIncrement(&g_OrbOutOfReach); return; }
        if (d2 < 1.0) return;                         // already on the player
        const double d = std::sqrt(d2);
        // Constant speed toward the player; clamp to 1.0 only so the last frame
        // lands exactly on the player instead of overshooting past it.
        const double t = (kGlobePullSpeed >= d) ? 1.0 : (kGlobePullSpeed / d);
        g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("x"), RValue(gx + dx * t) });
        g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("y"), RValue(gy + dy * t) });
        InterlockedIncrement(&g_OrbPickupHits);
    } catch (...) {}
}

// Called once per frame from FrameCallback while the mod is on.  Globes are few
// (tens on screen at worst), and the per-frame cost is one instance_number per
// globe object plus four calls per globe actually present.
static void OrbPickupTick()
{
    ResolveOrbAssets();
    if (g_GlobeObjIdx.empty()) return;
    int budget = 64;                       // a runaway globe count cannot cost a frame
    for (int objIdx : g_GlobeObjIdx) {
        int n = 0;
        try { n = (int)g_Yytk->CallBuiltin("instance_number", { RValue((double)objIdx) }).ToDouble(); }
        catch (...) { continue; }
        for (int i = 0; i < n && budget > 0; ++i, --budget) {
            try {
                RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)objIdx), RValue((double)i) });
                if (inst.m_Kind == VALUE_UNDEFINED) continue;
                InterlockedIncrement(&g_OrbGlobesSeen);
                PullOneGlobe(inst);
            } catch (...) {}
        }
    }
}

// Reads as a decision tree: seen=0 while standing next to globes means the object
// indices are wrong; seen>0 with noplayer>0 means the player never resolved;
// only outofreach means the radius is simply too small (nearest= says by how much).
static void OrbPickupStats()
{
    char b[300];
    sprintf_s(b, "orbpickup stat: globe objs=%zu | seen=%ld pulled=%ld noplayer=%ld outofreach=%ld | nearest=%ld px (reach %.0f px) | player via %s",
              g_GlobeObjIdx.size(),
              g_OrbGlobesSeen, g_OrbPickupHits, g_OrbNoPlayer, g_OrbOutOfReach,
              g_OrbNearestPx, kGlobeBaseRadius * kOrbPickupFactor, g_OrbPlayerHow.c_str());
    Out(b);
}

// ---- pet quest collector (scaffolding - see PetQuestCollectorMod.hpp) -----
// Read-only enumeration only: counts what the mod *would* act on, so
// `petquest stat` already reports real numbers before the collect call and
// accepted-quest gate exist. Nothing here mutates an instance, a quest, or
// an inventory - see the header's STATUS note for why.
static int g_QuestObjParentIdx = -1;
static int g_CompanionObjIdx = -1;
static int g_LootManagerObjIdx = -1;
static bool g_PetQuestAssetsResolved = false;

// Named in the plan (docs/pet-quest-collector-plan.md §11 risks) as members of
// the Quest_Object_Parent_obj family that are not carriable quest items - a
// civilian NPC and two scripted set-pieces. Confirmed present in the family
// via hs-game-sdk's generated hierarchy (2026-09-10). Everything else in the
// ~106-object family is left in per the plan's "no hand-built allowlist"
// finding; further exclusions get added only from observed breakage in a live
// session, not guessed here.
static constexpr HeroSiege::Objects::GameObject kPetQuestExcluded[] = {
    HeroSiege::Objects::GameObject::Civilian_NPC_obj,
    HeroSiege::Objects::GameObject::Boat_Quest_obj,
    HeroSiege::Objects::GameObject::Black_Hole_Quest_obj,
};

static volatile long g_PetQuestCandidatesSeen = 0;   // family instances enumerated this tick
static volatile long g_PetQuestOnScreen = 0;         // ...and inside the camera view
static volatile long g_PetQuestExcludedHits = 0;     // ...but on the static exclusion list
static volatile long g_PetQuestNoCam = 0;             // camera view unavailable this tick
static volatile long g_PetQuestPetSeen = 0;           // ticks where a Companion_obj instance existed
static volatile long g_PetQuestCollected = 0;         // collects actually invoked
static volatile long g_PetQuestRefusedGate = 0;       // skipped: canPickup false or lootType != 0
static volatile long g_PetQuestNoLootMgr = 0;         // skipped: no live Loot_Manager_obj
static volatile long g_PetQuestTargetLost = 0;        // target vanished while the pet was travelling
static volatile long g_PetQuestTravelTimeouts = 0;    // pet never reached the target

// ---- the confirmed collect call -------------------------------------------
// MEASURED 2026-09-11 on a real collect and then reproduced by this plugin:
// the quest counter advanced 7/15 -> 8/15. The game invokes the item's own
// m_Questpickup through the runtime's call-a-method-value helper:
//
//   void (CInstance* self, CInstance* other, RValue* result,
//         int argc, RValue* methodValue, RValue** args)
//
// with self = the item, other = Loot_Manager_obj, and one real argument.
// m_Questpickup then calls update_quest(questIndex, questObjectiveNumber,
// questValue) and QuestSaveUpdate, so the objective credit is inside the
// call - which is what makes this safe to drive, per the original plan's §11.
//
// This lives outside the research guard because the shipped mod needs it;
// `citrace collect` (dev only) routes through the same helper so there is one
// call shape in this file, not two.
//
// ---- HOW THE CALL IS REACHED --------------------------------------------
// CONFIRMED LIVE 2026-09-11: quest counter advanced, via `script_execute`.
//
// This took three attempts and the first two are worth keeping, because both
// failures were the same mistake wearing different clothes.
//
// 1. A fixed game address (exe+0xB489070, the runtime's call-a-method-value
//    dispatcher, read off one build's decompiled body), validated against
//    nothing. Shipped. That is the defect that already killed `relicgate`
//    (see SetRelicGate): a constant RVA names unrelated bytes the moment the
//    game is rebuilt, and here it would have transferred control into them
//    on a player's machine.
//
// 2. Reading the callable off the value's own CScriptRef - correct in
//    principle, since a GML method value normally *is* one, and it needs no
//    address. MEASURED, and wrong for this runner: `m_Questpickup` comes back
//    VALUE_OBJECT with m_ObjectKind = 0 (OBJECT_KIND_YYOBJECTBASE, not
//    SCRIPTREF), m_CallScript and m_CallYYC both reading 0, and
//    `method_get_index` returns nothing for it - while a sibling variable on
//    the same instance (`s_lootDrawData`) does resolve to script #105134.
//    This game's runner boxes these values in a shape YYToolkit's CScriptRef
//    does not describe. 309 refusals, 0 collects, 0 crashes.
//
//    Note what 1 and 2 have in common: a layout somebody wrote down was
//    trusted without a positive control on THIS target. An address is the
//    loud version of that error; a struct field is the quiet one.
//
// 3. What ships: let the runtime dispatch it. `script_execute`, resolved by
//    name, through CallBuiltinEx, which supplies self and other. Nothing
//    here depends on the value's shape or on any address - only on a builtin
//    the runner exposes by name, the same contract every other shipped
//    mechanism in this file already relies on. The call shape is the one
//    measured on a real collect and never changed across all three attempts:
//    self = the item, other = Loot_Manager_obj, one real argument.
//
// The CScriptRef path stays as a fallback for a runner where that layout
// does hold, reached only when route A fails to dispatch at all - never as a
// retry of a call that already ran, so "one item, one call" holds. It is
// fully validated before use (readable as a CScriptRef, kind is SCRIPTREF, a
// callable field landing in executable memory inside Hero_Siege.exe) and a
// failed proof refuses, bumps a counter, and logs one line naming the field
// that was wrong. Whether the method is bound to the instance we were handed
// is counted but deliberately NOT enforced - see MethodValueBindsTo.
//
// See agents.md, "Never Call an Address You Resolved by Hand".
static volatile long g_PetQuestNoMethodFn = 0;   // method value carried nothing callable (collect refused)
static volatile long g_PetQuestBadBind = 0;      // bound to a different instance (observed, not enforced)
static volatile long g_PetQuestNoEffect = 0;     // call dispatched and returned, but the item was still there
static CInstance* HhResolveInstance(const RValue& value);   // defined below; ships in both builds

// True when addr is committed, executable, and part of mod's loaded image.
// This cannot prove a pointer is the *right* function - nothing can, cheaply -
// but it does prove we are not about to jump into data, into another module,
// or into unmapped space, which is exactly how a stale fixed address fails.
static bool AddrIsExecutableInModule(HMODULE mod, const void* addr)
{
    if (!mod || !addr) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(addr, &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.AllocationBase != (PVOID)mod) return false;
    const DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ
                           | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (mbi.Protect & executable) != 0;
}

// Safe to read `bytes` at `p`? A wrong struct layout must produce a refusal
// and a log line, never an access violation inside a frame callback.
static bool ReadablePtr(const void* p, size_t bytes)
{
    return p && !IsBadReadPtr(p, bytes);
}

// Which check stopped a collect, so the log can name it instead of the mod
// going quiet. Kept as a value rather than a bool so the refusal message is
// specific enough to act on without another research session.
enum class MethodRefFault { None, NotAnObject, NotAScriptRef, Unreadable, NoCallable };

// The callable inside a GML method value, or nullptr - never a guess.
//
// FIXED 2026-09-11 (third pass). The first version took m_CallYYC and only
// consulted m_CallScript if m_CallYYC was *null* - so a non-null m_CallYYC
// that failed validation refused the whole call instead of trying the other
// field. It also had the preference backwards. m_CallScript's script function
// is the field this plugin already proves on this runtime: HookOneScript
// swaps exactly that pointer, and `citrace nativetrace` resolved
// m_Questpickup to exe+0x98732C0 through it. That is a positive control;
// m_CallYYC has none here. So: try the proven field first, validate each
// candidate independently, take the first that passes.
static PFUNC_YYGMLScript MethodValueFunction(const RValue& methodValue, MethodRefFault* whyOut = nullptr)
{
    auto fail = [&](MethodRefFault why) -> PFUNC_YYGMLScript {
        if (whyOut) *whyOut = why;
        return nullptr;
    };
    if (whyOut) *whyOut = MethodRefFault::None;
    if (methodValue.m_Kind != VALUE_OBJECT || !methodValue.m_Object) return fail(MethodRefFault::NotAnObject);
    if (!ReadablePtr(methodValue.m_Object, sizeof(CScriptRef))) return fail(MethodRefFault::Unreadable);
    if (methodValue.m_Object->m_ObjectKind != OBJECT_KIND_SCRIPTREF) return fail(MethodRefFault::NotAScriptRef);
    const CScriptRef* ref = reinterpret_cast<const CScriptRef*>(methodValue.m_Object);

    HMODULE game = GetModuleHandleA(nullptr);
    PFUNC_YYGMLScript candidates[2] = { nullptr, nullptr };
    if (ReadablePtr(ref->m_CallScript, sizeof(CScript)) && ReadablePtr(ref->m_CallScript->m_Functions, sizeof(YYGMLFuncs)))
        candidates[0] = ref->m_CallScript->m_Functions->m_ScriptFunction;
    candidates[1] = ref->m_CallYYC;
    for (PFUNC_YYGMLScript fn : candidates)
        if (fn && AddrIsExecutableInModule(game, (const void*)fn)) return fn;
    return fail(MethodRefFault::NoCallable);
}

// Whether the method is bound to `expected`. An m_BoundThis we cannot read as
// an instance is a binding we failed to inspect, not a wrong one.
//
// This is an OBSERVATION, not a gate - see InvokeMethodValue. It was written
// as a gate on the assumption that m_BoundThis is always the very CInstance
// we were handed, and that assumption has never been measured on this
// runtime.
static bool MethodValueBindsTo(const RValue& methodValue, CInstance* expected)
{
    if (methodValue.m_Kind != VALUE_OBJECT || !methodValue.m_Object) return false;
    if (!ReadablePtr(methodValue.m_Object, sizeof(CScriptRef))) return false;
    if (methodValue.m_Object->m_ObjectKind != OBJECT_KIND_SCRIPTREF) return false;
    const CScriptRef* ref = reinterpret_cast<const CScriptRef*>(methodValue.m_Object);
    const RValue& bound = ref->m_BoundThis;
    if (bound.m_Kind == VALUE_OBJECT && ReadablePtr(bound.m_Object, sizeof(YYObjectBase))
        && bound.m_Object->m_ObjectKind == OBJECT_KIND_CINSTANCE)
        return reinterpret_cast<CInstance*>(bound.m_Object) == expected;
    return true;
}

// One line, once per session, when a collect refuses on structural grounds.
// A mod that silently does nothing costs a research session to diagnose; a
// mod that says which field was wrong costs a read. Ships in both builds
// deliberately - this is the failure a future game or YYToolkit update will
// produce, and the player's own out.txt should explain it.
static volatile long g_MethodRefFaultLogged = 0;
static void LogMethodRefFaultOnce(const RValue& methodValue, MethodRefFault why)
{
    if (InterlockedCompareExchange(&g_MethodRefFaultLogged, 1, 0) != 0) return;
    const char* name = "?";
    switch (why) {
    case MethodRefFault::NotAnObject:   name = "the value is not an object (not a method at all)"; break;
    case MethodRefFault::Unreadable:    name = "the object is not readable as a CScriptRef"; break;
    case MethodRefFault::NotAScriptRef: name = "the object is not OBJECT_KIND_SCRIPTREF"; break;
    case MethodRefFault::NoCallable:    name = "neither m_CallScript's script function nor m_CallYYC is executable code inside Hero_Siege.exe"; break;
    default: name = "none"; break;
    }
    std::string msg = "petquest: COLLECT REFUSED - ";
    msg += name;
    msg += ". Nothing was called.";
    // The raw fields, so the next step is a read rather than a session.
    try {
        msg += " [kind=" + std::to_string((int)methodValue.m_Kind);
        if (methodValue.m_Kind == VALUE_OBJECT && ReadablePtr(methodValue.m_Object, sizeof(YYObjectBase))) {
            msg += " objectKind=" + std::to_string((int)methodValue.m_Object->m_ObjectKind);
            if (ReadablePtr(methodValue.m_Object, sizeof(CScriptRef))) {
                const CScriptRef* ref = reinterpret_cast<const CScriptRef*>(methodValue.m_Object);
                HMODULE game = GetModuleHandleA(nullptr);
                char b[192];
                PFUNC_YYGMLScript viaScript = nullptr;
                if (ReadablePtr(ref->m_CallScript, sizeof(CScript)) && ReadablePtr(ref->m_CallScript->m_Functions, sizeof(YYGMLFuncs)))
                    viaScript = ref->m_CallScript->m_Functions->m_ScriptFunction;
                sprintf_s(b, " m_CallScript=%p scriptFn=%p(inImage=%d) m_CallYYC=%p(inImage=%d) boundKind=%d",
                          (void*)ref->m_CallScript, (void*)viaScript,
                          AddrIsExecutableInModule(game, (const void*)viaScript) ? 1 : 0,
                          (void*)ref->m_CallYYC,
                          AddrIsExecutableInModule(game, (const void*)ref->m_CallYYC) ? 1 : 0,
                          (int)ref->m_BoundThis.m_Kind);
                msg += b;
            }
        }
        msg += "]";
    } catch (...) { msg += " [field dump failed]"; }
    Out(msg);
    Out("  This is a structural mismatch, not a missing address - see agents.md"
        " \"Never Call an Address You Resolved by Hand\" and re-check CScriptRef"
        " against the YYToolkit headers this was built with.");
}

// Which route actually made the last call, for `petquest stat`.
static const char* g_PetQuestPathUsed = "(none yet)";

// Route A is `script_execute` by name; route B is the value's own CScriptRef.
// See the HOW THE CALL IS REACHED block above for why that order, and for
// what was measured on this runner. CONFIRMED LIVE 2026-09-11 through route
// A: one collect, item removed, quest counter advanced.
static bool InvokeMethodValue(CInstance* selfInst, CInstance* otherInst, const RValue& methodValue,
                              const std::vector<RValue>& args, RValue& resultOut)
{
    if (!selfInst) return false;
    if (methodValue.m_Kind != VALUE_OBJECT || !methodValue.m_Object) {
        InterlockedIncrement(&g_PetQuestNoMethodFn);
        LogMethodRefFaultOnce(methodValue, MethodRefFault::NotAnObject);
        return false;
    }

    // --- Route A: the runtime's own dispatcher, reached by name ------------
    // script_execute(target, args...) - the method value goes first.
    {
        std::vector<RValue> callArgs;
        callArgs.reserve(args.size() + 1);
        callArgs.push_back(methodValue);
        for (const RValue& a : args) callArgs.push_back(a);
        RValue res;
        AurieStatus st = AURIE_EXTERNAL_ERROR;
        try { st = g_Yytk->CallBuiltinEx(res, "script_execute", selfInst, otherInst, callArgs); }
        catch (...) { st = AURIE_EXTERNAL_ERROR; }
        if (AurieSuccess(st)) {
            resultOut = res;
            g_PetQuestPathUsed = "script_execute (name-resolved, no layout)";
            return true;
        }
    }

    // --- Route B: the value's own CScriptRef, where the layout does hold ---
    // Only reached when route A could not dispatch at all - never as a retry
    // of a call that already ran, so "one item, one call" still holds.
    MethodRefFault why = MethodRefFault::None;
    PFUNC_YYGMLScript fn = MethodValueFunction(methodValue, &why);
    if (!fn) {
        InterlockedIncrement(&g_PetQuestNoMethodFn);
        LogMethodRefFaultOnce(methodValue, why);
        return false;
    }
    // The bind check is counted, not enforced. The property that protects the
    // process is "the pointer is executable code inside the game's image",
    // and that one does gate, above. "m_BoundThis is this exact CInstance" is
    // a consistency expectation nobody has measured on this runtime, and the
    // game's own call site supplies `self` explicitly anyway - so a mismatch
    // is worth knowing about, not worth refusing a measured-correct call for.
    if (!MethodValueBindsTo(methodValue, selfInst)) InterlockedIncrement(&g_PetQuestBadBind);
    // The callee copies what it needs onto its own stack and does not write
    // back through the argument pointers, so const_cast here is borrowing -
    // and it avoids RValue copy semantics on values the caller still owns.
    std::vector<RValue*> argPtrs;
    argPtrs.reserve(args.size());
    for (const RValue& a : args) argPtrs.push_back(const_cast<RValue*>(&a));
    fn(selfInst, otherInst, resultOut, (int)argPtrs.size(),
       argPtrs.empty() ? nullptr : argPtrs.data());
    g_PetQuestPathUsed = "CScriptRef callable (struct-resolved)";
    return true;
}

static void ResolvePetQuestAssets()
{
    if (g_PetQuestAssetsResolved) return;
    g_PetQuestAssetsResolved = true;
    auto assetIndex = [](const char* name, HeroSiege::Objects::GameObject fallback) -> int {
        try {
            const int i = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue(std::string(name)) }).ToDouble();
            if (i >= 0) return i;
        } catch (...) {}
        return (int)fallback;
    };
    g_QuestObjParentIdx = assetIndex("Quest_Object_Parent_obj", HeroSiege::Objects::GameObject::Quest_Object_Parent_obj);
    g_CompanionObjIdx = assetIndex("Companion_obj", HeroSiege::Objects::GameObject::Companion_obj);
    // `other` for the collect call. MEASURED: the game passes the
    // Loot_Manager_obj instance, not the player - so do the same rather than
    // substitute something convenient.
    g_LootManagerObjIdx = assetIndex("Loot_Manager_obj", HeroSiege::Objects::GameObject::Loot_Manager_obj);
}

static bool IsPetQuestExcluded(int objIdx)
{
    for (HeroSiege::Objects::GameObject excluded : kPetQuestExcluded) {
        if (objIdx == (int)excluded) return true;
    }
    return false;
}

// Called once per frame from FrameCallback while `petquest 1` is on. Mirrors
// OrbPickupTick's shape (per-family instance_number/instance_find loop under a
// bounded budget), but PullOneGlobe's mutation and PathFindStep-style call are
// deliberately not present: there is nothing yet proven safe to call on a
// quest item instance (see header STATUS). GameMaker's instance_number /
// instance_find return every instance of a parent object's descendants, not
// just exact matches - this is standard GameMaker family semantics, and is
// also plan item Phase 0.5's cheap cross-check once a live session can watch
// the counts below move.
// The pet fetches, then collects. One item at a time, as a small state
// machine, so the collect is something the player can watch happen rather
// than items silently vanishing across the screen:
//
//   Idle   -> pick the nearest eligible on-screen item, remember its id
//   Travel -> step the pet toward it each frame (same x/y write PullOneGlobe
//             uses for globes); on arrival, or on timeout, go to Collect
//   Collect-> re-read the game's own gates, invoke m_Questpickup, cool down
//
// Gates are re-read at collect time, never cached from selection: an item can
// stop being collectable during the two seconds the pet is walking over.
enum class PetQuestPhase { Idle, Travel };
static PetQuestPhase g_PetQuestPhase = PetQuestPhase::Idle;
static double g_PetQuestTargetId = -4.0;
static int    g_PetQuestTravelFrames = 0;
static int    g_PetQuestCooldown = 0;

static constexpr double kPetQuestSpeed = 11.0;        // px/frame; a brisk trot, not a teleport
static constexpr double kPetQuestArriveR = 26.0;      // close enough to read as "the pet is on it"
static constexpr int    kPetQuestTravelMax = 240;     // ~4s at 60fps before giving up on a target
static constexpr int    kPetQuestCooldownFrames = 24; // beat between collects, so it reads as fetching
// MEASURED: the real collect passed exactly this. What m_Questpickup does
// with it has not been read, and update_quest takes the credited amount from
// the item's own questValue rather than from here, so the safe default is to
// reproduce what was observed rather than to infer a meaning. Adjustable live
// (`petquest arg <n>`) so a multi-value item can be tested without a rebuild.
static std::atomic<double> g_PetQuestArg{ 1.0 };

// Both gates the game's own call site applies, re-read from the live instance.
// Driving m_Questpickup on an item the game would have skipped is precisely
// how an objective gets credited for something uncollectable.
static bool PetQuestItemIsCollectable(const RValue& inst)
{
    try {
        if (!g_Yytk->CallBuiltin("variable_instance_exists", { inst, RValue("m_Questpickup") }).ToBoolean()) return false;
        if (!g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("canPickup") }).ToBoolean()) return false;
        // lootType 0 is the m_Questpickup branch. The other types want a
        // different m_Quest* method and none of them is confirmed, so they are
        // left alone rather than guessed at.
        if (g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("lootType") }).ToDouble() != 0.0) return false;
        return true;
    } catch (...) { return false; }
}

static bool PetQuestCollectOne(const RValue& inst)
{
    try {
        if (!PetQuestItemIsCollectable(inst)) { InterlockedIncrement(&g_PetQuestRefusedGate); return false; }
        int lm = 0;
        try { lm = (int)g_Yytk->CallBuiltin("instance_number", { RValue((double)g_LootManagerObjIdx) }).ToDouble(); }
        catch (...) { lm = 0; }
        if (lm <= 0) { InterlockedIncrement(&g_PetQuestNoLootMgr); return false; }
        RValue lmInst = g_Yytk->CallBuiltin("instance_find", { RValue((double)g_LootManagerObjIdx), RValue(0.0) });
        CInstance* lootMgr = HhResolveInstance(lmInst);
        CInstance* item = HhResolveInstance(inst);
        if (!item || !lootMgr) { InterlockedIncrement(&g_PetQuestNoLootMgr); return false; }
        RValue method = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("m_Questpickup") });
        RValue result;
        std::vector<RValue> args{ RValue(g_PetQuestArg.load()) };
        if (!InvokeMethodValue(item, lootMgr, method, args, result)) return false;
        InterlockedIncrement(&g_PetQuestCollected);
        // Did the call actually do anything? m_Questpickup removes the item,
        // so an instance that is still there afterwards means the call
        // dispatched and returned without effect - the failure mode the plan
        // warned about for script_execute on a method value ("quietly no-ops
        // instead of rejecting"). This is NOT proof of objective credit (plan
        // §4 rule 3 still stands); it only separates "nothing ran" from "ran
        // and did nothing", which are otherwise identical from outside.
        try {
            if (g_Yytk->CallBuiltin("instance_exists", { inst }).ToBoolean())
                InterlockedIncrement(&g_PetQuestNoEffect);
        } catch (...) {}
        return true;
    } catch (...) { return false; }
}

static void PetQuestCollectorTick()
{
    ResolvePetQuestAssets();
    if (g_QuestObjParentIdx < 0) return;

    CInstance* pet = nullptr;
    RValue petInst;
    try {
        int n = 0;
        try { n = (int)g_Yytk->CallBuiltin("instance_number", { RValue((double)g_CompanionObjIdx) }).ToDouble(); }
        catch (...) { n = 0; }
        if (n > 0) {
            InterlockedIncrement(&g_PetQuestPetSeen);
            petInst = g_Yytk->CallBuiltin("instance_find", { RValue((double)g_CompanionObjIdx), RValue(0.0) });
        }
    } catch (...) {}
    // No pet out, no fetching. The mod is "the pet collects quest items", so
    // without one it stays a counter - which is also what stops it running in
    // menus and cutscenes.
    if (petInst.m_Kind == VALUE_UNDEFINED) { g_PetQuestPhase = PetQuestPhase::Idle; return; }

    if (g_PetQuestCooldown > 0) { --g_PetQuestCooldown; return; }

    double vx = 0, vy = 0, vw = 0, vh = 0;
    try {
        RValue cam = g_Yytk->CallBuiltin("view_get_camera", { RValue(0.0) });
        vx = g_Yytk->CallBuiltin("camera_get_view_x", { cam }).ToDouble();
        vy = g_Yytk->CallBuiltin("camera_get_view_y", { cam }).ToDouble();
        vw = g_Yytk->CallBuiltin("camera_get_view_width", { cam }).ToDouble();
        vh = g_Yytk->CallBuiltin("camera_get_view_height", { cam }).ToDouble();
        if (vw <= 0 || vh <= 0) { InterlockedIncrement(&g_PetQuestNoCam); return; }
    } catch (...) { InterlockedIncrement(&g_PetQuestNoCam); return; }

    // --- Travel: the pet is already on its way somewhere -------------------
    if (g_PetQuestPhase == PetQuestPhase::Travel) {
        RValue target = RValue(g_PetQuestTargetId);
        bool alive = false;
        try { alive = g_Yytk->CallBuiltin("instance_exists", { target }).ToBoolean(); } catch (...) {}
        if (!alive) {
            // These items despawn on their own (deleteTimer), so losing one
            // mid-walk is ordinary, not an error.
            InterlockedIncrement(&g_PetQuestTargetLost);
            g_PetQuestPhase = PetQuestPhase::Idle;
            return;
        }
        try {
            const double tx = g_Yytk->CallBuiltin("variable_instance_get", { target, RValue("x") }).ToDouble();
            const double ty = g_Yytk->CallBuiltin("variable_instance_get", { target, RValue("y") }).ToDouble();
            const double px = g_Yytk->CallBuiltin("variable_instance_get", { petInst, RValue("x") }).ToDouble();
            const double py = g_Yytk->CallBuiltin("variable_instance_get", { petInst, RValue("y") }).ToDouble();
            const double dx = tx - px, dy = ty - py;
            const double d2 = dx * dx + dy * dy;
            const bool arrived = (d2 <= kPetQuestArriveR * kPetQuestArriveR);
            if (++g_PetQuestTravelFrames > kPetQuestTravelMax) {
                // Straight-line movement should always arrive, so a timeout
                // means something is holding the pet (its own AI winning the
                // x/y tug-of-war, a teleport, a room change). Collect anyway:
                // the fetch animation is cosmetic, the credit is the point.
                InterlockedIncrement(&g_PetQuestTravelTimeouts);
                PetQuestCollectOne(target);
                g_PetQuestPhase = PetQuestPhase::Idle;
                g_PetQuestCooldown = kPetQuestCooldownFrames;
                return;
            }
            if (!arrived) {
                const double d = std::sqrt(d2);
                const double t = (kPetQuestSpeed >= d) ? 1.0 : (kPetQuestSpeed / d);
                g_Yytk->CallBuiltin("variable_instance_set", { petInst, RValue("x"), RValue(px + dx * t) });
                g_Yytk->CallBuiltin("variable_instance_set", { petInst, RValue("y"), RValue(py + dy * t) });
                return;
            }
            PetQuestCollectOne(target);
        } catch (...) {}
        g_PetQuestPhase = PetQuestPhase::Idle;
        g_PetQuestCooldown = kPetQuestCooldownFrames;
        return;
    }

    // --- Idle: choose the nearest eligible item on screen ------------------
    int budget = 64;   // a runaway quest-item count on screen cannot cost a frame
    int total = 0;
    try { total = (int)g_Yytk->CallBuiltin("instance_number", { RValue((double)g_QuestObjParentIdx) }).ToDouble(); }
    catch (...) { return; }

    double petX = 0, petY = 0;
    try {
        petX = g_Yytk->CallBuiltin("variable_instance_get", { petInst, RValue("x") }).ToDouble();
        petY = g_Yytk->CallBuiltin("variable_instance_get", { petInst, RValue("y") }).ToDouble();
    } catch (...) { return; }

    double bestD2 = -1.0, bestId = -4.0;
    for (int i = 0; i < total && budget > 0; ++i, --budget) {
        try {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)g_QuestObjParentIdx), RValue((double)i) });
            if (inst.m_Kind == VALUE_UNDEFINED) continue;
            InterlockedIncrement(&g_PetQuestCandidatesSeen);
            RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("object_index") });
            if (IsPetQuestExcluded((int)oi.ToDouble())) { InterlockedIncrement(&g_PetQuestExcludedHits); continue; }
            const double ix = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
            const double iy = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
            if (ix < vx || ix > vx + vw || iy < vy || iy > vy + vh) continue;
            InterlockedIncrement(&g_PetQuestOnScreen);
            if (!PetQuestItemIsCollectable(inst)) continue;
            const double dx = ix - petX, dy = iy - petY;
            const double d2 = dx * dx + dy * dy;
            if (bestD2 < 0.0 || d2 < bestD2) {
                bestD2 = d2;
                bestId = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("id") }).ToDouble();
            }
        } catch (...) {}
    }
    if (bestD2 < 0.0) return;
    g_PetQuestTargetId = bestId;
    g_PetQuestTravelFrames = 0;
    g_PetQuestPhase = PetQuestPhase::Travel;
}

static void PetQuestCollectorStats()
{
    char b[320];
    // Plan C C0.7: these counters have never been read in a session with quest
    // items actually on screen - doing so confirms the original plan's Phase 0
    // items 5 and 7 (family enumeration, camera-bounds stability) for free
    // while the rest of the C0 batch runs.
    sprintf_s(b, "petquest stat: family objs seen=%ld excluded=%ld on-screen=%ld | pet-seen ticks=%ld | no-camera ticks=%ld",
              g_PetQuestCandidatesSeen, g_PetQuestExcludedHits, g_PetQuestOnScreen,
              g_PetQuestPetSeen, g_PetQuestNoCam);
    Out(b);
    char c[320];
    sprintf_s(c, "  collected=%ld | skipped(gate)=%ld skipped(no Loot_Manager)=%ld | target lost=%ld travel timeouts=%ld | phase=%s arg=%.2f",
              g_PetQuestCollected, g_PetQuestRefusedGate, g_PetQuestNoLootMgr,
              g_PetQuestTargetLost, g_PetQuestTravelTimeouts,
              (g_PetQuestPhase == PetQuestPhase::Travel ? "travel" : "idle"), g_PetQuestArg.load());
    Out(c);
    // Structural refusals, reported separately from gameplay ones: these two
    // are the only counters that can mean "the runtime is not shaped the way
    // this build assumes". Nonzero here on a new game build is the signal to
    // re-check CScriptRef against the YYToolkit headers - not to go hunting
    // for an address.
    char e[288];
    sprintf_s(e, "  call route=%s | dispatched-but-item-remained=%ld", g_PetQuestPathUsed, g_PetQuestNoEffect);
    Out(e);
    if (g_PetQuestNoMethodFn || g_PetQuestBadBind) {
        char d[288];
        sprintf_s(d, "  REFUSED (structural): no callable on the method value=%ld (collect did NOT run)"
                     " | bound to another instance=%ld (counted only, the call still ran)"
                     " - the first refusal of the session is logged above with the raw fields",
                  g_PetQuestNoMethodFn, g_PetQuestBadBind);
        Out(d);
    }
}

// ---- interaction/pickup trace (Phase 0.1 research, dev build only) --------
// Answers the plan's single gating measurement: which script actually runs on
// a successful quest-item interaction, how often, and its Self/Other/argument
// context. MEASURED 2026-09-10: gml_Script_CheckPlayerInteraction, the name
// the plan assumed, is a false lead - a live session hovered + pressed F on a
// quest item and citrace's own counter stayed at 0 calls. These are the other
// UseKey-/Interact-/Pickup-shaped names already indexed in hs-game-sdk
// (LootBlocksUseKey's neighbors), traced together so one more session settles
// which one (if any) fires instead of guessing again one name at a time.
// Every hook always calls the original first and returns its result
// unchanged - this only observes, it never alters whether an interaction
// succeeds. Logged lines go to `out.txt` via Out() like every other research
// command. Never compiled into a player build.
#ifndef FORGEPACT_RELEASE
static std::atomic<bool> g_CiTraceOn{ false };
static volatile long g_CiLogged = 0;
static constexpr long kCiLogBudget = 300;   // caps out.txt growth over a long session

// MEASURED 2026-09-10 session 6: the original "#N" here is the *object
// type* index (asset_get_index), which every instance of that type shares -
// with several identical quest items on the ground at once, two different
// physical items were compared as if they were the same one. `@id` is
// GameMaker's actual per-instance unique id, appended so a reader can tell
// at a glance whether two log lines are really the same instance.
static std::string CiDescribeInstance(CInstance* inst)
{
    if (!inst) return "(null)";
    try {
        RValue r = inst->ToRValue();
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { r, RValue("object_index") });
        RValue nm = g_Yytk->CallBuiltin("object_get_name", { oi });
        RValue id = g_Yytk->CallBuiltin("variable_instance_get", { r, RValue("id") });
        return nm.ToString() + "#" + std::to_string((int)oi.ToDouble()) + "@" + std::to_string((long)id.ToDouble());
    } catch (...) { return "(unresolved)"; }
}

// Instance variables whose *names* look quest/interaction-shaped, dumped on
// both Self and Other whenever a traced script fires - same technique as the
// Beacon groundwork's AggroVars() above, aimed at Phase 0.4 (where a quest
// item instance carries its quest id) instead of monster AI state.
static std::string CiQuestVars(CInstance* inst)
{
    std::string line;
    if (!inst) return line;
    try {
        RValue id = inst->ToRValue();
        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { id });
        int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
        for (int i = 0; i < n; ++i) {
            RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            std::string s = nm.ToString(), ls = Lower(s);
            if (ls.find("quest") != std::string::npos || ls.find("interact") != std::string::npos
                || ls.find("collect") != std::string::npos || ls.find("pickup") != std::string::npos
                || ls.find("accept") != std::string::npos || ls.find("complete") != std::string::npos
                || ls.find("usekey") != std::string::npos || ls.find("hover") != std::string::npos) {
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, nm });
                std::string d = Describe(v); if (d.size() > 60) d = d.substr(0, 60) + "...";
                line += " " + s + "=" + d;
            }
        }
    } catch (...) { line += " (exc)"; }
    return line;
}

// Unfiltered variant of CiQuestVars - every keyword guess so far has missed,
// so this lists every instance variable name+value with no filter at all.
// Only ever called from the interact-key match below (an edge-triggered,
// rare event), so the cost of an unfiltered dump is bounded to that moment.
// CiExpandContainer is defined further below (it needs Describe() and this
// file's other helpers in a specific order); forward-declared here so
// CiAllVars can route through it too rather than the flatter Describe().
static std::string CiExpandContainer(const RValue& v);

static std::string CiAllVars(CInstance* inst)
{
    std::string line;
    if (!inst) return line;
    try {
        RValue id = inst->ToRValue();
        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { id });
        int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
        for (int i = 0; i < n; ++i) {
            RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            std::string s = nm.ToString();
            RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, nm });
            std::string d = CiExpandContainer(v); if (d.size() > 200) d = d.substr(0, 200) + "...";
            line += " " + s + "=" + d;
        }
    } catch (...) { line += " (exc)"; }
    return line;
}

// Defined further down alongside the rest of the Profile_Manager_obj/player
// identification helpers; forward-declared here so the snapshot/diff code
// above them in the file can use them.
static void ResolveCiProfileManagerIdx();
static void ResolveCiPlayerId();
static int CiGetProfileManagerObjIdx();

// ---- whole-state snapshot/diff (Phase 0 research, "shoot everywhere") -----
// Tester's suggestion 2026-09-10: rather than keep guessing which single
// function fires, capture everything observable right before the F-press and
// again right after, and print only what actually changed. Covers globals
// (the most likely place a "last interacted"/quest-progress flag would live)
// plus the player and Profile_Manager_obj instances (the two objects this
// session's hook trail has already implicated). A hooked script/builtin
// answers "was this called"; a diff answers "what changed" regardless of
// which of the many un-hooked call paths did it.
using CiSnapshot = std::unordered_map<std::string, std::string>;
static CiSnapshot g_CiSnapGlobalsBefore, g_CiSnapPlayerBefore, g_CiSnapPmBefore, g_CiSnapItemBefore;
static bool g_CiSnapTaken = false;
static double g_CiSnapRoomBefore = -1.0;   // MEASURED 2026-09-10: a room/zone change (or death/respawn)
                                            // between snap1 and snap2 recreates the player and every
                                            // per-zone instance - a real, observed confound, not a guess.
                                            // Checked so "item vanished" can be told apart from "the whole
                                            // room changed under us" instead of being misread as evidence.

// Describe() renders an array as just "array" and a ds_map/ds_list as a
// stable reference id (e.g. "kind=15 str=ref ds_map 41") - both hide exactly
// the kind of state a diff needs to see (quest progress plausibly lives
// inside a ds_map's *contents*, or in an input-state array's *elements*, not
// in a reassigned variable). This expands one level of either into the
// snapshot string instead of just the opaque handle. Capped at 40 entries and
// one level deep (no recursing into a nested container) so an unrelated giant
// structure elsewhere can't turn a snapshot into a multi-second stall.
// GameMaker's method_get_index()/script_get_name() are read-only - they
// answer "what does this bound method wrap" without invoking it. Added
// 2026-09-10 session 6 after the item dump turned up m_QuestInteract,
// m_QuestUseKey, m_QuestActivate, m_Questpickup, m_QuestActive,
// m_QuestDestructible as VALUE_OBJECT (bound-method-shaped) instance
// variables with no matching name anywhere in hs-game-sdk's static script
// search - exactly the "no separate script-table entry" pattern this file's
// prior hooks kept finding, except this time the method itself is sitting
// in a variable we can inspect directly instead of needing to guess and hook
// a name. Deliberately never calls the method - per the plan's own warning,
// an unverified call could remove a quest item without crediting progress.
static std::string CiTryResolveMethod(const RValue& v)
{
    if (v.m_Kind != VALUE_OBJECT) return "";
    try {
        RValue idx = g_Yytk->CallBuiltin("method_get_index", { v });
        if (idx.m_Kind == VALUE_UNDEFINED) return "";
        int scriptIdx = (int)idx.ToDouble();
        if (scriptIdx < 0) return "";
        try {
            RValue name = g_Yytk->CallBuiltin("script_get_name", { RValue((double)scriptIdx) });
            if (name.m_Kind == VALUE_STRING) return " ->method:" + name.ToString() + "#" + std::to_string(scriptIdx);
        } catch (...) {}
        return " ->method:#" + std::to_string(scriptIdx);
    } catch (...) { return ""; }
}

static std::string CiExpandContainer(const RValue& v)
{
    try {
        if (v.m_Kind == VALUE_ARRAY) {
            RValue n = g_Yytk->CallBuiltin("array_length", { v });
            int len = (int)n.ToDouble();
            std::string s = "array[" + std::to_string(len) + "]={";
            for (int i = 0; i < len && i < 100; ++i) {
                RValue el = g_Yytk->CallBuiltin("array_get", { v, RValue((double)i) });
                s += (i ? "," : "") + Describe(el);
            }
            if (len > 100) s += ",...";
            s += "}";
            return s;
        }
    } catch (...) { return "<array-expand-failed>"; }

    std::string base = Describe(v);
    if (base.find("ref ds_map ") != std::string::npos) {
        try {
            std::string s = "{";
            RValue key = g_Yytk->CallBuiltin("ds_map_find_first", { v });
            int n = 0;
            while (key.m_Kind != VALUE_UNDEFINED && n < 100) {
                RValue val = g_Yytk->CallBuiltin("ds_map_find_value", { v, key });
                s += (n ? "," : "") + Describe(key) + "=" + Describe(val);
                key = g_Yytk->CallBuiltin("ds_map_find_next", { v, key });
                ++n;
            }
            s += "}";
            return base + " " + s;
        } catch (...) { return base + " <map-expand-failed>"; }
    }
    if (base.find("ref ds_list ") != std::string::npos) {
        try {
            RValue sizeR = g_Yytk->CallBuiltin("ds_list_size", { v });
            int size = (int)sizeR.ToDouble();
            std::string s = "[";
            for (int i = 0; i < size && i < 100; ++i) {
                RValue el = g_Yytk->CallBuiltin("ds_list_find_value", { v, RValue((double)i) });
                s += (i ? "," : "") + Describe(el);
            }
            if (size > 100) s += ",...";
            s += "]";
            return base + " " + s;
        } catch (...) { return base + " <list-expand-failed>"; }
    }
    return base + CiTryResolveMethod(v);
}

// Same keyword set CiQuestVars uses, broadened per the tester's "collect more
// data than not enough" - used only to decide which *globals* get the
// (comparatively expensive) container expansion above, since doing that for
// all ~3500 globals unconditionally risks a real frame stall. Player/
// Profile_Manager_obj instance variables are few enough (~170 total) to
// always expand.
static bool CiNameLooksRelevant(const std::string& lowerName)
{
    static const char* kKeywords[] = {
        "quest", "interact", "collect", "pickup", "target", "hover",
        "usekey", "accept", "complete", "select", "active", "current", "near"
    };
    for (const char* k : kKeywords) if (lowerName.find(k) != std::string::npos) return true;
    return false;
}

static void CiSnapshotInstance(CInstance* inst, CiSnapshot& out)
{
    out.clear();
    if (!inst) return;
    try {
        RValue id = inst->ToRValue();
        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { id });
        int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
        for (int i = 0; i < n; ++i) {
            RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, nm });
            out[nm.ToString()] = CiExpandContainer(v);
        }
    } catch (...) {}
}

static void CiSnapshotGlobals(CiSnapshot& out)
{
    out.clear();
    CInstance* global = nullptr;
    AurieStatus st = g_Yytk->GetGlobalInstance(&global);
    if (!AurieSuccess(st) || !global) return;
    RValue globalrv = RValue(global);
    g_Yytk->EnumInstanceMembers(globalrv, [&](const char* name, RValue* val) -> bool {
        if (!name) return false;
        if (!val) { out[name] = "<null>"; return false; }
        bool expand = val->m_Kind == VALUE_ARRAY || CiNameLooksRelevant(Lower(name));
        out[name] = expand ? CiExpandContainer(*val) : Describe(*val);
        return false;   // keep enumerating everything
    });
}

// Prints only additions/removals/changes, capped so a huge unrelated churn
// (e.g. a timer ticking) cannot flood out.txt and bury the real signal.
static void CiDiffSnapshot(const std::string& label, const CiSnapshot& before, const CiSnapshot& after)
{
    constexpr int kMaxDiffLines = 60;
    int printed = 0;
    for (const auto& kv : after) {
        auto it = before.find(kv.first);
        if (it == before.end()) {
            Out("  [" + label + "] + " + kv.first + " = " + kv.second + " (NEW)");
            if (++printed >= kMaxDiffLines) { Out("  [" + label + "] ...(truncated)"); return; }
        } else if (it->second != kv.second) {
            Out("  [" + label + "] ~ " + kv.first + " : " + it->second + " -> " + kv.second);
            if (++printed >= kMaxDiffLines) { Out("  [" + label + "] ...(truncated)"); return; }
        }
    }
    for (const auto& kv : before) {
        if (after.find(kv.first) == after.end()) {
            Out("  [" + label + "] - " + kv.first + " (REMOVED, was " + kv.second + ")");
            if (++printed >= kMaxDiffLines) { Out("  [" + label + "] ...(truncated)"); return; }
        }
    }
    if (printed == 0) Out("  [" + label + "] (no changes)");
}

// CiSnapTake()/CiSnapDiff() are defined further down (after HhResolveInstance's
// real definition, which they need); forward-declared here so `citrace snap1`/
// `citrace snap2` in RunCommand can call them.
static void CiSnapTake();
static void CiSnapDiff();
// Phase C1 §3.2 step 1: defined far below with the rest of the C1 tooling,
// forward-declared here because Hook_Ci_KeyboardCheckPressed (above it in this
// file) is the one place it fires from.
static void CiCaptureStackWalk(const char* whatFired);
// Same reason: the dispatcher trace arms on the real F-press edge, so the
// capture window is the collect itself rather than the whole session.
static void CiDispatchArmOnPress();

#define CITRACE_HOOK(NAME) \
    static PFUNC_YYGMLScript g_OrigCi_##NAME = nullptr; \
    static volatile long g_CiCalls_##NAME = 0; \
    static RValue& Hook_Ci_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        RValue& r = g_OrigCi_##NAME ? g_OrigCi_##NAME(S, O, R, argc, A) : R; \
        InterlockedIncrement(&g_CiCalls_##NAME); \
        if (g_CiTraceOn.load() && g_CiLogged < kCiLogBudget) { \
            try { \
                std::string b = std::string("citrace " #NAME " #") + std::to_string(g_CiCalls_##NAME) \
                    + " self=" + CiDescribeInstance(S) + " other=" + CiDescribeInstance(O) \
                    + " argc=" + std::to_string(argc) + AggroArgs(argc, A) \
                    + " result=" + Describe(r) \
                    + " selfvars=[" + CiQuestVars(S) + " ] othervars=[" + CiQuestVars(O) + " ]"; \
                Out(b); \
                InterlockedIncrement(&g_CiLogged); \
            } catch (...) {} \
        } \
        return r; \
    }
// Same shape as CITRACE_HOOK, but takes the script's literal (hookable) name
// separately from the C++-identifier-safe token used to build variable names -
// needed for the anon@N@... closures below, whose real names contain '@'.
#define CITRACE_HOOK_NAMED(SAFE, SCRIPTNAME) \
    static PFUNC_YYGMLScript g_OrigCi_##SAFE = nullptr; \
    static volatile long g_CiCalls_##SAFE = 0; \
    static RValue& Hook_Ci_##SAFE(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        RValue& r = g_OrigCi_##SAFE ? g_OrigCi_##SAFE(S, O, R, argc, A) : R; \
        InterlockedIncrement(&g_CiCalls_##SAFE); \
        if (g_CiTraceOn.load() && g_CiLogged < kCiLogBudget) { \
            try { \
                std::string b = std::string("citrace " SCRIPTNAME " #") + std::to_string(g_CiCalls_##SAFE) \
                    + " self=" + CiDescribeInstance(S) + " other=" + CiDescribeInstance(O) \
                    + " argc=" + std::to_string(argc) + AggroArgs(argc, A) \
                    + " result=" + Describe(r) \
                    + " selfvars=[" + CiQuestVars(S) + " ] othervars=[" + CiQuestVars(O) + " ]"; \
                Out(b); \
                InterlockedIncrement(&g_CiLogged); \
            } catch (...) {} \
        } \
        return r; \
    }
#undef CITRACE_HOOK
#define CITRACE_HOOK(NAME) CITRACE_HOOK_NAMED(NAME, #NAME)
CITRACE_HOOK(CheckPlayerInteraction)
CITRACE_HOOK(CheckUseKey)
CITRACE_HOOK(PlayerInteracting)
CITRACE_HOOK(LootBlocksUseKey)
CITRACE_HOOK(PickupLoot)
// MEASURED 2026-09-10, session 4: `dump target`/`dump hover`/`dump mouse`
// against the live game's own globals (no rebuild needed for that step -
// DoDump() already existed) turned up these as the real hover/interact
// candidates, none of which were in any prior static name search here.
CITRACE_HOOK(GetMouseTarget)
CITRACE_HOOK(PlayerGetMouseTarget)
CITRACE_HOOK(GetMouseDisabledTarget)
CITRACE_HOOK(CanISeeTarget)
CITRACE_HOOK(PlayerMouseAction)
CITRACE_HOOK(GetPlayerMouseDisabled)
CITRACE_HOOK(KeyboardMouseInput)
CITRACE_HOOK(RefreshMouseMove)
CITRACE_HOOK(GetQuestHoverDescription)
#undef CITRACE_HOOK

// The plan's §3 "one chokepoint, not 106" finding, applied literally: these
// are the only script-table entries hs-game-sdk found anywhere inside
// Quest_Object_Parent_obj's own Create event - seven anonymous closures,
// presumably assigned to instance variables there and invoked later by
// whatever shared loot-interaction system also drives Coin_obj /
// Loot_Ground_obj. Traced together since MEASURED 2026-09-10 already ruled
// out all five named candidates above on a real collect (0 calls each).
CITRACE_HOOK_NAMED(Anon1400, "anon@1400@gml_Object_Quest_Object_Parent_obj_Create_0")
CITRACE_HOOK_NAMED(Anon1584, "anon@1584@gml_Object_Quest_Object_Parent_obj_Create_0")
CITRACE_HOOK_NAMED(Anon2113, "anon@2113@gml_Object_Quest_Object_Parent_obj_Create_0")
CITRACE_HOOK_NAMED(Anon2786, "anon@2786@gml_Object_Quest_Object_Parent_obj_Create_0")
CITRACE_HOOK_NAMED(Anon3858, "anon@3858@gml_Object_Quest_Object_Parent_obj_Create_0")
CITRACE_HOOK_NAMED(Anon4737, "anon@4737@gml_Object_Quest_Object_Parent_obj_Create_0")
CITRACE_HOOK_NAMED(Anon5164, "anon@5164@gml_Object_Quest_Object_Parent_obj_Create_0")
// MEASURED 2026-09-10, session 4: keyboard_check_pressed(70=F) fires with
// Self=Profile_Manager_obj, not the quest item or the player - so whatever
// this object does next with that "F just pressed" result is the real
// dispatch point. Same technique as the Quest_Object_Parent_obj anons: every
// script-table entry inside Profile_Manager_obj's own Create event.
CITRACE_HOOK_NAMED(PmAnon1940, "anon@1940@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon2426, "anon@2426@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon3331, "anon@3331@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon5032, "anon@5032@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon5174, "anon@5174@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon5646, "anon@5646@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon6344, "anon@6344@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon6662, "anon@6662@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon7920, "anon@7920@gml_Object_Profile_Manager_obj_Create_0")
CITRACE_HOOK_NAMED(PmAnon9698, "anon@9698@gml_Object_Profile_Manager_obj_Create_0")
// MEASURED 2026-09-10, session 7: HookOneScript (used for every hook above)
// always prepends "gml_Script_" - correct for script assets and the anon
// closures above, but an object's own built-in event code is named
// "gml_Object_<ObjName>_<Event>_<N>" with no such prefix, so it was never
// actually reachable through any hook this file installed before now. These
// are installed via HookRawNamedRoutine (no prefix added) instead of
// HookOneScript - see InstallCiTraceHooks. Covers the built-in events most
// likely to run hover/interact detection: Step (continuous per-frame check),
// Destroy (cleanup), and the full Mouse event set (0=left btn, 1=right,
// 2=middle, 3=no button/hover, 4-6=press, 7-9=release, 10=enter, 11=leave -
// "no button"/enter/leave are exactly what continuous hover detection would
// use), plus the first few Alarms.
CITRACE_HOOK_NAMED(QoStep, "gml_Object_Quest_Object_Parent_obj_Step_0")
CITRACE_HOOK_NAMED(QoDestroy, "gml_Object_Quest_Object_Parent_obj_Destroy_0")
CITRACE_HOOK_NAMED(QoCreate, "gml_Object_Quest_Object_Parent_obj_Create_0")
CITRACE_HOOK_NAMED(QoMouse0, "gml_Object_Quest_Object_Parent_obj_Mouse_0")
CITRACE_HOOK_NAMED(QoMouse1, "gml_Object_Quest_Object_Parent_obj_Mouse_1")
CITRACE_HOOK_NAMED(QoMouse2, "gml_Object_Quest_Object_Parent_obj_Mouse_2")
CITRACE_HOOK_NAMED(QoMouse3, "gml_Object_Quest_Object_Parent_obj_Mouse_3")
CITRACE_HOOK_NAMED(QoMouse4, "gml_Object_Quest_Object_Parent_obj_Mouse_4")
CITRACE_HOOK_NAMED(QoMouse5, "gml_Object_Quest_Object_Parent_obj_Mouse_5")
CITRACE_HOOK_NAMED(QoMouse6, "gml_Object_Quest_Object_Parent_obj_Mouse_6")
CITRACE_HOOK_NAMED(QoMouse7, "gml_Object_Quest_Object_Parent_obj_Mouse_7")
CITRACE_HOOK_NAMED(QoMouse8, "gml_Object_Quest_Object_Parent_obj_Mouse_8")
CITRACE_HOOK_NAMED(QoMouse9, "gml_Object_Quest_Object_Parent_obj_Mouse_9")
CITRACE_HOOK_NAMED(QoMouse10, "gml_Object_Quest_Object_Parent_obj_Mouse_10")
CITRACE_HOOK_NAMED(QoMouse11, "gml_Object_Quest_Object_Parent_obj_Mouse_11")
CITRACE_HOOK_NAMED(QoAlarm0, "gml_Object_Quest_Object_Parent_obj_Alarm_0")
CITRACE_HOOK_NAMED(QoAlarm1, "gml_Object_Quest_Object_Parent_obj_Alarm_1")
CITRACE_HOOK_NAMED(QoAlarm2, "gml_Object_Quest_Object_Parent_obj_Alarm_2")
CITRACE_HOOK_NAMED(QoAlarm3, "gml_Object_Quest_Object_Parent_obj_Alarm_3")
CITRACE_HOOK_NAMED(PmStep, "gml_Object_Profile_Manager_obj_Step_0")
CITRACE_HOOK_NAMED(PlayerStep, "gml_Object_Player_obj_Step_0")
CITRACE_HOOK_NAMED(PlayerMouse3, "gml_Object_Player_obj_Mouse_3")
#undef CITRACE_HOOK_NAMED

// ---- builtin variant (plan B2, hooking the interaction gate rather than a
// named script) - MEASURED 2026-09-10: all 12 script-table hooks above
// stayed at 0 calls across 3 live collects, so the check is not dispatched
// through any named script hs-game-sdk's static search can find. This is the
// one remaining untried path before the plan's own re-scoping exit criterion:
// hook the builtins that a hover+press check would plausibly call directly.
// `keyboard_check_pressed` / `mouse_check_button_pressed` are far hotter than
// distance_to_object (many unrelated systems poll keys/buttons every frame),
// so unlike the named-script hooks above this does NOT log unconditionally -
// see CiKeyLooksLikeInteract below for the filter.
static TRoutine g_OrigCi_KeyboardCheckPressed = nullptr;
static TRoutine g_OrigCi_MouseCheckButtonPressed = nullptr;
static volatile long g_CiCalls_KeyboardCheckPressed = 0;
static volatile long g_CiCalls_MouseCheckButtonPressed = 0;
static constexpr long kCiBuiltinFirstN = 5;   // always logged, so a wrong key-code guess is still visible

// GameMaker's ord("f")/ord("F")/ord("e")/ord("E") plus vk_enter/vk_space -
// the plausible interact-key bindings this game would use for hover+press.
static bool CiKeyLooksLikeInteract(double key)
{
    int k = (int)key;
    return k == 69 || k == 70 || k == 101 || k == 102 || k == 13 || k == 32;
}

static void Hook_Ci_KeyboardCheckPressed(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigCi_KeyboardCheckPressed) g_OrigCi_KeyboardCheckPressed(Result, S, O, argc, Args);
    long n = InterlockedIncrement(&g_CiCalls_KeyboardCheckPressed);
    // Phase C1 §3.2 step 1. Deliberately ahead of the g_CiTraceOn/log-budget
    // gate below: an armed stack walk is a separate, explicitly counted-down
    // request and must not be silently swallowed because tracing was toggled
    // off or the shared 300-line trace budget happened to be spent. It has its
    // own arm counter, so this costs one atomic load per call when idle.
    if (argc > 0 && CiKeyLooksLikeInteract(Args[0].ToDouble()) && Result.ToDouble() != 0.0) {
        CiCaptureStackWalk("keyboard_check_pressed(70) returned pressed");
        CiDispatchArmOnPress();
    }
    if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget) return;
    bool keyMatch = argc > 0 && CiKeyLooksLikeInteract(Args[0].ToDouble());
    bool interesting = (n <= kCiBuiltinFirstN) || keyMatch;
    if (!interesting) return;
    try {
        std::string b = std::string("citrace keyboard_check_pressed #") + std::to_string(n)
            + " self=" + CiDescribeInstance(S) + " key=" + (argc > 0 ? Describe(Args[0]) : "?") + " result=" + Describe(Result);
        // MEASURED 2026-09-10, session 5: no CiQuestVars keyword (quest/
        // interact/collect/pickup/accept/complete/usekey/hover) matched
        // anything useful on this Self across two prior sessions. On the
        // actual "F just pressed" edge (Result==1, not every recheck), dump
        // every instance variable unfiltered instead of guessing more
        // keywords - this fires once, not every frame.
        if (keyMatch && Result.ToDouble() != 0.0) {
            b += " ALLVARS=[" + CiAllVars(S) + " ]";
        }
        Out(b);
        InterlockedIncrement(&g_CiLogged);
    } catch (...) {}
}

static void Hook_Ci_MouseCheckButtonPressed(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigCi_MouseCheckButtonPressed) g_OrigCi_MouseCheckButtonPressed(Result, S, O, argc, Args);
    long n = InterlockedIncrement(&g_CiCalls_MouseCheckButtonPressed);
    if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget || n > kCiBuiltinFirstN) return;
    try {
        std::string b = std::string("citrace mouse_check_button_pressed #") + std::to_string(n)
            + " self=" + CiDescribeInstance(S) + " button=" + (argc > 0 ? Describe(Args[0]) : "?") + " result=" + Describe(Result);
        Out(b);
        InterlockedIncrement(&g_CiLogged);
    } catch (...) {}
}

// MEASURED 2026-09-10, session 5: since Profile_Manager_obj is confirmed to
// be Self at the moment F is detected (session 4), the next-cheapest lead is
// whatever spatial/collision builtin it calls next to find what's under the
// cursor - not another named-script guess. Filtered to Self==Profile_Manager_obj
// specifically (same per-call cost precedent as Hook_distance_to_object,
// which already pays this on an even hotter builtin) so the cost for every
// OTHER call in the game stays a single cheap object_index compare.
static int g_CiProfileManagerObjIdx = -1;
static bool g_CiProfileManagerIdxResolved = false;
static void ResolveCiProfileManagerIdx()
{
    if (g_CiProfileManagerIdxResolved) return;
    g_CiProfileManagerIdxResolved = true;
    try {
        g_CiProfileManagerObjIdx = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue(std::string("Profile_Manager_obj")) }).ToDouble();
    } catch (...) {}
}
static int CiGetProfileManagerObjIdx() { ResolveCiProfileManagerIdx(); return g_CiProfileManagerObjIdx; }
static bool CiSelfIsProfileManager(CInstance* S)
{
    if (!S || g_CiProfileManagerObjIdx < 0) return false;
    try {
        RValue r = S->ToRValue();
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { r, RValue("object_index") });
        return (int)oi.ToDouble() == g_CiProfileManagerObjIdx;
    } catch (...) { return false; }
}

// MEASURED 2026-09-10, session 5 continued: Profile_Manager_obj turned out to
// be the game's generic input/profile abstraction layer (gamePadEnabled,
// keyBinds, inputState, mouse_x_prev/mouse_y_prev, profileNumber,
// platformOnlineAccountId, ...) - none of its ~50 instance variables mention
// quest/target/hover/nearest anything, and all 6 filtered builtins below
// stayed at 0 for it too, so it does no position/hover checking itself. The
// next candidate by the same reasoning: the player's own object, on the
// theory that it reads Profile_Manager_obj's processed input state
// (pressedArray/inputState) in its own Step event and does the hover+collect
// check inline there - the same "no separate script-table entry" pattern
// every prior candidate in this file has shown.
static double g_CiPlayerId = -1.0;
static void ResolveCiPlayerId()
{
    try {
        RValue player;
        if (HhResolveLocalPlayer(player)) {
            RValue id = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("id") });
            g_CiPlayerId = id.ToDouble();
        }
    } catch (...) {}
}
static bool CiSelfIsPlayer(CInstance* S)
{
    if (!S || g_CiPlayerId < 0) return false;
    try {
        RValue r = S->ToRValue();
        RValue id = g_Yytk->CallBuiltin("variable_instance_get", { r, RValue("id") });
        return id.ToDouble() == g_CiPlayerId;
    } catch (...) { return false; }
}

// A builtin can only be detoured once, so Profile_Manager_obj and the player
// share a single hook per builtin (each logs which of the two Self actually
// was, via CiDescribeInstance) rather than two separate hook installs
// fighting over the same native routine.
#define CITRACE_BUILTIN_PM(SAFE, NAME) \
    static TRoutine g_OrigCi_##SAFE = nullptr; \
    static volatile long g_CiCalls_##SAFE = 0; \
    static void Hook_Ci_##SAFE(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args) { \
        if (g_OrigCi_##SAFE) g_OrigCi_##SAFE(Result, S, O, argc, Args); \
        if (!CiSelfIsProfileManager(S) && !CiSelfIsPlayer(S)) return; \
        long n = InterlockedIncrement(&g_CiCalls_##SAFE); \
        if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget) return; \
        try { \
            std::string args; \
            for (int i = 0; i < argc && i < 6; ++i) args += (i ? ", " : "") + Describe(Args[i]); \
            /* MEASURED 2026-09-10: the player calls these constantly against */ \
            /* Enemy_Parent_obj/Collision_Parent_obj for ordinary movement    */ \
            /* collision - still counted above, but not logged, so it cannot  */ \
            /* burn the shared budget before a genuinely new match shows up.  */ \
            if (args.find("Enemy_Parent_obj") != std::string::npos || args.find("Collision_Parent_obj") != std::string::npos) return; \
            std::string b = std::string("citrace " NAME " #") + std::to_string(n) + " self=" + CiDescribeInstance(S) + " args=[" + args + "] result=" + Describe(Result); \
            Out(b); \
            InterlockedIncrement(&g_CiLogged); \
        } catch (...) {} \
    }
CITRACE_BUILTIN_PM(InstancePosition, "instance_position")
CITRACE_BUILTIN_PM(InstancePlace, "instance_place")
CITRACE_BUILTIN_PM(CollisionPoint, "collision_point")
CITRACE_BUILTIN_PM(PointInRectangle, "point_in_rectangle")
CITRACE_BUILTIN_PM(DistanceToPoint, "distance_to_point")
CITRACE_BUILTIN_PM(InstanceNearest, "instance_nearest")
CITRACE_BUILTIN_PM(PointInCircle, "point_in_circle")
CITRACE_BUILTIN_PM(PositionMeeting, "position_meeting")
#undef CITRACE_BUILTIN_PM

// We already know quest items get destroyed on collect (plan §2). This
// catches the exact moment and its Self/args context - cheap and rare
// (nowhere near variable_instance_set's frequency, which was considered and
// skipped: this session's own logs already show one 3-second frame stall
// unrelated to citrace, and doubling the interpreter cost of every single
// instance-variable write in the game is a real risk of causing another one
// mid-session, not just a slow diagnostic).
static bool CiInstanceIsQuestObject(CInstance* inst)
{
    if (!inst) return false;
    try {
        RValue r = inst->ToRValue();
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { r, RValue("object_index") });
        return HeroSiege::Objects::IsDescendantOf((int32_t)oi.ToDouble(), (int32_t)HeroSiege::Objects::GameObject::Quest_Object_Parent_obj);
    } catch (...) { return false; }
}
// instance_deactivate_object(target) accepts EITHER a raw object/asset index
// (a plain number - deactivates every instance of that type) OR a specific
// instance reference (deactivates just that one), so this checks the raw
// numeric value against the quest family first before falling back to
// resolving it as an instance's object_index.
static bool CiRValueIsQuestObject(const RValue& v)
{
    try {
        if (v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 || v.m_Kind == VALUE_INT64) {
            if (HeroSiege::Objects::IsDescendantOf((int32_t)v.ToDouble(), (int32_t)HeroSiege::Objects::GameObject::Quest_Object_Parent_obj)) return true;
        }
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { v, RValue("object_index") });
        return HeroSiege::Objects::IsDescendantOf((int32_t)oi.ToDouble(), (int32_t)HeroSiege::Objects::GameObject::Quest_Object_Parent_obj);
    } catch (...) { return false; }
}
static TRoutine g_OrigCi_InstanceDestroy = nullptr;
static volatile long g_CiCalls_InstanceDestroy = 0;
static void Hook_Ci_InstanceDestroy(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigCi_InstanceDestroy) g_OrigCi_InstanceDestroy(Result, S, O, argc, Args);
    bool selfIsQuest = CiInstanceIsQuestObject(S);
    bool argIsQuest = argc > 0 && CiRValueIsQuestObject(Args[0]);
    if (!selfIsQuest && !argIsQuest) return;
    long n = InterlockedIncrement(&g_CiCalls_InstanceDestroy);
    if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget) return;
    try {
        std::string args;
        for (int i = 0; i < argc && i < 4; ++i) args += (i ? ", " : "") + Describe(Args[i]);
        std::string b = std::string("citrace instance_destroy #") + std::to_string(n) + " self=" + CiDescribeInstance(S) + " args=[" + args + "]";
        Out(b);
        InterlockedIncrement(&g_CiLogged);
    } catch (...) {}
}

// MEASURED 2026-09-10 session 6: after a real collect, the item vanished
// from instance_number/instance_find entirely (the "nearest quest item"
// search came up empty) while instance_destroy stayed at 0 calls. GameMaker
// excludes *deactivated* instances from those same enumeration builtins
// while keeping them alive in memory - the signature this measurement
// actually matches, not destruction. instance_deactivate_object(target) is
// GameMaker's single-instance form of that (passing an instance id
// deactivates just it; passing an object index deactivates the whole type -
// distinguished by CiRValueIsQuestObject above). instance_deactivate_all is
// hooked too, filtered to Self, in case deactivation is issued as an "all
// except this widget" broadcast from inside the item's own code rather than
// a single-target call.
static TRoutine g_OrigCi_InstanceDeactivateObject = nullptr;
static volatile long g_CiCalls_InstanceDeactivateObject = 0;
static void Hook_Ci_InstanceDeactivateObject(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigCi_InstanceDeactivateObject) g_OrigCi_InstanceDeactivateObject(Result, S, O, argc, Args);
    bool selfIsQuest = CiInstanceIsQuestObject(S);
    bool argIsQuest = argc > 0 && CiRValueIsQuestObject(Args[0]);
    if (!selfIsQuest && !argIsQuest) return;
    long n = InterlockedIncrement(&g_CiCalls_InstanceDeactivateObject);
    if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget) return;
    try {
        std::string args;
        for (int i = 0; i < argc && i < 4; ++i) args += (i ? ", " : "") + Describe(Args[i]);
        std::string b = std::string("citrace instance_deactivate_object #") + std::to_string(n) + " self=" + CiDescribeInstance(S) + " args=[" + args + "]";
        Out(b);
        InterlockedIncrement(&g_CiLogged);
    } catch (...) {}
}
static TRoutine g_OrigCi_InstanceDeactivateAll = nullptr;
static volatile long g_CiCalls_InstanceDeactivateAll = 0;
static void Hook_Ci_InstanceDeactivateAll(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigCi_InstanceDeactivateAll) g_OrigCi_InstanceDeactivateAll(Result, S, O, argc, Args);
    if (!CiInstanceIsQuestObject(S)) return;
    long n = InterlockedIncrement(&g_CiCalls_InstanceDeactivateAll);
    if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget) return;
    try {
        std::string args;
        for (int i = 0; i < argc && i < 4; ++i) args += (i ? ", " : "") + Describe(Args[i]);
        std::string b = std::string("citrace instance_deactivate_all #") + std::to_string(n) + " self=" + CiDescribeInstance(S) + " args=[" + args + "]";
        Out(b);
        InterlockedIncrement(&g_CiLogged);
    } catch (...) {}
}

// instance_destroy and both deactivate builtins all measured 0 calls on a
// genuine collect (session 6, three separate rounds), yet the item still
// vanished from the family enumeration. instance_change(obj, perform_events)
// morphs the calling instance into a different object type in place, without
// destroying it - it would leave the object still alive but no longer a
// Quest_Object_Parent_obj descendant, explaining the same disappearance
// through a mechanism not yet tried. Filtered to Self, since instance_change
// acts on the calling instance itself, not an explicit target argument.
static TRoutine g_OrigCi_InstanceChange = nullptr;
static volatile long g_CiCalls_InstanceChange = 0;
static void Hook_Ci_InstanceChange(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    // Captured BEFORE the original runs - S's own object_index changes as a
    // side effect of this exact builtin, so both the filter and the logged
    // description must be taken first or they'd describe the post-change type.
    bool selfIsQuest = CiInstanceIsQuestObject(S);
    std::string selfBefore = CiDescribeInstance(S);
    if (g_OrigCi_InstanceChange) g_OrigCi_InstanceChange(Result, S, O, argc, Args);
    if (!selfIsQuest) return;
    long n = InterlockedIncrement(&g_CiCalls_InstanceChange);
    if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget) return;
    try {
        std::string args;
        for (int i = 0; i < argc && i < 4; ++i) args += (i ? ", " : "") + Describe(Args[i]);
        std::string b = std::string("citrace instance_change #") + std::to_string(n) + " self(before)=" + selfBefore
            + " self(after)=" + CiDescribeInstance(S) + " args=[" + args + "]";
        Out(b);
        InterlockedIncrement(&g_CiLogged);
    } catch (...) {}
}

// Last candidate, added only at the tester's explicit request after every
// safer mechanism above measured 0 calls on a genuine collect:
// variable_instance_set could flip object_index (or a marker flag) directly,
// bypassing instance_change/deactivate/destroy entirely. This is one of the
// hottest builtins in the whole engine (every object's every per-frame field
// write goes through it), so unlike every other hook above, this does NOT
// resolve the target instance for every call - it first checks the variable
// *name* being written against a small fixed set with a plain string
// comparison (no interpreter round-trip), and only resolves the target
// instance's object_index for the rare calls that already matched a name.
// That keeps the added cost for the other ~100% of instance-variable writes
// in the entire game at one string compare, not a second CallBuiltin.
static bool CiVarNameLooksLikeCollectMarker(const RValue& nameArg)
{
    if (nameArg.m_Kind != VALUE_STRING) return false;
    const std::string& s = nameArg.ToString();
    return s == "object_index" || s == "canPickup" || s == "isActive" || s == "active"
        || s == "questObjectiveNumber" || s == "distanceForPickup" || s == "activateQuestObjectWithMouse"
        || s == "itemActive" || s == "questIndex";
}
static TRoutine g_OrigCi_VariableInstanceSet = nullptr;
static volatile long g_CiCalls_VariableInstanceSet = 0;
static void Hook_Ci_VariableInstanceSet(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigCi_VariableInstanceSet) g_OrigCi_VariableInstanceSet(Result, S, O, argc, Args);
    if (argc < 3 || !CiVarNameLooksLikeCollectMarker(Args[1])) return;   // pure string compare, no CallBuiltin - the hot-path cost
    if (!CiRValueIsQuestObject(Args[0])) return;
    long n = InterlockedIncrement(&g_CiCalls_VariableInstanceSet);
    if (!g_CiTraceOn.load() || g_CiLogged >= kCiLogBudget) return;
    try {
        std::string b = std::string("citrace variable_instance_set #") + std::to_string(n)
            + " target=" + Describe(Args[0]) + " name=" + Describe(Args[1]) + " value=" + Describe(Args[2])
            + " caller=" + CiDescribeInstance(S);
        Out(b);
        InterlockedIncrement(&g_CiLogged);
    } catch (...) {}
}

static bool g_CiHooksInstalled = false;
static void InstallCiTraceHooks()
{
    if (g_CiHooksInstalled) return;
    g_CiHooksInstalled = true;
    ResolveCiProfileManagerIdx();
    ResolveCiPlayerId();
    HookBuiltin("instance_position",    "fp_ci_ip",  (PVOID)Hook_Ci_InstancePosition,   &g_OrigCi_InstancePosition);
    HookBuiltin("instance_place",       "fp_ci_ipl", (PVOID)Hook_Ci_InstancePlace,      &g_OrigCi_InstancePlace);
    HookBuiltin("collision_point",      "fp_ci_cp",  (PVOID)Hook_Ci_CollisionPoint,     &g_OrigCi_CollisionPoint);
    HookBuiltin("point_in_rectangle",   "fp_ci_pir", (PVOID)Hook_Ci_PointInRectangle,   &g_OrigCi_PointInRectangle);
    HookBuiltin("distance_to_point",    "fp_ci_dtp", (PVOID)Hook_Ci_DistanceToPoint,    &g_OrigCi_DistanceToPoint);
    HookBuiltin("instance_nearest",     "fp_ci_in",  (PVOID)Hook_Ci_InstanceNearest,    &g_OrigCi_InstanceNearest);
    HookBuiltin("point_in_circle",      "fp_ci_pic", (PVOID)Hook_Ci_PointInCircle,      &g_OrigCi_PointInCircle);
    HookBuiltin("position_meeting",     "fp_ci_pmt", (PVOID)Hook_Ci_PositionMeeting,    &g_OrigCi_PositionMeeting);
    HookBuiltin("instance_destroy",     "fp_ci_id",  (PVOID)Hook_Ci_InstanceDestroy,    &g_OrigCi_InstanceDestroy);
    HookBuiltin("instance_deactivate_object", "fp_ci_ido", (PVOID)Hook_Ci_InstanceDeactivateObject, &g_OrigCi_InstanceDeactivateObject);
    HookBuiltin("instance_deactivate_all",    "fp_ci_ida", (PVOID)Hook_Ci_InstanceDeactivateAll,    &g_OrigCi_InstanceDeactivateAll);
    HookBuiltin("instance_change",            "fp_ci_ic",  (PVOID)Hook_Ci_InstanceChange,           &g_OrigCi_InstanceChange);
    HookBuiltin("variable_instance_set",      "fp_ci_vis", (PVOID)Hook_Ci_VariableInstanceSet,      &g_OrigCi_VariableInstanceSet);
    HookOneScriptTable("CheckPlayerInteraction", "fp_ci_cpi",  (PVOID)Hook_Ci_CheckPlayerInteraction, &g_OrigCi_CheckPlayerInteraction);
    HookOneScriptTable("CheckUseKey",            "fp_ci_cuk",  (PVOID)Hook_Ci_CheckUseKey,            &g_OrigCi_CheckUseKey);
    HookOneScriptTable("PlayerInteracting",      "fp_ci_pi",   (PVOID)Hook_Ci_PlayerInteracting,      &g_OrigCi_PlayerInteracting);
    HookOneScriptTable("LootBlocksUseKey",       "fp_ci_lbuk", (PVOID)Hook_Ci_LootBlocksUseKey,       &g_OrigCi_LootBlocksUseKey);
    HookOneScriptTable("PickupLoot",             "fp_ci_pl",   (PVOID)Hook_Ci_PickupLoot,             &g_OrigCi_PickupLoot);
    HookOneScriptTable("GetMouseTarget",         "fp_ci_gmt",  (PVOID)Hook_Ci_GetMouseTarget,         &g_OrigCi_GetMouseTarget);
    HookOneScriptTable("PlayerGetMouseTarget",   "fp_ci_pgmt", (PVOID)Hook_Ci_PlayerGetMouseTarget,   &g_OrigCi_PlayerGetMouseTarget);
    HookOneScriptTable("GetMouseDisabledTarget", "fp_ci_gmdt", (PVOID)Hook_Ci_GetMouseDisabledTarget, &g_OrigCi_GetMouseDisabledTarget);
    HookOneScriptTable("CanISeeTarget",          "fp_ci_cist", (PVOID)Hook_Ci_CanISeeTarget,          &g_OrigCi_CanISeeTarget);
    HookOneScriptTable("PlayerMouseAction",      "fp_ci_pma",  (PVOID)Hook_Ci_PlayerMouseAction,      &g_OrigCi_PlayerMouseAction);
    HookOneScriptTable("GetPlayerMouseDisabled", "fp_ci_gpmd", (PVOID)Hook_Ci_GetPlayerMouseDisabled, &g_OrigCi_GetPlayerMouseDisabled);
    HookOneScriptTable("KeyboardMouseInput",     "fp_ci_kmi",  (PVOID)Hook_Ci_KeyboardMouseInput,     &g_OrigCi_KeyboardMouseInput);
    HookOneScriptTable("RefreshMouseMove",       "fp_ci_rmm",  (PVOID)Hook_Ci_RefreshMouseMove,       &g_OrigCi_RefreshMouseMove);
    HookOneScriptTable("GetQuestHoverDescription", "fp_ci_gqhd", (PVOID)Hook_Ci_GetQuestHoverDescription, &g_OrigCi_GetQuestHoverDescription);
    HookOneScriptTable("anon@1400@gml_Object_Quest_Object_Parent_obj_Create_0", "fp_ci_a1400", (PVOID)Hook_Ci_Anon1400, &g_OrigCi_Anon1400);
    HookOneScriptTable("anon@1584@gml_Object_Quest_Object_Parent_obj_Create_0", "fp_ci_a1584", (PVOID)Hook_Ci_Anon1584, &g_OrigCi_Anon1584);
    HookOneScriptTable("anon@2113@gml_Object_Quest_Object_Parent_obj_Create_0", "fp_ci_a2113", (PVOID)Hook_Ci_Anon2113, &g_OrigCi_Anon2113);
    HookOneScriptTable("anon@2786@gml_Object_Quest_Object_Parent_obj_Create_0", "fp_ci_a2786", (PVOID)Hook_Ci_Anon2786, &g_OrigCi_Anon2786);
    HookOneScriptTable("anon@3858@gml_Object_Quest_Object_Parent_obj_Create_0", "fp_ci_a3858", (PVOID)Hook_Ci_Anon3858, &g_OrigCi_Anon3858);
    HookOneScriptTable("anon@4737@gml_Object_Quest_Object_Parent_obj_Create_0", "fp_ci_a4737", (PVOID)Hook_Ci_Anon4737, &g_OrigCi_Anon4737);
    HookOneScriptTable("anon@5164@gml_Object_Quest_Object_Parent_obj_Create_0", "fp_ci_a5164", (PVOID)Hook_Ci_Anon5164, &g_OrigCi_Anon5164);
    HookBuiltin("keyboard_check_pressed",     "fp_ci_kcp",  (PVOID)Hook_Ci_KeyboardCheckPressed,     &g_OrigCi_KeyboardCheckPressed);
    HookBuiltin("mouse_check_button_pressed", "fp_ci_mcbp", (PVOID)Hook_Ci_MouseCheckButtonPressed,  &g_OrigCi_MouseCheckButtonPressed);
    HookOneScriptTable("anon@1940@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p1940", (PVOID)Hook_Ci_PmAnon1940, &g_OrigCi_PmAnon1940);
    HookOneScriptTable("anon@2426@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p2426", (PVOID)Hook_Ci_PmAnon2426, &g_OrigCi_PmAnon2426);
    HookOneScriptTable("anon@3331@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p3331", (PVOID)Hook_Ci_PmAnon3331, &g_OrigCi_PmAnon3331);
    HookOneScriptTable("anon@5032@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p5032", (PVOID)Hook_Ci_PmAnon5032, &g_OrigCi_PmAnon5032);
    HookOneScriptTable("anon@5174@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p5174", (PVOID)Hook_Ci_PmAnon5174, &g_OrigCi_PmAnon5174);
    HookOneScriptTable("anon@5646@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p5646", (PVOID)Hook_Ci_PmAnon5646, &g_OrigCi_PmAnon5646);
    HookOneScriptTable("anon@6344@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p6344", (PVOID)Hook_Ci_PmAnon6344, &g_OrigCi_PmAnon6344);
    HookOneScriptTable("anon@6662@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p6662", (PVOID)Hook_Ci_PmAnon6662, &g_OrigCi_PmAnon6662);
    HookOneScriptTable("anon@7920@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p7920", (PVOID)Hook_Ci_PmAnon7920, &g_OrigCi_PmAnon7920);
    HookOneScriptTable("anon@9698@gml_Object_Profile_Manager_obj_Create_0", "fp_ci_p9698", (PVOID)Hook_Ci_PmAnon9698, &g_OrigCi_PmAnon9698);
    // Raw object-event names (no "gml_Script_" prefix) - see the comment
    // above these hooks' CITRACE_HOOK_NAMED declarations for why this is a
    // genuinely new code path, not a repeat of anything tried before.
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Step_0",    "fp_ci_qostep", (PVOID)Hook_Ci_QoStep,       &g_OrigCi_QoStep);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Destroy_0", "fp_ci_qodest", (PVOID)Hook_Ci_QoDestroy,    &g_OrigCi_QoDestroy);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Create_0",  "fp_ci_qocre",  (PVOID)Hook_Ci_QoCreate,     &g_OrigCi_QoCreate);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_0",   "fp_ci_qom0",   (PVOID)Hook_Ci_QoMouse0,     &g_OrigCi_QoMouse0);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_1",   "fp_ci_qom1",   (PVOID)Hook_Ci_QoMouse1,     &g_OrigCi_QoMouse1);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_2",   "fp_ci_qom2",   (PVOID)Hook_Ci_QoMouse2,     &g_OrigCi_QoMouse2);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_3",   "fp_ci_qom3",   (PVOID)Hook_Ci_QoMouse3,     &g_OrigCi_QoMouse3);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_4",   "fp_ci_qom4",   (PVOID)Hook_Ci_QoMouse4,     &g_OrigCi_QoMouse4);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_5",   "fp_ci_qom5",   (PVOID)Hook_Ci_QoMouse5,     &g_OrigCi_QoMouse5);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_6",   "fp_ci_qom6",   (PVOID)Hook_Ci_QoMouse6,     &g_OrigCi_QoMouse6);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_7",   "fp_ci_qom7",   (PVOID)Hook_Ci_QoMouse7,     &g_OrigCi_QoMouse7);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_8",   "fp_ci_qom8",   (PVOID)Hook_Ci_QoMouse8,     &g_OrigCi_QoMouse8);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_9",   "fp_ci_qom9",   (PVOID)Hook_Ci_QoMouse9,     &g_OrigCi_QoMouse9);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_10",  "fp_ci_qom10",  (PVOID)Hook_Ci_QoMouse10,    &g_OrigCi_QoMouse10);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Mouse_11",  "fp_ci_qom11",  (PVOID)Hook_Ci_QoMouse11,    &g_OrigCi_QoMouse11);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Alarm_0",   "fp_ci_qoa0",   (PVOID)Hook_Ci_QoAlarm0,     &g_OrigCi_QoAlarm0);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Alarm_1",   "fp_ci_qoa1",   (PVOID)Hook_Ci_QoAlarm1,     &g_OrigCi_QoAlarm1);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Alarm_2",   "fp_ci_qoa2",   (PVOID)Hook_Ci_QoAlarm2,     &g_OrigCi_QoAlarm2);
    HookRawNamedRoutine("gml_Object_Quest_Object_Parent_obj_Alarm_3",   "fp_ci_qoa3",   (PVOID)Hook_Ci_QoAlarm3,     &g_OrigCi_QoAlarm3);
    HookRawNamedRoutine("gml_Object_Profile_Manager_obj_Step_0",        "fp_ci_pmstep", (PVOID)Hook_Ci_PmStep,       &g_OrigCi_PmStep);
    HookRawNamedRoutine("gml_Object_Player_obj_Step_0",                 "fp_ci_plstep", (PVOID)Hook_Ci_PlayerStep,  &g_OrigCi_PlayerStep);
    HookRawNamedRoutine("gml_Object_Player_obj_Mouse_3",                "fp_ci_plm3",   (PVOID)Hook_Ci_PlayerMouse3, &g_OrigCi_PlayerMouse3);
}

static void CiTraceStats()
{
    Out(std::string("citrace stat: tracing=") + (g_CiTraceOn.load() ? "on" : "off") + " logged=" + std::to_string(g_CiLogged) + "/" + std::to_string(kCiLogBudget));
    char b[400];
    sprintf_s(b, "  CheckPlayerInteraction hook=%s calls=%ld | CheckUseKey hook=%s calls=%ld | PlayerInteracting hook=%s calls=%ld",
              g_OrigCi_CheckPlayerInteraction ? "yes" : "no", g_CiCalls_CheckPlayerInteraction,
              g_OrigCi_CheckUseKey ? "yes" : "no", g_CiCalls_CheckUseKey,
              g_OrigCi_PlayerInteracting ? "yes" : "no", g_CiCalls_PlayerInteracting);
    Out(b);
    sprintf_s(b, "  LootBlocksUseKey hook=%s calls=%ld | PickupLoot hook=%s calls=%ld",
              g_OrigCi_LootBlocksUseKey ? "yes" : "no", g_CiCalls_LootBlocksUseKey,
              g_OrigCi_PickupLoot ? "yes" : "no", g_CiCalls_PickupLoot);
    Out(b);
    sprintf_s(b, "  GetMouseTarget calls=%ld | PlayerGetMouseTarget calls=%ld | GetMouseDisabledTarget calls=%ld | CanISeeTarget calls=%ld",
              g_CiCalls_GetMouseTarget, g_CiCalls_PlayerGetMouseTarget, g_CiCalls_GetMouseDisabledTarget, g_CiCalls_CanISeeTarget);
    Out(b);
    sprintf_s(b, "  PlayerMouseAction calls=%ld | GetPlayerMouseDisabled calls=%ld | KeyboardMouseInput calls=%ld | RefreshMouseMove calls=%ld | GetQuestHoverDescription calls=%ld",
              g_CiCalls_PlayerMouseAction, g_CiCalls_GetPlayerMouseDisabled, g_CiCalls_KeyboardMouseInput,
              g_CiCalls_RefreshMouseMove, g_CiCalls_GetQuestHoverDescription);
    Out(b);
    sprintf_s(b, "  [ProfileManager|Player-filtered] instance_position=%ld instance_place=%ld collision_point=%ld point_in_rectangle=%ld",
              g_CiCalls_InstancePosition, g_CiCalls_InstancePlace, g_CiCalls_CollisionPoint, g_CiCalls_PointInRectangle);
    Out(b);
    sprintf_s(b, "  [ProfileManager|Player-filtered, cont.] distance_to_point=%ld instance_nearest=%ld point_in_circle=%ld position_meeting=%ld",
              g_CiCalls_DistanceToPoint, g_CiCalls_InstanceNearest, g_CiCalls_PointInCircle, g_CiCalls_PositionMeeting);
    Out(b);
    sprintf_s(b, "  [quest-object-filtered] instance_destroy=%ld instance_deactivate_object=%ld instance_deactivate_all=%ld instance_change=%ld variable_instance_set(marker names)=%ld",
              g_CiCalls_InstanceDestroy, g_CiCalls_InstanceDeactivateObject, g_CiCalls_InstanceDeactivateAll, g_CiCalls_InstanceChange, g_CiCalls_VariableInstanceSet);
    Out(b);
    sprintf_s(b, "  Quest_Object_Parent_obj Create anons: 1400=%ld 1584=%ld 2113=%ld 2786=%ld 3858=%ld 4737=%ld 5164=%ld",
              g_CiCalls_Anon1400, g_CiCalls_Anon1584, g_CiCalls_Anon2113, g_CiCalls_Anon2786,
              g_CiCalls_Anon3858, g_CiCalls_Anon4737, g_CiCalls_Anon5164);
    Out(b);
    sprintf_s(b, "  [builtins] keyboard_check_pressed hook=%s calls=%ld | mouse_check_button_pressed hook=%s calls=%ld",
              g_OrigCi_KeyboardCheckPressed ? "yes" : "no", g_CiCalls_KeyboardCheckPressed,
              g_OrigCi_MouseCheckButtonPressed ? "yes" : "no", g_CiCalls_MouseCheckButtonPressed);
    Out(b);
    sprintf_s(b, "  Profile_Manager_obj Create anons: 1940=%ld 2426=%ld 3331=%ld 5032=%ld 5174=%ld",
              g_CiCalls_PmAnon1940, g_CiCalls_PmAnon2426, g_CiCalls_PmAnon3331, g_CiCalls_PmAnon5032, g_CiCalls_PmAnon5174);
    Out(b);
    sprintf_s(b, "  Profile_Manager_obj Create anons (cont.): 5646=%ld 6344=%ld 6662=%ld 7920=%ld 9698=%ld",
              g_CiCalls_PmAnon5646, g_CiCalls_PmAnon6344, g_CiCalls_PmAnon6662, g_CiCalls_PmAnon7920, g_CiCalls_PmAnon9698);
    Out(b);
    sprintf_s(b, "  [raw object-event] Qo.Step=%ld Qo.Destroy=%ld Qo.Create=%ld Pm.Step=%ld Player.Step=%ld Player.Mouse3=%ld",
              g_CiCalls_QoStep, g_CiCalls_QoDestroy, g_CiCalls_QoCreate, g_CiCalls_PmStep, g_CiCalls_PlayerStep, g_CiCalls_PlayerMouse3);
    Out(b);
    sprintf_s(b, "  [raw object-event] Qo.Mouse 0-5=%ld,%ld,%ld,%ld,%ld,%ld 6-11=%ld,%ld,%ld,%ld,%ld,%ld",
              g_CiCalls_QoMouse0, g_CiCalls_QoMouse1, g_CiCalls_QoMouse2, g_CiCalls_QoMouse3, g_CiCalls_QoMouse4, g_CiCalls_QoMouse5,
              g_CiCalls_QoMouse6, g_CiCalls_QoMouse7, g_CiCalls_QoMouse8, g_CiCalls_QoMouse9, g_CiCalls_QoMouse10, g_CiCalls_QoMouse11);
    Out(b);
    sprintf_s(b, "  [raw object-event] Qo.Alarm 0-3=%ld,%ld,%ld,%ld",
              g_CiCalls_QoAlarm0, g_CiCalls_QoAlarm1, g_CiCalls_QoAlarm2, g_CiCalls_QoAlarm3);
    Out(b);
}
#endif

static void BeaconStatus()
{
    Out(std::string("beacon: ") + (g_BeEnabled.load() ? "ON" : "off") + (g_BeForced.load() ? " (forced)" : "")
        + " hook=" + (g_BeHookInstalled ? "yes" : "no") + " active=" + (BeaconActive() ? "yes" : "no")
        + " range=" + std::to_string((long long)g_BeRange) + " mode=" + (g_BeRareOnly ? "rare" : "all")
        + " scans=" + std::to_string(g_BeScans) + " ranged=" + std::to_string(g_BeRanged) + " leashSkips=" + std::to_string(g_BeLeashSkips)
        + " wake=" + std::to_string((long long)g_BeWakeRadius) + " every=" + std::to_string(g_BeWakeEvery) + " wakeCalls=" + std::to_string(g_BeWakeCalls) + " freezeSkipped=" + std::to_string(g_BeFreezeSkipped) + " awake=" + std::to_string(g_BeWoken)
        + " creators=" + (g_BeWakeCreators ? "on" : "off") + " creatorsAwake=" + std::to_string(g_BeCreatorsAwake)
        + " farStep=" + (g_BeFarStep ? "on" : "off") + " farFrom=" + std::to_string((long long)g_BeFarFrom) + " farSteps=" + std::to_string(g_BeFarSteps) + " farErrors=" + std::to_string(g_BeFarStepErrors)
        + " spawnNear=" + (g_BeSpawnNear ? "on" : "off") + " spawnLies=" + std::to_string(g_BeSpawnLies)
        + " itemLoaded=" + (g_BeItemTagged.load() ? "yes" : "no"));
}
static void BeaconAutoArm()
{
    bool wanted = g_BeForced.load();
    // Skip builtin entries - see the identical note in TyrantAutoArm (bug found 2026-09-11).
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (!e.builtin && e.mechanic == "beacon") { wanted = true; break; }   // active only while the amulet is worn
    if (!wanted) return;
    InstallBeaconHook();
    g_BeEnabled.store(g_BeHookInstalled);
    Out(std::string("beacon: ") + (g_BeHookInstalled ? "armed" : "hook failed") + " (range " + std::to_string((long long)g_BeRange) + ", mode " + (g_BeRareOnly ? "rare" : "all") + ")");
}

static bool HhApplyBuff(CInstance* player, const HhBuff& b, double frames)
{
    try {
        double mplr = 1.0;
        try { RValue m = g_Yytk->CallBuiltin("variable_global_get", { RValue("mplr") }); if (m.m_Kind != VALUE_UNDEFINED) mplr = m.ToDouble(); } catch (...) {}
        std::vector<RValue> vals = { RValue(b.v0), RValue(b.v1) };
        std::vector<RValue> args = {
            RValue(mplr), RValue(b.id), RValue(vals), RValue(frames),
            RValue(true), RValue(false), RValue(1.0), RValue(false), RValue(false), RValue(true)
        };
        RValue result;
        AurieStatus st = g_Yytk->CallGameScriptEx(result, "gml_Script_BuffAdd", player, player, args);
        if (AurieSuccess(st)) InterlockedIncrement(&g_HhBuffsApplied);
        if (g_HhTrace) Out("hh: BuffAdd id=" + std::to_string((long long)b.id) + " [" + std::to_string(b.v0) + "," + std::to_string(b.v1) + "] frames=" + std::to_string((int)frames) + " st=" + std::to_string((int)st));
        return AurieSuccess(st);
    } catch (...) { Out("hh: BuffAdd EXCEPTION"); }
    return false;
}

// Core: translate one dying enemy's affixes into buffs on `player`.
static bool HhOnKill(CInstance* player, const RValue& enemy)
{
    InterlockedIncrement(&g_HhKills);
    int applied = 0;
    try {
#ifndef FORGEPACT_RELEASE
        // Research build: prove what argument 2 really is on the first kills (object name,
        // whether it carries the enemy fields), so the detection logic rests on evidence.
        if (g_HhTrace && g_HhKills <= 5) {
            std::string what = Describe(enemy);
            try {
                RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue("object_index") });
                RValue nm = g_Yytk->CallBuiltin("object_get_name", { oi });
                RValue hasR = g_Yytk->CallBuiltin("variable_instance_exists", { enemy, RValue("enemyRarity") });
                RValue hasL = g_Yytk->CallBuiltin("variable_instance_exists", { enemy, RValue("affixList") });
                RValue hasF = g_Yytk->CallBuiltin("variable_instance_exists", { enemy, RValue("enemyAffix") });
                what += " object=" + nm.ToString() + " enemyRarity?" + (hasR.ToBoolean() ? "y" : "n") + " affixList?" + (hasL.ToBoolean() ? "y" : "n") + " enemyAffix?" + (hasF.ToBoolean() ? "y" : "n");
                if (hasR.ToBoolean()) what += " rarity=" + Describe(g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue("enemyRarity") }));
            } catch (...) { what += " (field probe failed)"; }
            Out("hh: kill #" + std::to_string(g_HhKills) + " arg2=" + what);
        }
#endif
        // Rarity flag of the dying enemy (1 = normal; higher = champion/rare/... by the game's own scale).
        double rarity = -1.0;
        try {
            RValue hasR = g_Yytk->CallBuiltin("variable_instance_exists", { enemy, RValue("enemyRarity") });
            if (hasR.ToBoolean()) rarity = g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue("enemyRarity") }).ToDouble();
        } catch (...) {}
        if (rarity >= 2.0) InterlockedIncrement(&g_HhRarityKills);
        // Live 2026-09-04: normal monsters (rarity 1) can carry affix slots (#2, #16, #21 seen)
        // and even a filled affixList, so ONLY the game's own rarity flag decides.
        if (rarity < 2.0) return false;

        // Affix keys.  Best source: the enemy's own health bar (enemy.myHealthBar ->
        // Enemy_Health_Bar_Parent_obj) carries `affixName`, the array of displayed affix
        // strings ("Fallen Angel", "Extra Fast", ...).  Normalised to lower-case letters so
        // they match the kHhAffixNames keys ("fallenangel", "extrafast").  Fallbacks: the
        // numeric affixList / enemyAffix flags translated through the index table.
        std::vector<std::string> keys;
        try {
            RValue bar = g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue("myHealthBar") });
            if (bar.m_Kind != VALUE_UNDEFINED) {
                RValue names = g_Yytk->CallBuiltin("variable_instance_get", { bar, RValue("affixName") });
                if (names.m_Kind == VALUE_ARRAY) {
                    int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
                    for (int i = 0; i < n && i < 8; ++i) {
                        RValue s = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
                        std::string raw = s.ToString(), k;
                        for (unsigned char ch : raw) if (std::isalnum(ch) && ch < 128) k += (char)std::tolower(ch);
                        if (!k.empty()) keys.push_back(k);
                    }
                } else if (names.m_Kind == VALUE_STRING) {
                    std::string raw = names.ToString(), k;
                    for (unsigned char ch : raw) if (std::isalnum(ch) && ch < 128) k += (char)std::tolower(ch);
                    if (!k.empty()) keys.push_back(k);
                }
            }
        } catch (...) {}
        if (!keys.empty() && g_HhTrace) {
            std::string ks; for (const auto& k : keys) ks += (ks.empty() ? "" : ",") + k;
            Out("hh: health bar affixes = [" + ks + "]");
        }
        if (keys.empty()) try {
            RValue hasList = g_Yytk->CallBuiltin("variable_instance_exists", { enemy, RValue("affixList") });
            if (hasList.ToBoolean()) {
                RValue list = g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue("affixList") });
                if (list.m_Kind == VALUE_ARRAY) {
                    int n = (int)g_Yytk->CallBuiltin("array_length", { list }).ToDouble();
                    for (int i = 0; i < n && i < 16; ++i) {
                        RValue e = g_Yytk->CallBuiltin("array_get", { list, RValue((double)i) });
                        std::string k = (e.m_Kind == VALUE_REAL || e.m_Kind == VALUE_INT32 || e.m_Kind == VALUE_INT64) ? HhAffixName((int)e.ToDouble()) : HhKeyOf(e);
                        if (!k.empty()) keys.push_back(k);
                    }
                }
            }
        } catch (...) {}
        // Live 2026-09-04: normal monsters (enemyRarity 1) also carry a few non-zero enemyAffix
        // slots (#16, #21 seen), so the flag array is only trusted on rarity >= 2.
        if (keys.empty() && rarity >= 2.0) {
            try {
                RValue hasFlags = g_Yytk->CallBuiltin("variable_instance_exists", { enemy, RValue("enemyAffix") });
                if (hasFlags.ToBoolean()) {
                    RValue flags = g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue("enemyAffix") });
                    if (flags.m_Kind == VALUE_ARRAY) {
                        int n = (int)g_Yytk->CallBuiltin("array_length", { flags }).ToDouble();
                        for (int i = 0; i < n && i < 128; ++i) {
                            RValue f = g_Yytk->CallBuiltin("array_get", { flags, RValue((double)i) });
                            double v = (f.m_Kind == VALUE_BOOL) ? (f.ToBoolean() ? 1.0 : 0.0)
                                     : ((f.m_Kind == VALUE_REAL || f.m_Kind == VALUE_INT32 || f.m_Kind == VALUE_INT64) ? f.ToDouble() : 0.0);
                            if (v != 0.0) keys.push_back(HhAffixName(i));
                        }
                    }
                }
            } catch (...) {}
        }
        // The enemy also carries its display affix data (affixName = text drawn above the
        // health bar, affixMod = modifier list).  Add their lower-cased words as keys so
        // hhmap can address affixes by name (e.g. "berserker") as well as by index ("#7").
        std::string affixText;
        for (const char* field : { "affixMod", "affixName" }) {
            try {
                RValue has = g_Yytk->CallBuiltin("variable_instance_exists", { enemy, RValue(field) });
                if (!has.ToBoolean()) continue;
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue(field) });
                if (v.m_Kind == VALUE_STRING) { std::string s = v.ToString(); if (!s.empty()) affixText += (affixText.empty() ? "" : " ") + s; }
                else if (v.m_Kind == VALUE_ARRAY) {
                    int n = (int)g_Yytk->CallBuiltin("array_length", { v }).ToDouble();
                    for (int i = 0; i < n && i < 16; ++i) {
                        RValue e = g_Yytk->CallBuiltin("array_get", { v, RValue((double)i) });
                        std::string k = HhKeyOf(e);
                        if (!k.empty()) affixText += (affixText.empty() ? "" : " ") + k;
                    }
                }
            } catch (...) {}
        }
        if (!affixText.empty() && (rarity >= 2.0 || !keys.empty())) {
            std::string word;
            for (char ch : affixText + " ") {
                if (std::isalnum((unsigned char)ch)) word += (char)std::tolower((unsigned char)ch);
                else { if (word.size() > 2 && word != "affix") keys.push_back(word.rfind("affix", 0) == 0 ? word.substr(5) : word); word.clear(); }
            }
        }
        if (g_HhTrace && (rarity >= 2.0 || !keys.empty())) Out("hh: affix text=\"" + affixText + "\"");
        if (!keys.empty()) InterlockedIncrement(&g_HhAffixKills);

        // Only the game's rarity gate above makes a monster eligible.
        InterlockedIncrement(&g_HhRareKills);
        if (g_HhTrace) {
            std::string ks; for (const auto& k : keys) ks += (ks.empty() ? "" : ",") + k;
            Out("hh: rare kill rarity=" + std::to_string((int)rarity) + " affixes=[" + ks + "]");
        }
        if (!HhEquipped(player)) { InterlockedIncrement(&g_HhSkippedNotEquipped); if (g_HhTrace) Out("hh: belt not equipped, skipped"); return false; }
        double spd = 60.0;
        try { spd = g_Yytk->CallBuiltin("game_get_speed", { RValue(0.0) }).ToDouble(); if (spd < 1.0) spd = 60.0; } catch (...) {}
        double frames = g_HhDurationSec * spd;
        int mapped = 0;
        for (const std::string& key : keys) {
            auto it = g_HhMap.find(key);
            if (it == g_HhMap.end() && key.rfind("affix", 0) == 0) it = g_HhMap.find(key.substr(5));
            if (it != g_HhMap.end()) {
                ++mapped;
                if (HhApplyBuff(player, it->second, frames)) {
                    ++applied;
                    HhRememberStolen(player, it->first, g_HhDurationSec, it->second.id);
                }
            }
            else if (g_HhTrace) Out("hh: no mapping for affix '" + key + "'");
        }
        if (mapped == 0 && g_HhDefaultOn && HhApplyBuff(player, g_HhDefault, frames)) {
            ++applied;
            HhRememberStolen(player, "Rare Essence", g_HhDurationSec, g_HhDefault.id);
        }
    } catch (...) { Out("hh: EXCEPTION in HhOnKill"); }
    return applied > 0;
}

#ifndef FORGEPACT_RELEASE
// --- equip tracing (research build) ------------------------------------------
// Logs which instance runs the equipment scripts and what they receive, so the
// runtime location of the equipped item structs can be pinned down live.
static std::string HhSelfName(CInstance* S)
{
    if (!S) return "null";
    try {
        RValue id = S->ToRValue();
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("object_index") });
        RValue nm = g_Yytk->CallBuiltin("object_get_name", { oi });
        RValue iid = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("id") });
        return nm.ToString() + "#" + std::to_string((long long)iid.ToDouble());
    } catch (...) { return "?"; }
}
static void HhLogSelfVars(CInstance* S, const char* filter)
{
    if (!S) return;
    try {
        RValue id = S->ToRValue();
        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { id });
        int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
        std::string line;
        for (int i = 0; i < n; ++i) {
            RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            std::string s = nm.ToString();
            if (Lower(s).find(filter) != std::string::npos) {
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, nm });
                line += " " + s + "=" + Describe(v);
            }
        }
        Out("   self vars ~" + std::string(filter) + ":" + (line.empty() ? " (none)" : line));
    } catch (...) {}
}
#define HH_TRACE_HOOK(NAME) \
    static PFUNC_YYGMLScript g_OrigTrace_##NAME = nullptr; \
    static RValue& Hook_Trace##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        if (g_HhTrace) { \
            std::string a; for (int i = 0; i < argc && i < 6; ++i) a += " a" + std::to_string(i) + "=" + (A && A[i] ? Describe(*A[i]) : std::string("?")); \
            Out(std::string("trace " #NAME " self=") + HhSelfName(S) + " other=" + HhSelfName(O) + " argc=" + std::to_string(argc) + a); \
            HhLogSelfVars(S, "equip"); \
        } \
        return g_OrigTrace_##NAME ? g_OrigTrace_##NAME(S, O, R, argc, A) : R; \
    }
HH_TRACE_HOOK(RunItemEquipped)
HH_TRACE_HOOK(ItemEquip)
HH_TRACE_HOOK(EquipItemUnequip)
static bool g_HhTraceHooksInstalled = false;
static void InstallEquipTraceHooks()
{
    if (g_HhTraceHooksInstalled) return;
    g_HhTraceHooksInstalled = true;
    HookOneScriptTable("RunItemEquipped", "fp_tr_rie", (PVOID)Hook_TraceRunItemEquipped, &g_OrigTrace_RunItemEquipped);
    HookOneScriptTable("ItemEquip", "fp_tr_ie", (PVOID)Hook_TraceItemEquip, &g_OrigTrace_ItemEquip);
    HookOneScriptTable("EquipItemUnequip", "fp_tr_eiu", (PVOID)Hook_TraceEquipItemUnequip, &g_OrigTrace_EquipItemUnequip);
}
#endif

// Live-verified 2026-09-04 (research probe, 5 real kills): EnemyDestroyKillProc runs with
// self = the DYING ENEMY and argument 2 = the killing Player_obj (the earlier reading had
// the roles swapped, which is why no kill ever showed enemy data).
// ---- signature drops ----------------------------------------------------------------------
// On every monster kill roll g_SigDropPct (Angelic's own 1-in-7500, no pity); on a hit build the next signature item
// through the game's own loader (InitItemFromJson(json, "region-account-timestamp-type")) and
// drop it where the monster died (LootGroundCreateFromItem).  The forge hooks fire inside
// InitItemFromJson -> CreateItemNew, so the built-in entry above dresses the item.
// Vanilla rates (agreed 2026-09-06): rare/champion 0.05 pct, ancient 0.5 pct, and a pity
// counter that guarantees a drop after 1500 rare-tier kills without one.  `sigdrop` tunes them.
static const double kSigDropAngelicPct = 100.0 / 7500.0;   // the game's Angelic/Unholy base (1 in 7500)
static double g_SigDropPct = kSigDropAngelicPct;           // normal, rare and champion kills
static double g_SigDropAncientPct = kSigDropAngelicPct;    // ancient (4) kills
static long g_SigDropPity = 0;                             // 0 = no pity (default)
static long g_SigDropSinceLast = 0;
static long g_SigDropRolls = 0, g_SigDropHits = 0, g_SigDropFails = 0;
static int g_SigDropNext = 0;           // 0 = crown, 1 = belt (they alternate)
static bool SpawnSignatureItem(int which, double x, double y, CInstance* ctx)
{
    const double seed = which == 0 ? kSigCrownSeed : kSigBeltSeed;
    const int type = which == 0 ? 0 : 8;
    const int b = which == 0 ? 7 : 2;
    const char* label = which == 0 ? "Tyrant's Crown" : "Headhunter";
    const char* stage = "start";
    try {
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const std::string key = "0-0-" + std::to_string(ms) + "-" + std::to_string(type);
        const std::string json = "{\"w\":1,\"a\":" + std::to_string((long long)seed) + ",\"j\":0,\"b\":" + std::to_string(b) + ",\"c\":0,\"o\":1}";
        stage = "global";
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        if (!g) { ++g_SigDropFails; Out("sigdrop: no global instance"); return false; }
        stage = "json_parse";
        RValue parsed; g_Yytk->CallBuiltinEx(parsed, "json_parse", g, g, { RValue(json) });
        if (parsed.m_Kind != VALUE_OBJECT) { ++g_SigDropFails; Out(std::string("sigdrop: json_parse gave ") + Describe(parsed)); return false; }
        stage = "InitItemFromJson";
        RValue item; AurieStatus st = g_Yytk->CallGameScriptEx(item, "gml_Script_InitItemFromJson", g, g, { parsed, RValue(key) });
        if (!AurieSuccess(st) || item.m_Kind != VALUE_OBJECT) {   // argument order not yet proven live: try the swap once
            RValue item2; AurieStatus st2 = g_Yytk->CallGameScriptEx(item2, "gml_Script_InitItemFromJson", g, g, { RValue(key), parsed });
            if (AurieSuccess(st2) && item2.m_Kind == VALUE_OBJECT) { item = item2; st = st2; }
            else { ++g_SigDropFails; Out(std::string("sigdrop: InitItemFromJson gave ") + Describe(item) + " st=" + std::to_string((int)st) + " / swapped " + Describe(item2) + " st=" + std::to_string((int)st2) + " for " + label); return false; }
        }
        if (g_SigDropHits + g_SigDropFails < 3) {
            try { RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { item }); std::string s = js.ToString(); Out("sigdrop: item = " + s.substr(0, 420)); } catch (...) { Out("sigdrop: item stringify failed"); }
        }
        stage = "LootGroundCreateFromItem";
        CInstance* self = ctx ? ctx : g;
        RValue res; AurieStatus st3 = AURIE_SUCCESS;
        try {
            st3 = g_Yytk->CallGameScriptEx(res, "gml_Script_LootGroundCreateFromItem", self, self, { RValue(x), RValue(y), item });   // decompiled: (x, y, item)
        } catch (const std::exception& e) { ++g_SigDropFails; Out(std::string("sigdrop: LootGroundCreateFromItem threw: ") + e.what()); return false; }
        if (!AurieSuccess(st3)) { ++g_SigDropFails; Out(std::string("sigdrop: LootGroundCreateFromItem st=") + std::to_string((int)st3) + " for " + label); return false; }
        stage = "after";
        ++g_SigDropHits;
        Out(std::string("sigdrop: ") + label + " dropped at " + std::to_string((int)x) + "," + std::to_string((int)y) + " kind=" + std::to_string((int)res.m_Kind));
        return true;
    } catch (...) { ++g_SigDropFails; Out(std::string("sigdrop: EXCEPTION at ") + stage + " while dropping " + label); return false; }
}
// ---- Angelic drops on our own die ----------------------------------------------------------
// The game's own angelic roll (DropItemAngelicChance: irandom(item drop rate) < chance, chance
// 2-3k in ordinary zones) practically never fires offline, and its guaranteed maker
// (DropItemAngelic) loops forever when the zone's unique loot list has no angelic candidate.
// So: one die per kill, and on a hit an Angelic/Unholy unique from the table below, built
// through the same path as the signature drops (definition json -> InitItemFromJson ->
// LootGroundCreateFromItem) with c=1 (unique repo).  Each row is validated once against the
// game's unique repo: the name key must match and the base's info flag 40 (hidden / dev item,
// which the game's own picker skips) must NOT be set - live 2026-09-07: exactly the nine dev
// and joke items carry it (Dev Charm, DEVELOPRE BOOT, Elemelon...), the 49 real ones do not.
struct AngelicBase { int type, sub, b; const char* key; const char* name; bool angelic; };
static const AngelicBase kAngelicBases[] = {
    { 0, 0, 85, "helmet_lucifers_crown", "Lucifer's Crown", true },
    { 0, 0, 86, "helmet_mask_of_celestial", "Mask of the Celestial", true },
    { 1, 0, 2, "armors_tayrels_chestplate", "Tayrel's Chestplate", true },
    { 1, 0, 33, "armors_st_judas_hauberk", "St. Jupe's Plate of Command", true },
    { 1, 0, 95, "armors_grand_archwizards_mantle", "Grand Arch Wizard's Mantle", false },
    { 2, 0, 62, "boots_marchers_of_hatred", "Marcher's of Hatred", false },
    { 2, 0, 65, "boots_developer_boots", "DEVELOPRE BOOT", false },
    { 2, 0, 79, "boots_peg_leg", "Peg Leg", false },
    { 3, 1, 15, "w_melee_st_gabriels_retribution", "St. Gabriel's Retribution", true },
    { 3, 1, 24, "w_melee_st_mikas_zweihander", "St. Mika's Zweih?nder", true },
    { 3, 1, 35, "w_melee_stofflix_cooking_cleaver", "Stofflix Cooking Cleaver", false },
    { 3, 3, 18, "w_melee_dawn_bringer", "The Dawn Bringer", true },
    { 3, 3, 21, "w_melee_stormslayer", "Stormslayer", false },
    { 3, 4, 5, "w_melee_st_rexis_sundering_axe", "St. Rexis Sundering Axe", true },
    { 3, 4, 9, "w_melee_aurelion_fury", "Aurelion Fury", true },
    { 3, 5, 0, "w_claw_storm_fury", "Storm Fury", true },
    { 3, 6, 9, "w_polearm_st_draxis_pigstick", "St. Draxis' Pigstick", true },
    { 3, 7, 5, "w_chainsaw_st_meeses_longsaw", "St. Draxis Longsaw", true },
    { 3, 9, 4, "w_spell_nimosLightbringer", "St. Nimo's Lightbringer", true },
    { 3, 10, 3, "w_spell_st_houdeaniis_tiny_fire_rod", "St. HouDeanii's Tiny Fire Rod", true },
    { 3, 13, 6, "w_bow_st_soloyolos_holy_bow", "St. Soloyolo's Holy Bow", true },
    { 3, 13, 13, "w_bow_st_amithiels_truth", "St. Amitiel's Truth", true },
    { 3, 14, 8, "w_gun_st_brooks_elementium_pistol", "Commander's Sentry Blaster", true },
    { 3, 14, 16, "w_gun_glock22", "Glock 22", false },
    { 3, 16, 6, "w_throwing_st_neris_rainbow_lance", "St. Neri's Rainbow Lance", true },
    { 4, 0, 30, "gloves_ahtos_diamond_hands", "St. Ahto's Diamond Hands", true },
    { 4, 0, 49, "gloves_demonfire_accelerators", "Demonfire Accelerators", false },
    { 5, 0, 62, "amulets_liliths_scorn", "Lilith's Scorn", false },
    { 6, 0, 26, "shields_tomis_vibrant_aura", "St. Tomi's Vibrant Aura", true },
    { 6, 0, 30, "shields_hallgars_blood_forged", "St. Hallgar's Bloodforged Aegis", true },
    { 7, 0, 9, "rings_devlins_eternal_grace", "St. Aaron's Eternal Rage", true },
    { 7, 0, 14, "rings_fury_of_tarethiel", "Fury of Tarethiel", true },
    { 7, 0, 57, "rings_absolute_zero", "Absolute Zero", false },
    { 8, 0, 44, "belts_majories_belt", "Majorie's Belt", true },
    { 8, 0, 51, "belts_liquor_holster", "Liquor Holster", true },
    { 8, 0, 53, "belts_el_patrons_madness", "El Patr?n's Madness", false },
    { 10, 0, 32, "charms_annihilus", "Annihilator", false },
    { 10, 0, 47, "charms_water_melon", "Water Melon", true },
    { 10, 0, 48, "charms_earth_melon", "Earth Melon", true },
    { 10, 0, 49, "charms_fire_melon", "Fire Melon", true },
    { 10, 0, 50, "charms_air_melon", "Air Melon", true },
    { 10, 0, 51, "charms_supreme_elemelon", "Supreme Elemelon", true },
    { 10, 0, 53, "charms_arcane_pumpkin", "Arcane Pumpkin", false },
    { 10, 0, 54, "charms_rotten_pumpkin", "Rotten Pumpkin", false },
    { 10, 0, 56, "charms_liliths_wrath", "Lilith's Wrath", false },
    { 10, 0, 57, "charms_divine_crack_pipe", "Divine Crackpipe", true },
    { 10, 0, 65, "charms_forking_bolts", "Forking Bolts", false },
    { 10, 0, 66, "charms_reverse_card", "Reverse Card", true },
    { 10, 0, 74, "charms_almighty_nugget", "The Almighty Nugget", true },
    { 10, 0, 78, "charms_dev_charm_small", "Dev Charm Small", true },
    { 10, 0, 79, "charms_dev_charm_small", "Dev Charm Small", true },
    { 10, 0, 80, "charms_dev_charm_small", "Dev Charm Small", true },
    { 10, 0, 89, "charms_overloaded_dice", "Overloaded Dice", false },
    { 10, 0, 90, "charms_devils_pact", "Devil's Pact", false },
    { 10, 0, 91, "charms_soul_collector", "Soul Collector", false },
    { 10, 0, 98, "charms_goburins_head", "Goburin's Head", false },
    { 18, 0, 5, "consumable_elixir_of_unworldly_cognition", "Elixir of Unworldly Cognition", false },
    { 18, 0, 8, "consumable_gold_inlaid_mysterious_potion", "Gold Inlaid Mysterious Potion", true },
};
struct AngelicCandidate { int type, sub, b; std::string name; bool angelic; };
static std::vector<AngelicCandidate> g_AngelicPool;
static bool g_AngelicPoolBuilt = false;
static double g_AngelicDropOneIn = 0.0;   // 0 = off; N = one angelic drop per N kills on average
static volatile long g_AngelicDropRolls = 0, g_AngelicDropHits = 0, g_AngelicDropFails = 0;
static std::string StructKey(const RValue& st, const char* field)
{
    try { RValue v = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue(field) }); if (v.m_Kind == VALUE_STRING) return v.ToString(); } catch (...) {}
    return std::string();
}
static void BuildAngelicPool(bool verbose)
{
    if (g_AngelicPoolBuilt) return;
    g_AngelicPoolBuilt = true;
    g_AngelicPool.clear();
    int rejected = 0;
    for (const AngelicBase& base : kAngelicBases) {
        std::string why;
        if (base.type == 18) why = "potion";
        else try {
            RValue def = g_Yytk->CallGameScript("gml_Script_GetUniqueRepoStruct",
                                                { RValue((double)base.type), RValue((double)base.sub), RValue((double)base.b) });
            if (def.m_Kind != VALUE_OBJECT) why = "no def";
            else {
                RValue info = g_Yytk->CallBuiltin("variable_struct_get", { def, RValue("itemBaseInfoStruct") });
                const std::string key = info.m_Kind == VALUE_OBJECT ? StructKey(info, "28") : std::string();
                if (key != base.key) why = "key mismatch: " + key;
                else {
                    RValue f40 = g_Yytk->CallBuiltin("variable_struct_get", { info, RValue("40") });
                    bool flag = false;
                    try { flag = f40.m_Kind != VALUE_UNDEFINED && f40.m_Kind != VALUE_UNSET && f40.ToBoolean(); } catch (...) { flag = false; }
                    if (flag) why = "hidden/dev item (info flag 40 set)";   // the game's picker skips these
                }
            }
            if (why.empty()) {   // build one (not dropped) and keep it only if the game calls it Angelic or Unholy
                const std::string json = "{\"w\":1,\"a\":123456789,\"j\":" + std::to_string(base.sub) + ",\"b\":" + std::to_string(base.b) + ",\"c\":1}";
                CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
                RValue parsed; if (g) g_Yytk->CallBuiltinEx(parsed, "json_parse", g, g, { RValue(json) });
                RValue item; if (g && parsed.m_Kind == VALUE_OBJECT) g_Yytk->CallGameScriptEx(item, "gml_Script_InitItemFromJson", g, g, { parsed, RValue(std::string("0-0-1-") + std::to_string(base.type)) });
                double rar = -1.0;
                if (item.m_Kind == VALUE_OBJECT) { RValue info = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemInfoStruct") }); if (info.m_Kind == VALUE_OBJECT) TryStructNumber(info, "27", rar); }
                if (rar != 7.0 && rar != 10.0) why = "game makes it rarity " + std::to_string((int)rar);
            }
        } catch (...) { why = "exception"; }
        if (why.empty()) g_AngelicPool.push_back({ base.type, base.sub, base.b, base.name, base.angelic });
        else ++rejected;
        if (verbose) Out(std::string("angeliclist: ") + base.name + " (" + std::to_string(base.type) + "/" + std::to_string(base.sub) + "/" + std::to_string(base.b) + ") -> " + (why.empty() ? "ok" : why));
    }
    Out("angelic pool: " + std::to_string(g_AngelicPool.size()) + " candidates, " + std::to_string(rejected) + " rejected");
}
static bool SpawnAngelicItem(const AngelicCandidate& c, double x, double y, CInstance* ctx)
{
    try {
        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const long long seed = 100000000LL + (long long)(std::uniform_real_distribution<double>(0.0, 899999999.0)(TyRng()));
        const std::string key = "0-0-" + std::to_string(ms) + "-" + std::to_string(c.type);
        const std::string json = "{\"w\":1,\"a\":" + std::to_string(seed) + ",\"j\":" + std::to_string(c.sub) + ",\"b\":" + std::to_string(c.b) + ",\"c\":1}";
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        if (!g) { Out("angelicdrop: no global instance"); return false; }
        RValue parsed; g_Yytk->CallBuiltinEx(parsed, "json_parse", g, g, { RValue(json) });
        if (parsed.m_Kind != VALUE_OBJECT) { Out("angelicdrop: json_parse failed"); return false; }
        RValue item; AurieStatus st = g_Yytk->CallGameScriptEx(item, "gml_Script_InitItemFromJson", g, g, { parsed, RValue(key) });
        if (!AurieSuccess(st) || item.m_Kind != VALUE_OBJECT) { Out("angelicdrop: InitItemFromJson failed st=" + std::to_string((int)st)); return false; }
        std::string made;
        try {
            RValue info = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemInfoStruct") });
            if (info.m_Kind == VALUE_OBJECT) { double rar = -1; TryStructNumber(info, "27", rar); made = StructKey(info, "28") + " rarity " + std::to_string((int)rar); }
        } catch (...) {}
        CInstance* self = ctx ? ctx : g;
        RValue res; AurieStatus st3 = g_Yytk->CallGameScriptEx(res, "gml_Script_LootGroundCreateFromItem", self, self, { RValue(x), RValue(y), item });
        if (!AurieSuccess(st3)) { Out("angelicdrop: LootGroundCreateFromItem st=" + std::to_string((int)st3)); return false; }
        Out("angelicdrop: " + c.name + " -> " + made + " at " + std::to_string((int)x) + "," + std::to_string((int)y));
        return true;
    } catch (...) { Out("angelicdrop: EXCEPTION while spawning " + c.name); return false; }
}
static volatile long g_KillsSeen = 0;
static void AngelicDropOnKill(CInstance* S)
{
    InterlockedIncrement(&g_KillsSeen);
    if (g_AngelicDropOneIn <= 0.0 || !S) return;
    try {
        RValue enemy = S->ToRValue();
        const double rarity = HhReadNumber(enemy, "enemyRarity", -1.0);
        if (rarity < 1.0) return;   // not a monster
        InterlockedIncrement(&g_AngelicDropRolls);
        if (std::uniform_real_distribution<double>(0.0, g_AngelicDropOneIn)(TyRng()) >= 1.0) return;
        BuildAngelicPool(false);
        if (g_AngelicPool.empty()) { InterlockedIncrement(&g_AngelicDropFails); return; }
        const size_t pick = (size_t)std::uniform_int_distribution<int>(0, (int)g_AngelicPool.size() - 1)(TyRng());
        const double x = HhReadNumber(enemy, "x", 0.0), y = HhReadNumber(enemy, "y", 0.0);
        if (SpawnAngelicItem(g_AngelicPool[pick], x, y, S)) InterlockedIncrement(&g_AngelicDropHits); else InterlockedIncrement(&g_AngelicDropFails);
    } catch (...) { InterlockedIncrement(&g_AngelicDropFails); Out("angelicdrop: EXCEPTION"); }
}
static void AngelicDropStatus()
{
    Out("angelicdrop: " + (g_AngelicDropOneIn > 0.0 ? "1 in " + std::to_string((long long)g_AngelicDropOneIn) + " kills" : std::string("off"))
        + " | rolls=" + std::to_string(g_AngelicDropRolls) + " drops=" + std::to_string(g_AngelicDropHits) + " fails=" + std::to_string(g_AngelicDropFails)
        + " | pool " + (g_AngelicPoolBuilt ? std::to_string(g_AngelicPool.size()) + " candidates" : std::string("not built yet")));
}

static void SignatureDropOnKill(CInstance* S)
{
    if (g_SigDropPct <= 0.0 || !S) return;
    try {
        RValue enemy = S->ToRValue();
        const double rarity = HhReadNumber(enemy, "enemyRarity", -1.0);
        if (rarity < 1.0) return;   // not a monster (no enemyRarity)
        ++g_SigDropRolls; ++g_SigDropSinceLast;
        const double pct = rarity >= 4.0 ? g_SigDropAncientPct : g_SigDropPct;
        const bool pity = g_SigDropPity > 0 && g_SigDropSinceLast >= g_SigDropPity;
        if (!pity && !TyRoll(pct)) return;
        const double x = HhReadNumber(enemy, "x", 0.0), y = HhReadNumber(enemy, "y", 0.0);
        if (SpawnSignatureItem(g_SigDropNext, x, y, S)) { g_SigDropNext = 1 - g_SigDropNext; g_SigDropSinceLast = 0; }
    } catch (...) {}
}
static std::string SigPct(double p) { char b[32]; sprintf_s(b, "%.3g", p); std::string s(b); return s + " pct"; }
static void SigDropStatus()
{
    Out("sigdrop: every kill " + (g_SigDropPct > 0.0 ? SigPct(g_SigDropPct) : std::string("off")) + ", ancient " + SigPct(g_SigDropAncientPct)
        + ", pity " + std::to_string(g_SigDropPity) + " (since last " + std::to_string(g_SigDropSinceLast) + ") | rolls=" + std::to_string(g_SigDropRolls)
        + " drops=" + std::to_string(g_SigDropHits) + " fails=" + std::to_string(g_SigDropFails) + " next=" + (g_SigDropNext == 0 ? "crown" : "belt"));
}

// Resolve through the runner rather than YYTK 4's version-dependent room layout.
// Its stale active-list offset can make GetInstanceObject see only the last object.
static CInstance* HhResolveInstance(const RValue& value)
{
    try {
        if (value.m_Kind != VALUE_REAL && value.m_Kind != VALUE_INT32 && value.m_Kind != VALUE_INT64
            && value.m_Kind != VALUE_REF && value.m_Kind != VALUE_OBJECT) return nullptr;
        if (!g_Yytk->CallBuiltin("instance_exists", { value }).ToBoolean()) return nullptr;
        RValue id = g_Yytk->CallBuiltin("variable_instance_get", { value, RValue("id") });
        // Modern runners return the built-in id as a typed instance reference.
        // Rejecting VALUE_REF here silences both killer and local-player lookup.
        if (id.m_Kind != VALUE_REAL && id.m_Kind != VALUE_INT32 && id.m_Kind != VALUE_INT64
            && id.m_Kind != VALUE_REF) return nullptr;
        const double number = id.ToDouble();
        if (number < 0.0 || number > INT32_MAX) return nullptr;
        // The engine's named resolver returns the actual instance as VALUE_OBJECT.
        // No room offsets, persistent raw pointers or game-build addresses are used.
        RValue resolved = g_Yytk->CallBuiltin("@@GetInstance@@", { id });
        if (resolved.m_Kind != VALUE_OBJECT || !resolved.ToInstance()) return nullptr;
        if (!g_Yytk->CallBuiltin("instance_exists", { resolved }).ToBoolean()) return nullptr;
        if (g_Yytk->CallBuiltin("variable_instance_get", { resolved, RValue("id") }).ToDouble() != number) return nullptr;
        return resolved.ToInstance();
    } catch (...) {}
    return nullptr;
}

// Bodies for the snapshot/diff research commands forward-declared near
// CiDiffSnapshot above - defined here because they need HhResolveInstance,
// just defined above, plus ResolveCiProfileManagerIdx/ResolveCiPlayerId,
// defined further below alongside the rest of the citrace machinery (also
// forward-declared, near the top of this file's forward-declaration block).
// Shared by snap1 and snap2: re-resolved independently each time rather than
// cached, since MEASURED 2026-09-10 session 6 shows instance_destroy never
// fires on collect (canPickup/isActive/active look like the real marker
// instead) - the item plausibly survives the interaction and stays "nearest"
// both times, but re-finding it is cheap insurance either way.
//
// FORGEPACT_RELEASE guard note: everything from here through the B4 Phase 0
// tools below (CiFindNearestQuestItem/CiSnapTake/CiSnapDiff/CiPokeKey*/
// CiMouseReport/CiMouseWriteTest) and the Plan C Phase C0 tools after them
// (CiDumpInstance/CiReportMethods/CiInvokeMethodValue/CiEventPerform/
// CiActivateObject/CiReportHoverGlobals/CiSweepGlobals) calls into the
// citrace machinery defined
// above (CiDescribeInstance, CiSnapshotInstance, CiGetProfileManagerObjIdx,
// etc.), which only exists inside the `#ifndef FORGEPACT_RELEASE` block
// closing well before this point - those symbols do not exist in a release
// build. This whole section must stay inside the same guard or a release
// build fails to compile (confirmed live 2026-09-10: `build.bat` with no
// args, i.e. the actual player/ship build, failed here before this guard was
// added - a pre-existing gap, not introduced by the B4 additions, just
// surfaced by actually building release while adding to this section).
#ifndef FORGEPACT_RELEASE
static CInstance* CiFindNearestQuestItem(std::string* outName = nullptr, long* outPx = nullptr)
{
    ResolvePetQuestAssets();
    if (g_QuestObjParentIdx < 0) return nullptr;
    try {
        RValue player;
        if (!HhResolveLocalPlayer(player)) return nullptr;
        const double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
        const double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
        int total = (int)g_Yytk->CallBuiltin("instance_number", { RValue((double)g_QuestObjParentIdx) }).ToDouble();
        double bestD2 = -1.0; CInstance* best = nullptr;
        for (int i = 0; i < total && i < 64; ++i) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)g_QuestObjParentIdx), RValue((double)i) });
            if (inst.m_Kind == VALUE_UNDEFINED) continue;
            const double ix = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
            const double iy = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
            const double d2 = (ix - px) * (ix - px) + (iy - py) * (iy - py);
            if (bestD2 < 0.0 || d2 < bestD2) { bestD2 = d2; best = HhResolveInstance(inst); }
        }
        if (best) {
            if (outName) *outName = CiDescribeInstance(best);
            if (outPx) *outPx = (long)std::sqrt(bestD2 < 0 ? 0 : bestD2);
        }
        return best;
    } catch (...) { return nullptr; }
}

static void CiSnapTake()
{
    ResolveCiProfileManagerIdx();
    ResolveCiPlayerId();
    try { g_CiSnapRoomBefore = g_Yytk->CallBuiltin("variable_global_get", { RValue("room") }).ToDouble(); } catch (...) { g_CiSnapRoomBefore = -1.0; }
    CiSnapshotGlobals(g_CiSnapGlobalsBefore);
    try {
        RValue player;
        if (HhResolveLocalPlayer(player)) {
            CInstance* p = HhResolveInstance(player);
            CiSnapshotInstance(p, g_CiSnapPlayerBefore);
        }
    } catch (...) {}
    try {
        int pmIdx = CiGetProfileManagerObjIdx();
        if (pmIdx >= 0) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)pmIdx), RValue(0.0) });
            if (inst.m_Kind != VALUE_UNDEFINED) {
                CInstance* pm = HhResolveInstance(inst);
                CiSnapshotInstance(pm, g_CiSnapPmBefore);
            }
        }
    } catch (...) {}
    // MEASURED 2026-09-10 session 6: instance_destroy never fires on collect,
    // so the item plausibly survives and just flips a flag (canPickup/
    // isActive/active were all present on it) - diffed now, not just dumped
    // once, so that flip (if any) shows up automatically.
    try {
        std::string name; long distPx = 0;
        CInstance* item = CiFindNearestQuestItem(&name, &distPx);
        if (item) {
            CiSnapshotInstance(item, g_CiSnapItemBefore);
            Out("citrace snap1: nearest quest item = " + name + " (" + std::to_string(distPx) + " px away), " + std::to_string(g_CiSnapItemBefore.size()) + " vars captured");
        } else {
            Out("citrace snap1: no quest item instance found nearby");
        }
    } catch (...) {}
    g_CiSnapTaken = true;
    char b[200];
    sprintf_s(b, "citrace snap1: captured globals=%zu player=%zu profileManager=%zu - now hover+press F, then run 'citrace snap2'",
              g_CiSnapGlobalsBefore.size(), g_CiSnapPlayerBefore.size(), g_CiSnapPmBefore.size());
    Out(b);
}

static void CiSnapDiff()
{
    if (!g_CiSnapTaken) { Out("citrace snap2: no snap1 taken yet"); return; }
    try {
        double roomNow = g_Yytk->CallBuiltin("variable_global_get", { RValue("room") }).ToDouble();
        if (g_CiSnapRoomBefore >= 0.0 && roomNow != g_CiSnapRoomBefore) {
            Out("citrace snap2: ROOM CHANGED (" + std::to_string((long)g_CiSnapRoomBefore) + " -> " + std::to_string((long)roomNow)
                + ") - everything below is contaminated by the room/zone transition, not the collect itself. Retake snap1 without changing rooms.");
        }
    } catch (...) {}
    Out("citrace snap2: diffing against snap1...");
    CiSnapshot after;
    CiSnapshotGlobals(after);
    CiDiffSnapshot("global", g_CiSnapGlobalsBefore, after);
    try {
        RValue player;
        if (HhResolveLocalPlayer(player)) {
            CInstance* p = HhResolveInstance(player);
            CiSnapshotInstance(p, after);
            CiDiffSnapshot("player", g_CiSnapPlayerBefore, after);
        }
    } catch (...) {}
    try {
        std::string name; long distPx = 0;
        CInstance* item = CiFindNearestQuestItem(&name, &distPx);
        if (item) {
            CiSnapshotInstance(item, after);
            Out("citrace snap2: nearest quest item = " + name + " (" + std::to_string(distPx) + " px away)");
            CiDiffSnapshot("questItem", g_CiSnapItemBefore, after);
        } else {
            Out("citrace snap2: no quest item instance found nearby (destroyed, or moved out of range)");
        }
    } catch (...) {}
    try {
        int pmIdx = CiGetProfileManagerObjIdx();
        if (pmIdx >= 0) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)pmIdx), RValue(0.0) });
            if (inst.m_Kind != VALUE_UNDEFINED) {
                CInstance* pm = HhResolveInstance(inst);
                CiSnapshotInstance(pm, after);
                CiDiffSnapshot("ProfileManager", g_CiSnapPmBefore, after);
            }
        }
    } catch (...) {}
    Out("citrace snap2: done");
}

// ---- Plan B4 Phase 0 research tools (dev build only) ----------------------
// docs/pet-quest-collector-plan-b4-input-simulation.md §3a/§3b/§5. Two
// independent, narrowly-scoped probes - neither is reachable from a release
// build (this whole file section lives inside the #ifndef FORGEPACT_RELEASE
// citrace block) and neither has any path into a player-facing command:
//
//   citrace pokekey <index> [value=1] - Phase 0 item 2. Writes one element of
//   Profile_Manager_obj.inputState for exactly one frame, then explicitly
//   restores the previous value on the next FrameCallback tick. §3a treats
//   "does the array self-heal, or does it need an explicit restore" as an
//   open question to observe live; this always restores explicitly regardless
//   of the answer (strictly safer - a self-healing array just gets restored
//   to the value it would have held anyway) and removes the "stuck F held"
//   failure mode §7's risk table names, while still logging whether the
//   value had already changed back on its own before the explicit restore
//   ran, so the self-heal question still gets answered.
//   CORRECTION (MEASURED 2026-09-10, live session, see
//   docs/pet-quest-collector-b4-research.md item 1): a real F press flips
//   inputState[30] AND inputState[60] from 1 to 0, not the other way around
//   - the reverse of what this command's own default value once assumed.
//   Call it as `citrace pokekey 30 0` (and likely also `citrace pokekey 60
//   0`), not with the default - the default is left at 1 only because this
//   is a generic one-element poke, not because 1 is expected to do anything
//   useful here.
//
//   citrace mouse / citrace mousewrite <x> <y> - Phase 0 item 3. `mouse`
//   reads mouse_x/mouse_y via variable_instance_get exactly as §5 item 3
//   specifies, meant to be called repeatedly while the tester moves the real
//   cursor to confirm the correlation. `mousewrite` is a cheap write-then-
//   readback test of the same two variables, which §3b expects to be
//   GML-level read-only; a value that *does* stick would itself be a
//   significant, unexpected finding worth logging. Neither command touches
//   OS input - this is not B4b-ii, only a diagnostic probe, and the write
//   test restores the original values immediately regardless of outcome.
static bool CiGetProfileManagerInstance(RValue& outInst)
{
    int pmIdx = CiGetProfileManagerObjIdx();
    if (pmIdx < 0) return false;
    try {
        RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)pmIdx), RValue(0.0) });
        if (inst.m_Kind == VALUE_UNDEFINED) return false;
        outInst = inst;
        return true;
    } catch (...) { return false; }
}

// MEASURED 2026-09-10, live session (docs/pet-quest-collector-b4-research.md
// item 1): a real F press flips TWO inputState elements together (30 and 60
// - see the correction in the comment block above), so testing either index
// alone (both tried live, neither did anything visible) doesn't rule out
// that the game's check requires both to read 0 in the same frame. Sized for
// exactly that: up to 2 simultaneous indices, one shared restore tick.
static constexpr int kCiPokeMaxSlots = 2;
static std::atomic<bool> g_CiPokeArmed{ false };
static int g_CiPokeIndex[kCiPokeMaxSlots] = { -1, -1 };
static double g_CiPokeOldValue[kCiPokeMaxSlots] = { 0.0, 0.0 };
static int g_CiPokeCount = 0;
static int g_CiPokeFramesLeft = 0;

static void CiPokeKeysArmed(const int* indices, int count, double value)
{
    if (g_CiPokeArmed.load()) { Out("citrace pokekey: a previous poke's restore is still pending - wait one frame and retry"); return; }
    if (count < 1 || count > kCiPokeMaxSlots) { Out("citrace pokekey: EXCEPTION (bad index count)"); return; }
    RValue pm;
    if (!CiGetProfileManagerInstance(pm)) { Out("citrace pokekey: Profile_Manager_obj instance not found"); return; }
    try {
        RValue arr = g_Yytk->CallBuiltin("variable_instance_get", { pm, RValue("inputState") });
        if (arr.m_Kind != VALUE_ARRAY) { Out("citrace pokekey: inputState is not an array (" + Describe(arr) + ")"); return; }
        int len = (int)g_Yytk->CallBuiltin("array_length", { arr }).ToDouble();
        for (int i = 0; i < count; ++i) {
            if (indices[i] < 0 || indices[i] >= len) { Out("citrace pokekey: index " + std::to_string(indices[i]) + " out of range (len=" + std::to_string(len) + ")"); return; }
        }
        std::string log = "citrace pokekey:";
        for (int i = 0; i < count; ++i) {
            RValue old = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)indices[i]) });
            double oldNum = 0.0; try { oldNum = old.ToDouble(); } catch (...) {}
            g_Yytk->CallBuiltin("array_set", { arr, RValue((double)indices[i]), RValue(value) });
            g_CiPokeIndex[i] = indices[i];
            g_CiPokeOldValue[i] = oldNum;
            log += " inputState[" + std::to_string(indices[i]) + "] " + Describe(old) + " -> " + std::to_string(value);
        }
        g_CiPokeCount = count;
        g_CiPokeFramesLeft = 1;
        g_CiPokeArmed.store(true);
        Out(log + " for one frame, simultaneously (auto-restores next tick) - now check in-game whether the interact prompt/item responds without a real F press");
    } catch (...) { Out("citrace pokekey: EXCEPTION"); }
}

static void CiPokeKey(int index, double value) { CiPokeKeysArmed(&index, 1, value); }
static void CiPokeKeys2(int idx1, int idx2, double value) { int idx[2] = { idx1, idx2 }; CiPokeKeysArmed(idx, 2, value); }

// Called every frame from FrameCallback (research build only), independent of
// the `petquest`/`citrace <on|off>` toggles - a pending restore must land
// even if tracing itself is off.
static void CiPokeKeyTick()
{
    if (!g_CiPokeArmed.load()) return;
    if (--g_CiPokeFramesLeft > 0) return;
    g_CiPokeArmed.store(false);
    RValue pm;
    if (!CiGetProfileManagerInstance(pm)) { Out("citrace pokekey: restore FAILED - Profile_Manager_obj instance not found (poke may be left stuck)"); return; }
    try {
        RValue arr = g_Yytk->CallBuiltin("variable_instance_get", { pm, RValue("inputState") });
        if (arr.m_Kind != VALUE_ARRAY) { Out("citrace pokekey: restore FAILED - inputState no longer an array"); return; }
        for (int i = 0; i < g_CiPokeCount; ++i) {
            RValue current = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)g_CiPokeIndex[i]) });
            double curNum = 0.0; try { curNum = current.ToDouble(); } catch (...) {}
            g_Yytk->CallBuiltin("array_set", { arr, RValue((double)g_CiPokeIndex[i]), RValue(g_CiPokeOldValue[i]) });
            Out("citrace pokekey: restored inputState[" + std::to_string(g_CiPokeIndex[i]) + "] to " + std::to_string(g_CiPokeOldValue[i])
                + " (" + (curNum == g_CiPokeOldValue[i] ? "already back to that value on its own - looks self-healing" : "was still " + std::to_string(curNum) + " just before this explicit restore") + ")");
        }
    } catch (...) { Out("citrace pokekey: restore EXCEPTION - poke may be left stuck, check inputState manually"); }
}

static void CiMouseReport()
{
    RValue player;
    if (!HhResolveLocalPlayer(player)) { Out("citrace mouse: no player instance"); return; }
    try {
        RValue mx = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("mouse_x") });
        RValue my = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("mouse_y") });
        std::string line = "citrace mouse: mouse_x=" + Describe(mx) + " mouse_y=" + Describe(my);
        RValue pm;
        if (CiGetProfileManagerInstance(pm)) {
            try {
                RValue exPrev = g_Yytk->CallBuiltin("variable_instance_exists", { pm, RValue("mouse_x_prev") });
                if (exPrev.ToBoolean()) {
                    RValue px = g_Yytk->CallBuiltin("variable_instance_get", { pm, RValue("mouse_x_prev") });
                    RValue py = g_Yytk->CallBuiltin("variable_instance_get", { pm, RValue("mouse_y_prev") });
                    line += " | ProfileManager.mouse_x_prev=" + Describe(px) + " mouse_y_prev=" + Describe(py);
                }
            } catch (...) {}
        }
        Out(line);
    } catch (...) { Out("citrace mouse: EXCEPTION reading mouse_x/mouse_y"); }
}

static void CiMouseWriteTest(double x, double y)
{
    RValue player;
    if (!HhResolveLocalPlayer(player)) { Out("citrace mousewrite: no player instance"); return; }
    try {
        RValue oldX = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("mouse_x") });
        RValue oldY = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("mouse_y") });
        g_Yytk->CallBuiltin("variable_instance_set", { player, RValue("mouse_x"), RValue(x) });
        g_Yytk->CallBuiltin("variable_instance_set", { player, RValue("mouse_y"), RValue(y) });
        RValue newX = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("mouse_x") });
        RValue newY = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("mouse_y") });
        // best-effort restore regardless of whether the write actually stuck
        try {
            g_Yytk->CallBuiltin("variable_instance_set", { player, RValue("mouse_x"), oldX });
            g_Yytk->CallBuiltin("variable_instance_set", { player, RValue("mouse_y"), oldY });
        } catch (...) {}
        bool stuck = (newX.ToDouble() == x && newY.ToDouble() == y);
        Out(std::string("citrace mousewrite: wrote (") + std::to_string(x) + "," + std::to_string(y) + ") readback (" + Describe(newX) + "," + Describe(newY) + ") "
            + (stuck ? "STUCK - GML-level write succeeded, unexpected, investigate further" : "did NOT stick (read-only, as §3b expected) - direct storage still needs locating")
            + " | restored original (" + Describe(oldX) + "," + Describe(oldY) + ")");
    } catch (...) { Out("citrace mousewrite: EXCEPTION"); }
}

// ---- Plan C Phase C0 research tools (dev build only) ----------------------
// docs/pet-quest-collector-plan-c-direct-invocation.md §2. Plans B1, B2 and B4
// all died on the same assumption - that the code we want must be reached by
// *name* - and session 7 proved that wall is hard (22 raw object-event names,
// all "not found" at hook-install time, before any live test was needed).
// Plan C attacks the assumption instead, through the two doors that use no
// name at all:
//
//   1. the callable is already sitting in a variable on the item
//      (m_QuestActivate/m_QuestActive/m_QuestDestructible/m_QuestInteract/
//      m_Questpickup/m_QuestUseKey, found session 6) - a GameMaker method
//      *value* can be invoked without ever appearing in the name table,
//      because we already hold it;
//   2. event_perform(type, number) runs an object's own event code by numeric
//      id - reaching exactly the object-event code HookRawNamedRoutine could
//      not resolve by name.
//
// Everything here is batched into ONE build and ONE relaunch, per agents.md's
// "hook every candidate in the same build" rule and the plan's §6 live-session
// cost row - seven checklist items, one round trip, not seven.
//
// Read-only (safe to run any time, no confirm token):
//   citrace item                     - C0.1/C0.4: nearest quest item, every
//                                      variable, container-expanded and
//                                      method-resolved.
//   citrace methods                  - C0.1 + C0.2 step 1: just the six
//                                      m_Quest* methods, what each resolves
//                                      to, plus which method-invocation
//                                      builtins this runtime actually exposes.
//                                      Settles the plan's "open question to
//                                      settle in step 1, not assume" without
//                                      invoking anything.
//   citrace dumpobj <ObjName> [nth]  - C0.4: full method-resolved dump of any
//                                      object instance. Quest_Manager_obj has
//                                      never been dumped at all;
//                                      Controller_obj is known to hold live
//                                      game state.
//   citrace player                   - C0.4: the same for the local player.
//   citrace globals                  - C0.5: the hover/target globals read as
//                                      *values* (session 4 hooked them and
//                                      measured 0 calls, but never read them).
//   citrace sweep [extra ...]        - C0.5: broadened `dump` sweep over the
//                                      plan's keyword list, with the
//                                      container/method expansion plain `dump`
//                                      lacks.
//
// Mutating (require a literal `confirm` token - see the safety block below):
//   citrace invoke item|global <name> confirm          - C0.2
//   citrace event <type> <number> confirm              - C0.3
//   citrace eventobj <ObjName> <type> <number> confirm - C0.3
//   citrace activate [ObjName] confirm                 - C0.6
//
// C0.7 needs no new code: `petquest 1` + `petquest stat` already counts family
// enumeration and camera-bounds stability, and has simply never been read in a
// session with quest items on screen.
//
// SAFETY (plan §4, binding from C0.2 onward). This is the first point in the
// whole investigation where the plugin *invokes* rather than *observes*, so a
// wrong call can remove a quest item without crediting its objective - which
// the original plan's §11 calls worse than no mod at all. Enforced here:
//   - a literal `confirm` token on every mutating command, so a stray or
//     half-pasted line in cmd.txt fails closed instead of firing;
//   - a once-per-session reminder banner listing the plan's manual
//     preconditions (save backup, non-progress-gated item first, fresh
//     character), printed before the first mutation;
//   - one call per command invocation, never a loop over the six methods
//     (plan §4 rule 4: "one item, one call, one observation");
//   - item marker flags captured before AND after every mutation and printed
//     as a delta, plus the family instance count, plus a best-effort quest
//     progress read - so "the item vanished" can be told apart from "the
//     objective advanced" rather than mistaken for it (plan §4 rule 3).

// The marker fields session 6's item dump found on Quest_Act_01_Body_Part_obj.
// Captured before and after every mutating C0 command: the plan's §6 second
// risk row asks for item flags specifically, not just quest progress, because
// a method invoked out of context could half-complete an interaction in a way
// quest progress alone would not show.
static const char* const kCiItemMarkerFields[] = {
    "canPickup", "isActive", "active", "questIndex", "questObjectType",
    "questObjectiveNumber", "distanceForPickup", "activateQuestObjectWithMouse",
    "itemActive", "x", "y", "visible",
};

static std::string CiItemFlags(CInstance* item)
{
    if (!item) return "(no item)";
    std::string s;
    try {
        RValue id = item->ToRValue();
        for (const char* f : kCiItemMarkerFields) {
            try {
                RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue(std::string(f)) });
                if (!ex.ToBoolean()) continue;
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(std::string(f)) });
                s += (s.empty() ? "" : " ") + std::string(f) + "=" + Describe(v);
            } catch (...) {}
        }
    } catch (...) { return "(flags read failed)"; }
    return s.empty() ? "(no marker fields present)" : s;
}

// How many instances of the quest-item family the engine currently enumerates.
// MEASURED session 6: a collected item drops out of instance_number /
// instance_find while instance_destroy stays at 0 - the deactivation
// signature. Printed alongside the flags so a mutation that makes an item
// vanish is visible even when the instance handle itself has gone stale.
static int CiQuestFamilyCount()
{
    ResolvePetQuestAssets();
    if (g_QuestObjParentIdx < 0) return -1;
    try { return (int)g_Yytk->CallBuiltin("instance_number", { RValue((double)g_QuestObjParentIdx) }).ToDouble(); }
    catch (...) { return -1; }
}

// Plan §4 rule 3: verify progress, not disappearance. Signature unverified -
// gml_Script_GetQuestProgress is a real name-table entry (hs-game-sdk indexes
// it) but nothing has measured what it takes, so this passes the item's own
// questIndex and reports whatever comes back, labelled unverified rather than
// presented as a reading. Both scripts are read-shaped ("Get...") and the call
// is wrapped, so a wrong arity degrades to undefined instead of mutating
// anything.
static std::string CiQuestProgressReport(CInstance* item)
{
    if (!item) return "";
    double questIndex = -1.0;
    try {
        RValue id = item->ToRValue();
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue("questIndex") });
        if (!ex.ToBoolean()) return " | questIndex absent, no progress read attempted";
        questIndex = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("questIndex") }).ToDouble();
    } catch (...) { return " | questIndex read failed"; }

    // MEASURED 2026-09-11: CallGameScriptEx throws in this runtime (see the
    // boundary table above CiInvokeMethodValue), while the non-Ex
    // CallGameScript works - `call GetQuestProgress` returned real:-1 cleanly
    // in the same session. So this uses the non-Ex form, which means the call
    // runs in global context with no explicit self; that is a real limitation
    // (a script that reads `self` will see the wrong instance) and is why the
    // result stays labelled unverified.
    std::string out = " | questIndex=" + std::to_string((long)questIndex);
    static const char* const kProgressScripts[] = { "gml_Script_GetQuestProgress", "gml_Script_GetQuestMaxProgress" };
    for (const char* scr : kProgressScripts) {
        try {
            PVOID probe = nullptr;
            if (!AurieSuccess(g_Yytk->GetNamedRoutinePointer(scr, &probe)) || !probe) {
                out += std::string(" ") + scr + "=(not in name table)";
                continue;
            }
            RValue res = g_Yytk->CallGameScript(scr, { RValue(questIndex) });
            out += std::string(" ") + scr + "(unverified sig, global ctx)=" + Describe(res);
        }
        catch (const std::exception& e) { out += std::string(" ") + scr + "=threw " + e.what(); }
        catch (...) { out += std::string(" ") + scr + "=threw (non-std)"; }
    }
    return out;
}

// Plan §4's manual preconditions cannot be enforced from inside the plugin -
// only stated, once, immediately before the first thing that could break a
// quest chain. Printed to out.txt, where the tester is already reading
// results.
static std::atomic<bool> g_CiSafetyBannerShown{ false };
static void CiSafetyBanner()
{
    if (g_CiSafetyBannerShown.exchange(true)) return;
    Out("citrace SAFETY (plan C 4) - first command this session that INVOKES game code rather than observing it:");
    Out("  1. back up %LOCALAPPDATA%\\Hero_Siege\\ (and any cloud-sync copy) before continuing;");
    Out("  2. test on Quest_Toy_Bear_obj (Act 8, Zone 8-4) first - confirmed to stay collectible after its quest completes;");
    Out("  3. verify quest PROGRESS after every call, not just that the item disappeared - a vanish without credit is worse than no mod;");
    Out("  4. one item, one call, one observation - no loops, no batching;");
    Out("  5. prefer a fresh character for destructive testing.");
}

// `confirm` gate for every mutating C0 command. Deliberately a literal word
// rather than a numeric flag: a mis-parsed or half-pasted command line then
// fails closed instead of firing.
static bool CiConfirmed(const std::string& token, const char* usage)
{
    if (Lower(token) == "confirm") { CiSafetyBanner(); return true; }
    Out(std::string("citrace: refused - this command mutates game state. Usage -> ") + usage);
    return false;
}

// ---- C0.1 / C0.4: method-resolved instance dumps --------------------------
// CiSnapshotInstance already routes every variable through CiExpandContainer,
// which already calls CiTryResolveMethod - the resolver session 7 built to
// answer exactly this question and which, checked against the full out.txt
// history, has never once been run (no "->method:" line has ever been
// logged). Nothing new has to resolve anything; what was missing is a command
// that *prints* the dump instead of only counting it, so that is all this is.
static void CiDumpInstance(const std::string& label, CInstance* inst)
{
    if (!inst) { Out("citrace " + label + ": no instance"); return; }
    CiSnapshot snap;
    CiSnapshotInstance(inst, snap);
    Out("citrace " + label + ": " + CiDescribeInstance(inst) + " - " + std::to_string(snap.size()) + " variables");
    // Sorted so two dumps of the same object are diffable by eye, and so the
    // m_Quest* group lands together instead of in hash order.
    std::vector<std::string> names;
    names.reserve(snap.size());
    for (const auto& kv : snap) names.push_back(kv.first);
    std::sort(names.begin(), names.end());
    int printed = 0;
    constexpr int kMaxDumpLines = 200;
    for (const std::string& n : names) {
        std::string v = snap[n];
        if (v.size() > 400) v = v.substr(0, 400) + "...(truncated)";
        Out("  " + n + " = " + v);
        if (++printed >= kMaxDumpLines) { Out("  ...(truncated at " + std::to_string(kMaxDumpLines) + " variables)"); break; }
    }
}

static void CiDumpNearestQuestItem()
{
    std::string name; long distPx = 0;
    CInstance* item = CiFindNearestQuestItem(&name, &distPx);
    if (!item) { Out("citrace item: no quest item instance found nearby (family count=" + std::to_string(CiQuestFamilyCount()) + ")"); return; }
    Out("citrace item: nearest = " + name + " (" + std::to_string(distPx) + " px away), family count=" + std::to_string(CiQuestFamilyCount()));
    CiDumpInstance("item", item);
}

static void CiDumpNamedObject(const std::string& objName, int nth)
{
    if (objName.empty()) { Out("citrace dumpobj: usage -> citrace dumpobj <ObjectName> [nth=0]"); return; }
    try {
        int idx = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) }).ToDouble();
        if (idx < 0) { Out("citrace dumpobj: object '" + objName + "' not found"); return; }
        int total = (int)g_Yytk->CallBuiltin("instance_number", { RValue((double)idx) }).ToDouble();
        if (total <= 0) { Out("citrace dumpobj: '" + objName + "' (#" + std::to_string(idx) + ") has no live instances"); return; }
        if (nth < 0 || nth >= total) { Out("citrace dumpobj: nth=" + std::to_string(nth) + " out of range (" + std::to_string(total) + " live instances)"); return; }
        RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)idx), RValue((double)nth) });
        if (inst.m_Kind == VALUE_UNDEFINED) { Out("citrace dumpobj: instance_find returned undefined"); return; }
        Out("citrace dumpobj: '" + objName + "' instance " + std::to_string(nth) + " of " + std::to_string(total));
        CiDumpInstance("dumpobj", HhResolveInstance(inst));
    } catch (...) { Out("citrace dumpobj: EXCEPTION"); }
}

static void CiDumpPlayer()
{
    RValue player;
    if (!HhResolveLocalPlayer(player)) { Out("citrace player: no player instance"); return; }
    CiDumpInstance("player", HhResolveInstance(player));
}

// ---- C0.2 step 1: how does THIS runtime accept a method value? ------------
// The plan lists four candidate invocation paths and says explicitly to
// settle which the runtime accepts rather than assume. Probing the name table
// costs nothing and calls nothing, so it happens here, read-only, before any
// invoke is attempted - and the invoke path below then tries them in the
// plan's stated order.
static const char* const kCiInvokeBuiltins[] = {
    "script_execute", "method_call", "method_get_index", "method_get_self", "script_get_name",
};

static const char* const kCiQuestMethodNames[] = {
    "m_QuestActivate", "m_QuestActive", "m_QuestDestructible",
    "m_QuestInteract", "m_Questpickup", "m_QuestUseKey",
};

static void CiReportMethods()
{
    Out("citrace methods: invocation builtins available in this runtime:");
    for (const char* b : kCiInvokeBuiltins) {
        PVOID p = nullptr;
        AurieStatus st = g_Yytk->GetNamedRoutinePointer(b, &p);
        Out(std::string("  ") + b + " -> " + ((AurieSuccess(st) && p) ? "present" : "NOT FOUND (st=" + std::to_string((int)st) + ")"));
    }

    std::string name; long distPx = 0;
    CInstance* item = CiFindNearestQuestItem(&name, &distPx);
    if (!item) { Out("citrace methods: no quest item instance found nearby - the m_Quest* half needs one in range"); return; }
    Out("citrace methods: nearest = " + name + " (" + std::to_string(distPx) + " px away)");
    try {
        RValue id = item->ToRValue();
        for (const char* mn : kCiQuestMethodNames) {
            try {
                RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue(std::string(mn)) });
                if (!ex.ToBoolean()) { Out(std::string("  ") + mn + " = (absent on this instance)"); continue; }
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(std::string(mn)) });
                std::string resolved = CiTryResolveMethod(v);
                Out(std::string("  ") + mn + " = " + Describe(v) + (resolved.empty() ? " (no script index - not a bound method, or unresolvable)" : resolved));
            } catch (...) { Out(std::string("  ") + mn + " = EXCEPTION"); }
        }
    } catch (...) { Out("citrace methods: EXCEPTION enumerating item methods"); }
    Out("citrace methods: read-only - nothing was invoked. Use `citrace invoke item <name> confirm` for that.");
}

// ---- C0.2: invoke a method value directly (the main event) ----------------
// MEASURED 2026-09-11, live session, twice. First attempt: three paths, all
// faulted. Rewritten with seven selectable call shapes and real fault
// reporting, redeployed, retried: **all seven fault with access violations**,
// for a method wrapping an anon closure (m_QuestActive) and for one wrapping
// an ordinary named script (global.GetMouseTarget) alike.
//
// The control test that interprets this - and it is the important part,
// because the first reading of it was wrong:
//
//     call GetQuestProgress   ->  real:-1.000000      (works)
//     call GetMouseTarget     ->  EXCEPTION           (faults)
//
// Both run through the PRE-EXISTING `call` command (DoCall -> CallGameScript),
// identical in shape, neither touched by this work. One works and one faults,
// so the variable is the script being called, not the call mechanism. An
// earlier version of this comment claimed the fault line was Ex vs non-Ex;
// that was inferred from two data points and is WRONG - CallBuiltinEx is fine
// with method values and with instance self/other (event_perform proves both).
//
// What actually happens: these YYC scripts dereference their arguments and
// `self` without validating them, so a cold call - global context, no
// arguments, no captured scope - faults inside the game's own code.
// GetQuestProgress is simply defensive enough to return -1 instead.
//
// So the premise Plan C rests on ("we already hold the callable, so it need
// not be found by name") is not sufficient: holding the callable was never the
// hard part - supplying the call *context* is. Two things remain untried, and
// this build adds both:
//
//   1. InvokeWithObject - YYTK's real `with`-style instance context, rather
//      than passing a self *pointer* into a call. spawnforce uses it
//      successfully elsewhere in this file. This is the only route that
//      establishes context the way the runtime itself does.
//   2. Arguments. Every call so far passed none, which is the single most
//      likely reason a script dereferencing arg[0] would fault. There is no
//      builtin that reports a script's arity, so this cannot be derived - it
//      has to be swept by hand, which is what the [args...] parameter is for.
// MEASURED 2026-09-11, live, on a real Quest_Act_01_Brick_obj collect
// (citrace nativetrace): the game invokes the item's own m_Questpickup
// through the runtime's call-a-method-value helper at exe+0xB489070, whose
// signature the same session read off its decompiled body:
//
//   void (CInstance* self, CInstance* other, RValue* result,
//         int argc, RValue* methodValue, RValue** args)
//
// - the same shape as the builtin dispatcher, with a method value in place of
// a numeric builtin id. That is the one call shape known to work, because it
// is the one the game makes: self = the item, other = Loot_Manager_obj, one
// real argument. Every shape C0.2 tried faulted; none of them was this.
//
// DEMOTED 2026-09-11, second pass. This address shipped for exactly one
// build before the same review that wrote agents.md's "Never Call an Address
// You Resolved by Hand" caught it. The mod no longer uses it: InvokeMethodValue
// (next to PetQuestCollectorTick) reads the callable straight off the method
// value's CScriptRef, which is what this dispatcher looks up anyway, and needs
// no address at all.
//
// What survives here, dev build only, is the A/B comparison: a researcher can
// still run the original shape against the address-free one on the same item
// in the same session and see they do the same thing. It is validated before
// being called now too - a wrong RVA on a future build must report a wrong
// address, not crash the game - and the address is printed on every call.
static constexpr unsigned long long kCiCallMethodFnRva = 0xB489070ull;
typedef void (*QuestPickupCallFn)(CInstance*, CInstance*, RValue*, int, RValue*, RValue**);

static bool InvokeMethodValueNative(CInstance* selfInst, CInstance* otherInst, const RValue& methodValue,
                                    const std::vector<RValue>& args, RValue& resultOut)
{
    if (!selfInst) return false;
    HMODULE mod = GetModuleHandleA(nullptr);
    if (!mod) return false;
    void* target = (void*)((char*)mod + kCiCallMethodFnRva);
    // The check the first version of this function did not have.
    if (!AddrIsExecutableInModule(mod, target)) return false;
    QuestPickupCallFn fn = (QuestPickupCallFn)target;
    // The helper copies what it needs onto its own stack and does not write
    // back through the method or argument pointers, so const_cast here is
    // borrowing - and it avoids RValue copy semantics on a value that owns a
    // reference to the method object.
    std::vector<RValue*> argPtrs;
    argPtrs.reserve(args.size());
    for (const RValue& a : args) argPtrs.push_back(const_cast<RValue*>(&a));
    fn(selfInst, otherInst, &resultOut, (int)argPtrs.size(),
       const_cast<RValue*>(&methodValue), argPtrs.empty() ? nullptr : argPtrs.data());
    return true;
}

enum class CiInvokePath {
    ScriptRef,          // the method value's own CScriptRef callable - what the mod ships
    NativeMethodValue,  // exe+0xB489070(self, other, &result, argc, &method, args) - the measured shape
    WithBuiltin,        // InvokeWithObject(item) { CallBuiltin  script_execute(method, args...) }
    WithBuiltinEx,      // InvokeWithObject(item) { CallBuiltinEx script_execute(method, args...) }
    BuiltinMethod,      // CallBuiltin  script_execute(method, args...)
    BuiltinExMethod,    // CallBuiltinEx script_execute(method, args...)
    BuiltinIndex,       // CallBuiltin  script_execute(<real index>, args...)
    BuiltinExIndex,     // CallBuiltinEx script_execute(<real index>, args...)
    MethodCall,         // CallBuiltin  method_call(method, [args...])
    ScriptByName,       // CallGameScript   gml_Script_<name>(args...)
    ScriptExByName,     // CallGameScriptEx gml_Script_<name>(args...)
};

struct CiInvokePathInfo { CiInvokePath path; const char* key; const char* label; };

// `key` is what a tester types as the optional [path] argument.
//
// `scriptref` comes first because it is what the mod ships: the measured call
// shape (self = the item, other = Loot_Manager_obj, one real argument) reached
// through the method value's own CScriptRef rather than through a fixed
// address. `native` is second and is the same call made the old way, kept as
// an A/B check that the two really are one mechanism - nothing else should
// use it.
//
// The nine shapes after those are measured negative (C0.2) and are kept only
// so a future session can re-run them without rebuilding. Note what the
// research doc concluded later (pet-quest-collector-c-research.md, "This
// explains every C0.2 access violation"): they were swept with no argument
// and no way to set `self`, both of which `collect` now supplies - so those
// negatives were measured under conditions that no longer hold, and are
// "not observed", not "does not work".
static const CiInvokePathInfo kCiInvokePaths[] = {
    { CiInvokePath::ScriptRef,       "scriptref", "the method value's own CScriptRef callable (what the mod ships - no fixed address)" },
    { CiInvokePath::NativeMethodValue, "native", "exe+0xB489070 call-a-method-value (the old fixed-address shape, A/B only)" },
    { CiInvokePath::WithBuiltin,     "with",      "InvokeWithObject + CallBuiltin script_execute" },
    { CiInvokePath::WithBuiltinEx,   "withex",    "InvokeWithObject + CallBuiltinEx script_execute" },
    { CiInvokePath::BuiltinMethod,   "builtin",   "CallBuiltin script_execute(method)" },
    { CiInvokePath::BuiltinExMethod, "builtinex", "CallBuiltinEx script_execute(method)" },
    { CiInvokePath::BuiltinIndex,    "index",     "CallBuiltin script_execute(index)" },
    { CiInvokePath::BuiltinExIndex,  "indexex",   "CallBuiltinEx script_execute(index)" },
    { CiInvokePath::MethodCall,      "methodcall","CallBuiltin method_call(method, [])" },
    { CiInvokePath::ScriptByName,    "script",    "CallGameScript gml_Script_<name>" },
    { CiInvokePath::ScriptExByName,  "scriptex",  "CallGameScriptEx gml_Script_<name>" },
};

// Pull the wrapped script's asset name out of CiTryResolveMethod's decorated
// string (" ->method:<name>#<index>"), or "" if it did not resolve.
static std::string CiResolvedScriptName(const RValue& methodValue)
{
    const std::string resolved = CiTryResolveMethod(methodValue);
    const std::string marker = " ->method:";
    const size_t at = resolved.find(marker);
    if (at == std::string::npos) return "";
    std::string name = resolved.substr(at + marker.size());
    const size_t hash = name.rfind('#');
    if (hash != std::string::npos) name = name.substr(0, hash);
    return name;
}

// Outcome of one path, kept richer than a bool so `auto` can stop on the first
// access violation rather than deliberately triggering eight more. An AV is
// caught and survivable (measured: the game kept running and ordinary calls
// still worked afterwards), but it is a real fault inside the runtime and is
// not worth repeating casually.
enum class CiPathResult { Invoked, Failed, AccessViolation };

static CiPathResult CiRunInvokePath(CiInvokePath path, const RValue& methodValue,
                                    const std::vector<RValue>& extraArgs,
                                    CInstance* selfInst, CInstance* otherInst,
                                    std::string& detail, std::string& resultOut)
{
    const char* label = "?";
    for (const CiInvokePathInfo& p : kCiInvokePaths) if (p.path == path) label = p.label;

    // script_execute(target, args...) - the method/index goes first, the
    // caller's extra arguments after it.
    auto withTarget = [&](const RValue& target) {
        std::vector<RValue> v;
        v.reserve(extraArgs.size() + 1);
        v.push_back(target);
        for (const RValue& a : extraArgs) v.push_back(a);
        return v;
    };

    try {
        switch (path) {
        case CiInvokePath::ScriptRef: {
            if (!selfInst) { detail = std::string(label) + " - no self instance to call with"; return CiPathResult::Failed; }
            // Routed through the shipped helper deliberately: the research
            // command and the mod must make the identical call, or a green
            // `citrace collect` would stop being evidence about the mod.
            RValue res;
            if (!InvokeMethodValue(selfInst, otherInst, methodValue, extraArgs, res)) {
                detail = std::string(label) + " - refused: the value is not a script reference, carries no"
                         " callable inside Hero_Siege.exe, or is bound to a different instance."
                         " Nothing was called. Check CScriptRef against the YYToolkit headers this was built with.";
                return CiPathResult::Failed;
            }
            resultOut = Describe(res);
            detail = std::string(label) + " - called with argc=" + std::to_string((int)extraArgs.size());
            return CiPathResult::Invoked;
        }
        case CiInvokePath::NativeMethodValue: {
            if (!selfInst) { detail = std::string(label) + " - no self instance to call with"; return CiPathResult::Failed; }
            HMODULE mod = GetModuleHandleA(nullptr);
            if (!mod) { detail = std::string(label) + " - no main module"; return CiPathResult::Failed; }
            // A/B comparison against `scriptref` only. Never the mod's path.
            RValue res;
            char addr[64];
            sprintf_s(addr, " at exe+0x%llX", kCiCallMethodFnRva);
            if (!InvokeMethodValueNative(selfInst, otherInst, methodValue, extraArgs, res)) {
                detail = std::string(label) + addr + " - call refused (no self, or that address is not executable"
                         " code in Hero_Siege.exe on this build - which is exactly why the mod no longer uses it)";
                return CiPathResult::Failed;
            }
            resultOut = Describe(res);
            detail = std::string(label) + addr + " - called with argc=" + std::to_string((int)extraArgs.size());
            return CiPathResult::Invoked;
        }
        case CiInvokePath::WithBuiltin:
        case CiInvokePath::WithBuiltinEx: {
            if (!selfInst) { detail = std::string(label) + " - no self instance to enter"; return CiPathResult::Failed; }
            // MEASURED 2026-09-11, live session, round 1: passing the raw
            // `id` RValue (VALUE_REF on this runner - see HhResolveInstance's
            // own comment on "modern runners return the built-in id as a
            // typed instance reference") made InvokeWithObject return
            // AURIE_NOT_IMPLEMENTED before the callback ever ran.
            //
            // MEASURED round 2: coercing that id to a plain VALUE_REAL fixed
            // the NOT_IMPLEMENTED fault, but produced AURIE_OBJECT_NOT_FOUND
            // instead - the function's own doc ("for EACH instance matching
            // Object") and both of this file's existing working call sites
            // (spawnforce, the menu-room resolver) confirm why: Object is an
            // object *type* index, not a specific instance's id. No object
            // type numbered ~300000 exists, so nothing matched.
            //
            // Fix (round 2): pass the target's own object_index (its type),
            // which is what the API actually wants. This means the callback
            // below may run once per *every* live instance of that type in
            // the room, not just the one hovered - the id-match guard
            // (already present, unchanged) is what keeps this to "one item,
            // one call" (plan 4 rule 4) regardless: every non-matching
            // instance is skipped silently, and `matched` prevents ever
            // acting twice even if two instances somehow shared an id.
            //
            // MEASURED round 3: round 2's object_index fetch went back in
            // uncoerced and reproduced round 1's exact failure
            // (AURIE_NOT_IMPLEMENTED) - object_index comes back VALUE_REF on
            // this runner too (Describe() showed "kind=15 str=ref object
            // Quest_Act_01_Brick_obj"), the same typed-reference kind `id`
            // needed coercing out of. InvokeWithObject only accepts a plain
            // VALUE_REAL Object argument, full stop, regardless of what it
            // represents - so every value handed to it needs the same
            // ToDouble()-and-rewrap treatment `targetId` already gets below,
            // not just the one that happened to get it first.
            double targetId = -1.0, objectTypeNum = -1.0;
            try {
                RValue self = selfInst->ToRValue();
                targetId = g_Yytk->CallBuiltin("variable_instance_get", { self, RValue("id") }).ToDouble();
                objectTypeNum = g_Yytk->CallBuiltin("variable_instance_get", { self, RValue("object_index") }).ToDouble();
            }
            catch (...) { detail = std::string(label) + " - could not read id/object_index off the target instance"; return CiPathResult::Failed; }
            const RValue objectType(objectTypeNum);
            const std::vector<RValue> args = withTarget(methodValue);
            bool ok = false, av = false, matched = false;
            std::string inner, res;
            // The lambda catches its own faults: letting one unwind back out
            // through YYTK's with-machinery would leave the runtime's context
            // stack in an unknown state.
            AurieStatus ws = g_Yytk->InvokeWithObject(objectType, [&](CInstance* s, CInstance* o) {
                try {
                    if (matched) return;   // already handled the real target this call - never act twice
                    if (!s) return;
                    RValue sid = g_Yytk->CallBuiltin("variable_instance_get", { s->ToRValue(), RValue("id") });
                    if (sid.ToDouble() != targetId) return;   // some other instance of the same enumeration - not our target
                    matched = true;
                    if (path == CiInvokePath::WithBuiltin) {
                        RValue r = g_Yytk->CallBuiltin("script_execute", args);
                        res = Describe(r); ok = true;
                    } else {
                        RValue r;
                        AurieStatus st = g_Yytk->CallBuiltinEx(r, "script_execute", s, o, args);
                        if (AurieSuccess(st)) { res = Describe(r); ok = true; }
                        else inner = "st=" + std::to_string((int)st);
                    }
                }
                catch (const std::exception& e) { inner = std::string("threw ") + e.what(); }
                catch (...) { inner = "access violation inside the with-context"; av = true; }
            });
            if (ok) { resultOut = res; detail = std::string(label) + " (with st=" + std::to_string((int)ws) + ")"; return CiPathResult::Invoked; }
            if (!matched && inner.empty() && !av) {
                detail = std::string(label) + " - InvokeWithObject st=" + std::to_string((int)ws) + " (enumerated object type " + Describe(objectType) + " but no live instance matched id " + std::to_string(targetId) + ")";
                return CiPathResult::Failed;
            }
            detail = std::string(label) + " - " + (inner.empty() ? ("InvokeWithObject st=" + std::to_string((int)ws) + " (callback never ran)") : inner);
            return av ? CiPathResult::AccessViolation : CiPathResult::Failed;
        }
        case CiInvokePath::BuiltinMethod: {
            RValue res = g_Yytk->CallBuiltin("script_execute", withTarget(methodValue));
            resultOut = Describe(res); detail = label; return CiPathResult::Invoked;
        }
        case CiInvokePath::BuiltinExMethod: {
            RValue res;
            AurieStatus st = g_Yytk->CallBuiltinEx(res, "script_execute", selfInst, otherInst, withTarget(methodValue));
            if (!AurieSuccess(st)) { detail = std::string(label) + " st=" + std::to_string((int)st); return CiPathResult::Failed; }
            resultOut = Describe(res); detail = label; return CiPathResult::Invoked;
        }
        case CiInvokePath::BuiltinIndex:
        case CiInvokePath::BuiltinExIndex: {
            RValue idx = g_Yytk->CallBuiltin("method_get_index", { methodValue });
            if (idx.m_Kind == VALUE_UNDEFINED) { detail = std::string(label) + " - method_get_index returned undefined"; return CiPathResult::Failed; }
            const double index = idx.ToDouble();
            if (path == CiInvokePath::BuiltinIndex) {
                RValue res = g_Yytk->CallBuiltin("script_execute", withTarget(RValue(index)));
                resultOut = Describe(res); detail = std::string(label) + " index=" + std::to_string((long)index); return CiPathResult::Invoked;
            }
            RValue res;
            AurieStatus st = g_Yytk->CallBuiltinEx(res, "script_execute", selfInst, otherInst, withTarget(RValue(index)));
            if (!AurieSuccess(st)) { detail = std::string(label) + " st=" + std::to_string((int)st); return CiPathResult::Failed; }
            resultOut = Describe(res); detail = std::string(label) + " index=" + std::to_string((long)index); return CiPathResult::Invoked;
        }
        case CiInvokePath::MethodCall: {
            PVOID p = nullptr;
            if (!AurieSuccess(g_Yytk->GetNamedRoutinePointer("method_call", &p)) || !p) { detail = std::string(label) + " - not in name table"; return CiPathResult::Failed; }
            RValue argArray = g_Yytk->CallBuiltin("array_create", { RValue((double)extraArgs.size()) });
            for (size_t i = 0; i < extraArgs.size(); ++i)
                g_Yytk->CallBuiltin("array_set", { argArray, RValue((double)i), extraArgs[i] });
            RValue res = g_Yytk->CallBuiltin("method_call", { methodValue, argArray });
            resultOut = Describe(res); detail = label; return CiPathResult::Invoked;
        }
        case CiInvokePath::ScriptByName:
        case CiInvokePath::ScriptExByName: {
            const std::string name = CiResolvedScriptName(methodValue);
            if (name.empty()) { detail = std::string(label) + " - no resolvable script name"; return CiPathResult::Failed; }
            const std::string full = "gml_Script_" + name;
            if (path == CiInvokePath::ScriptByName) {
                RValue res = g_Yytk->CallGameScript(full, extraArgs);
                resultOut = Describe(res); detail = std::string(label) + " -> " + full; return CiPathResult::Invoked;
            }
            RValue res;
            AurieStatus st = g_Yytk->CallGameScriptEx(res, full, selfInst, otherInst, extraArgs);
            if (!AurieSuccess(st)) { detail = std::string(label) + " (" + full + ") st=" + std::to_string((int)st); return CiPathResult::Failed; }
            resultOut = Describe(res); detail = std::string(label) + " -> " + full; return CiPathResult::Invoked;
        }
        }
    }
    catch (const std::exception& e) { detail = std::string(label) + " threw std::exception: " + e.what(); return CiPathResult::Failed; }
    catch (...) { detail = std::string(label) + " -> ACCESS VIOLATION (fault inside the game's own code)"; return CiPathResult::AccessViolation; }
    detail = std::string(label) + " - unreachable";
    return CiPathResult::Failed;
}

// B1's idea with B1's blocker removed - except the blocker turned out not to
// be the one B1 named (see the measured note above). Self/other follow the
// original plan's credit decision (pet-quest-collector-plan.md 1): the item is
// self, the player is other.
//
// pathPref selects one path by key, or:
//   "" / "auto"  try each in order, STOPPING at the first access violation.
//                Every shape is measured to fault on a cold call, so a naive
//                run through all nine would trigger nine AVs inside the
//                runtime to learn nothing new.
//   "all"        try every path regardless of faults (the old behaviour),
//                for when a deliberate sweep is actually wanted.
// In every mode the walk stops at the first path that completes - they are
// fallbacks for each other, never a sequence, so plan 4 rule 4 ("one item,
// one call, one observation") still holds.
static void CiInvokeMethodValue(const std::string& source, const std::string& varName, const RValue& methodValue,
                                CInstance* selfInst, CInstance* otherInst, CInstance* itemForFlags,
                                const std::string& pathPref, const std::vector<RValue>& extraArgs,
                                const std::string& argsLabel)
{
    const std::string pref = Lower(pathPref);
    const bool sweepAll = (pref == "all");
    const bool autoPath = pref.empty() || pref == "auto" || sweepAll;
    if (!autoPath) {
        bool known = false;
        for (const CiInvokePathInfo& p : kCiInvokePaths) if (pref == p.key) known = true;
        if (!known) {
            std::string keys;
            for (const CiInvokePathInfo& p : kCiInvokePaths) keys += std::string(keys.empty() ? "" : "|") + p.key;
            Out("citrace invoke: unknown path '" + pathPref + "' - use auto | all | " + keys);
            return;
        }
    }
    if (methodValue.m_Kind != VALUE_OBJECT) {
        Out("citrace invoke: " + source + "." + varName + " is " + Describe(methodValue) + ", not a method/struct value - nothing to invoke");
        return;
    }

    const std::string flagsBefore = CiItemFlags(itemForFlags);
    const int countBefore = CiQuestFamilyCount();
    Out("citrace invoke: " + source + "." + varName + " " + Describe(methodValue) + CiTryResolveMethod(methodValue)
        + " args=[" + argsLabel + "]");
    Out("  before: familyCount=" + std::to_string(countBefore) + " " + flagsBefore + CiQuestProgressReport(itemForFlags));

    bool called = false, hitAv = false;
    std::string result, how;
    for (const CiInvokePathInfo& p : kCiInvokePaths) {
        if (!autoPath && pref != p.key) continue;
        std::string detail;
        const CiPathResult r = CiRunInvokePath(p.path, methodValue, extraArgs, selfInst, otherInst, detail, result);
        how += (how.empty() ? "" : "\n    -> ") + detail;
        if (r == CiPathResult::Invoked) { called = true; break; }
        if (r == CiPathResult::AccessViolation && !sweepAll) { hitAv = true; break; }
    }

    if (!called) {
        Out("  NOT INVOKED: " + how);
        if (hitAv) Out("  stopped at the first access violation (use `all` as the path to sweep every shape anyway, or name one path to retry it alone)");
        Out("  after: (unchanged - nothing ran)");
        return;
    }

    Out("  invoked via " + how + " -> returned " + result);

    // Re-resolve the item rather than reusing the handle: if the call removed
    // it, the old CInstance* is stale and reading through it would report
    // nonsense instead of the disappearance itself.
    std::string afterName; long afterPx = 0;
    CInstance* itemAfter = CiFindNearestQuestItem(&afterName, &afterPx);
    const int countAfter = CiQuestFamilyCount();
    Out("  after:  familyCount=" + std::to_string(countAfter) + " nearest=" + (itemAfter ? afterName : std::string("(none)"))
        + " " + CiItemFlags(itemAfter) + CiQuestProgressReport(itemAfter));
    if (countBefore >= 0 && countAfter >= 0 && countAfter < countBefore)
        Out("  NOTE: family count dropped " + std::to_string(countBefore) + " -> " + std::to_string(countAfter)
            + " - an item left the enumeration. That is NOT by itself a successful collect: confirm the quest objective advanced (plan 4 rule 3) before treating this as a mechanism.");
}

// Arguments for the invoke, parsed from the command line. Numbers pass
// through; the keywords resolve to the ids a quest interaction would plausibly
// be handed. No builtin reports a script's arity, so which of these (if any) a
// given m_Quest* method wants has to be swept by hand.
static bool CiParseInvokeArg(const std::string& tok, CInstance* item, CInstance* player, RValue& out, std::string& label)
{
    const std::string lc = Lower(tok);
    auto idOf = [](CInstance* inst) -> RValue {
        if (!inst) return RValue(-4.0);
        try { return g_Yytk->CallBuiltin("variable_instance_get", { inst->ToRValue(), RValue("id") }); }
        catch (...) { return RValue(-4.0); }
    };
    if (lc == "player") { out = idOf(player); label = "player(" + Describe(out) + ")"; return true; }
    if (lc == "item" || lc == "self") { out = idOf(item); label = "item(" + Describe(out) + ")"; return true; }
    if (lc == "noone") { out = RValue(-4.0); label = "noone(-4)"; return true; }
    if (lc == "true") { out = RValue(1.0); label = "true"; return true; }
    if (lc == "false") { out = RValue(0.0); label = "false"; return true; }
    try { out = RValue(std::stod(tok)); label = tok; return true; }
    catch (...) { return false; }
}

static void CiInvokeItemMethod(const std::string& varName, const std::string& pathPref, const std::string& argTokens)
{
    std::string name; long distPx = 0;
    CInstance* item = CiFindNearestQuestItem(&name, &distPx);
    if (!item) { Out("citrace invoke item: no quest item instance found nearby"); return; }
    RValue player;
    CInstance* p = HhResolveLocalPlayer(player) ? HhResolveInstance(player) : nullptr;

    std::vector<RValue> args; std::string argsLabel;
    std::string remaining = argTokens;
    for (;;) {
        std::string next, tok = FirstToken(remaining, next);
        if (tok.empty()) break;
        RValue v; std::string lbl;
        if (!CiParseInvokeArg(tok, item, p, v, lbl)) { Out("citrace invoke item: bad argument '" + tok + "' - use a number, or player|item|noone|true|false"); return; }
        args.push_back(v); argsLabel += (argsLabel.empty() ? "" : ", ") + lbl;
        remaining = next;
    }

    try {
        RValue id = item->ToRValue();
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue(varName) });
        if (!ex.ToBoolean()) { Out("citrace invoke item: '" + varName + "' does not exist on " + name + " - run `citrace item` to list what does"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(varName) });
        Out("citrace invoke item: target = " + name + " (" + std::to_string(distPx) + " px away), self=item other=player");
        CiInvokeMethodValue("item", varName, v, item, p, item, pathPref, args, argsLabel);
    } catch (...) { Out("citrace invoke item: EXCEPTION"); }
}

// ---- citrace collect: the measured call, wired up exactly as measured -----
// Phase C2's gate. Everything about this call shape came off one live,
// read-only round (citrace nativetrace) on a real Quest_Act_01_Brick_obj
// collect, not off a reading:
//
//   self  = the quest item
//   other = Loot_Manager_obj          (NOT the player - that was the wrong guess)
//   method= the item's own m_Questpickup
//   args  = one real, observed as 1   (the game's own call site passes its
//                                      caller's argument0, or undefined)
//
// How that call is *reached* is no longer measured off an address: the
// default path is `scriptref`, the same InvokeMethodValue the mod ships.
// `native` reproduces the original fixed-address shape for comparison.
//
// m_Questpickup then calls update_quest(questIndex, questObjectiveNumber,
// questValue) and QuestSaveUpdate, so the objective credit is inside the
// call - which is what makes this the right target rather than anything that
// merely makes the item disappear (original plan §11).
//
// Still a mutating command: `confirm` gate, safety banner, before/after flags
// and progress on both sides, one item, one call, no loop.
static CInstance* CiFirstInstanceOfObject(const char* objName)
{
    try {
        int idx = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) }).ToDouble();
        if (idx < 0) return nullptr;
        if ((int)g_Yytk->CallBuiltin("instance_number", { RValue((double)idx) }).ToDouble() <= 0) return nullptr;
        RValue inst = g_Yytk->CallBuiltin("instance_find", { RValue((double)idx), RValue(0.0) });
        if (inst.m_Kind == VALUE_UNDEFINED) return nullptr;
        return HhResolveInstance(inst);
    } catch (...) { return nullptr; }
}

static void CiCollectNearestQuestItem(const std::string& pathPref, const std::string& argTokens)
{
    std::string name; long distPx = 0;
    CInstance* item = CiFindNearestQuestItem(&name, &distPx);
    if (!item) { Out("citrace collect: no quest item instance found nearby"); return; }

    CInstance* lootMgr = CiFirstInstanceOfObject("Loot_Manager_obj");
    if (!lootMgr) {
        Out("citrace collect: no live Loot_Manager_obj instance - that is the `other` the real collect uses, so this would not reproduce the measured call. Aborting.");
        return;
    }

    // Default argument: the 1 observed live. A tester can override it (the
    // game's own call site also admits `undefined` when its caller passed
    // nothing), but the default is what was actually measured.
    std::vector<RValue> args; std::string argsLabel;
    RValue player;
    CInstance* p = HhResolveLocalPlayer(player) ? HhResolveInstance(player) : nullptr;
    std::string remaining = argTokens;
    for (;;) {
        std::string next, tok = FirstToken(remaining, next);
        if (tok.empty()) break;
        RValue v; std::string lbl;
        if (!CiParseInvokeArg(tok, item, p, v, lbl)) { Out("citrace collect: bad argument '" + tok + "' - use a number, or player|item|noone|true|false"); return; }
        args.push_back(v); argsLabel += (argsLabel.empty() ? "" : ", ") + lbl;
        remaining = next;
    }
    if (args.empty()) { args.push_back(RValue(1.0)); argsLabel = "1 (the value measured on a real collect)"; }

    try {
        RValue id = item->ToRValue();
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue("m_Questpickup") });
        if (!ex.ToBoolean()) { Out("citrace collect: '" + name + "' has no m_Questpickup - is it a Quest_Object_Parent_obj descendant?"); return; }
        RValue canPick = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("canPickup") });
        RValue lootType = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("lootType") });
        // Both gates sit in the game's own call site (read at RVA 0x86E0AC0).
        // Reproducing them here is not belt-and-braces: calling m_Questpickup
        // on an item the game would have skipped is exactly the way to credit
        // an objective for something that should not have been collectable.
        if (!canPick.ToBoolean()) { Out("citrace collect: refused - " + name + " has canPickup=false, and the game's own call site gates on it."); return; }
        const double lt = lootType.ToDouble();
        if (lt != 0.0) {
            Out("citrace collect: refused - " + name + " has lootType=" + std::to_string(lt)
                + ", and m_Questpickup is the lootType==0 branch. A different lootType wants a different m_Quest* method (see the research notes).");
            return;
        }
        RValue m = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("m_Questpickup") });
        Out("citrace collect: target = " + name + " (" + std::to_string(distPx) + " px away), self=item other=Loot_Manager_obj, canPickup=true lootType=0");
        CiInvokeMethodValue("item", "m_Questpickup", m, item, lootMgr, item, pathPref, args, argsLabel);
        Out("  plan §4 rule 3: an item vanishing is NOT the pass condition. Check the quest counter in the UI advanced before calling this a success.");
    } catch (...) { Out("citrace collect: EXCEPTION"); }
}

static void CiInvokeGlobalMethod(const std::string& globalName, const std::string& pathPref, const std::string& argTokens)
{
    try {
        RValue ex = g_Yytk->CallBuiltin("variable_global_exists", { RValue(globalName) });
        if (!ex.ToBoolean()) { Out("citrace invoke global: global." + globalName + " does not exist"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_global_get", { RValue(globalName) });
        // The hover/target globals are the reason this path exists (C0.5), so
        // the quest item is still the interesting self-context and still the
        // thing whose flags are worth watching.
        std::string name; long distPx = 0;
        CInstance* item = CiFindNearestQuestItem(&name, &distPx);
        RValue player;
        CInstance* p = HhResolveLocalPlayer(player) ? HhResolveInstance(player) : nullptr;

        std::vector<RValue> args; std::string argsLabel;
        std::string remaining = argTokens;
        for (;;) {
            std::string next, tok = FirstToken(remaining, next);
            if (tok.empty()) break;
            RValue av; std::string lbl;
            if (!CiParseInvokeArg(tok, item, p, av, lbl)) { Out("citrace invoke global: bad argument '" + tok + "' - use a number, or player|item|noone|true|false"); return; }
            args.push_back(av); argsLabel += (argsLabel.empty() ? "" : ", ") + lbl;
            remaining = next;
        }

        Out("citrace invoke global: global." + globalName + (item ? " (self=" + name + ", other=player)" : " (no quest item nearby; self=player)"));
        CiInvokeMethodValue("global", globalName, v, item ? item : p, p, item, pathPref, args, argsLabel);
    } catch (...) { Out("citrace invoke global: EXCEPTION"); }
}

// ---- C0.3: event_perform on the item's own events -------------------------
// Name-free by construction, and it reaches exactly the object-event code
// session 7 proved unreachable by name (22 raw names, all status 14 at install
// time).
//
// The mechanism itself is already proven in this plugin, which the plan did
// not know: `spawnforce` (the Density research command, further down this
// file) has been calling CallBuiltinEx(res, "event_perform", spawnerInstance,
// ..., { ev_alarm, n }) against live Enemy_Creator_obj instances for several
// sessions, and it demonstrably makes those spawners run their own alarm
// event code and spawn enemies. So the *call shape* below needs no
// validation - only the question of whether the quest item's events do
// anything useful is actually open.
//
// Numbers, not names, are what the plan asks for and what the engine
// takes; the label below is only an annotation so the log stays readable and
// the tester can follow the plan's "read-shaped events before press-shaped
// ones" ordering without a lookup table.
static std::string CiEventLabel(int type, int number)
{
    static const char* const kTypes[] = {
        "ev_create", "ev_destroy", "ev_alarm", "ev_step", "ev_collision", "ev_keyboard",
        "ev_mouse", "ev_other", "ev_draw", "ev_keypress", "ev_keyrelease", "ev_trigger",
    };
    static const char* const kMouse[] = {
        "ev_left_button", "ev_right_button", "ev_middle_button", "ev_no_button",
        "ev_left_press", "ev_right_press", "ev_middle_press",
        "ev_left_release", "ev_right_release", "ev_middle_release",
        "ev_mouse_enter", "ev_mouse_leave",
    };
    std::string t = (type >= 0 && type < (int)(sizeof(kTypes) / sizeof(kTypes[0]))) ? kTypes[type] : ("type" + std::to_string(type));
    std::string n = std::to_string(number);
    if (type == 6 && number >= 0 && number < (int)(sizeof(kMouse) / sizeof(kMouse[0]))) n = kMouse[number];
    // Labels come from the standard GameMaker event constants, not from
    // anything measured in this game - treat a surprising label as a wrong
    // label, never as evidence about what the event does.
    return t + "/" + n + " (standard GM naming, unverified for this build)";
}

static void CiEventPerform(int type, int number, const std::string& objName)
{
    std::string name; long distPx = 0;
    CInstance* item = CiFindNearestQuestItem(&name, &distPx);
    if (!item) { Out("citrace event: no quest item instance found nearby - event_perform needs the item as self"); return; }
    RValue player;
    CInstance* p = HhResolveLocalPlayer(player) ? HhResolveInstance(player) : nullptr;

    int objIdx = -1;
    if (!objName.empty()) {
        try { objIdx = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) }).ToDouble(); } catch (...) {}
        if (objIdx < 0) { Out("citrace eventobj: object '" + objName + "' not found"); return; }
    }

    const std::string flagsBefore = CiItemFlags(item);
    const int countBefore = CiQuestFamilyCount();
    const std::string what = objName.empty()
        ? ("event_perform(" + CiEventLabel(type, number) + ")")
        : ("event_perform_object(" + objName + ", " + CiEventLabel(type, number) + ")");
    Out("citrace event: " + what + " with self=" + name + " (" + std::to_string(distPx) + " px away), other=player");
    Out("  before: familyCount=" + std::to_string(countBefore) + " " + flagsBefore + CiQuestProgressReport(item));

    try {
        RValue res;
        AurieStatus st = objName.empty()
            ? g_Yytk->CallBuiltinEx(res, "event_perform", item, p, { RValue((double)type), RValue((double)number) })
            : g_Yytk->CallBuiltinEx(res, "event_perform_object", item, p, { RValue((double)objIdx), RValue((double)type), RValue((double)number) });
        if (!AurieSuccess(st)) { Out("  NOT PERFORMED: call failed st=" + std::to_string((int)st)); return; }
        Out("  performed -> " + Describe(res));
    } catch (...) { Out("  NOT PERFORMED: EXCEPTION"); return; }

    std::string afterName; long afterPx = 0;
    CInstance* itemAfter = CiFindNearestQuestItem(&afterName, &afterPx);
    const int countAfter = CiQuestFamilyCount();
    Out("  after:  familyCount=" + std::to_string(countAfter) + " nearest=" + (itemAfter ? afterName : std::string("(none)"))
        + " " + CiItemFlags(itemAfter) + CiQuestProgressReport(itemAfter));
    if (countBefore >= 0 && countAfter >= 0 && countAfter < countBefore)
        Out("  NOTE: family count dropped " + std::to_string(countBefore) + " -> " + std::to_string(countAfter)
            + " - confirm the quest objective advanced before treating this as a collect (plan 4 rule 3).");
}

// ---- C0.6: confirm the deactivation model ---------------------------------
// Session 6 inferred instance_deactivate_* from "the item left instance_number
// / instance_find while instance_destroy stayed at 0", but never confirmed it
// directly. instance_activate_object is the inverse: if a collected item
// reappears in the family count, the model is confirmed. Diagnostic only -
// reactivating an instance changes what the room enumerates, it does NOT
// credit or un-credit a quest objective, and this must not be mistaken for (or
// built into) a collect.
static void CiActivateObject(const std::string& objName)
{
    ResolvePetQuestAssets();
    int idx = g_QuestObjParentIdx;
    std::string label = "Quest_Object_Parent_obj";
    if (!objName.empty()) {
        label = objName;
        try { idx = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) }).ToDouble(); } catch (...) { idx = -1; }
    }
    if (idx < 0) { Out("citrace activate: object '" + label + "' not resolvable"); return; }
    PVOID probe = nullptr;
    if (!AurieSuccess(g_Yytk->GetNamedRoutinePointer("instance_activate_object", &probe)) || !probe) {
        Out("citrace activate: instance_activate_object is not in this runtime's name table - C0.6 cannot be tested this way");
        return;
    }

    const int before = CiQuestFamilyCount();
    try {
        g_Yytk->CallBuiltin("instance_activate_object", { RValue((double)idx) });
    } catch (...) { Out("citrace activate: EXCEPTION calling instance_activate_object"); return; }
    const int after = CiQuestFamilyCount();
    Out("citrace activate: instance_activate_object(" + label + " #" + std::to_string(idx) + ") - Quest_Object_Parent_obj family count "
        + std::to_string(before) + " -> " + std::to_string(after));
    if (after > before)
        Out("  CONFIRMED: instances reappeared, so a collected item is deactivated (not destroyed) - session 6's inference holds. Deactivating an item ourselves is NOT a collect: no progress is credited.");
    else
        Out("  No change. Either nothing was deactivated, or deactivation is not the mechanism - re-run immediately after a real collect to make this decisive.");
}

// ---- C0.5: the hover-target globals, read as values ------------------------
// Session 4's `dump` sweep found these names and they were hooked (0 calls),
// but never read as values or called. A global holding a method value is a
// direct target for C0.2's invoke path; global.hoverTooltip in particular is a
// live, settable flag that is already 1.
static const char* const kCiHoverGlobals[] = {
    "GetMouseTarget", "PlayerGetMouseTarget", "GetQuestHoverDescription",
    "GetMouseDisabledTarget", "CanISeeTarget", "hoverTooltip",
    "PlayerMouseAction", "GetPlayerMouseDisabled", "KeyboardMouseInput",
    "RefreshMouseMove",
};

static void CiReportHoverGlobals()
{
    Out("citrace globals: the hover/target globals, read as values (session 4 hooked these and measured 0 calls, but never read them):");
    for (const char* g : kCiHoverGlobals) {
        try {
            RValue ex = g_Yytk->CallBuiltin("variable_global_exists", { RValue(std::string(g)) });
            if (!ex.ToBoolean()) { Out(std::string("  global.") + g + " = (does not exist)"); continue; }
            RValue v = g_Yytk->CallBuiltin("variable_global_get", { RValue(std::string(g)) });
            std::string d = CiExpandContainer(v);
            if (d.size() > 300) d = d.substr(0, 300) + "...(truncated)";
            Out(std::string("  global.") + g + " = " + d);
        } catch (...) { Out(std::string("  global.") + g + " = EXCEPTION"); }
    }
    Out("citrace globals: read-only. Anything that resolves to a method is a target for `citrace invoke global <name> confirm`.");
}

// Broadened sweep, per C0.5's "broaden the dump sweep in the same round".
// Differs from the pre-existing `dump <substr>` in two ways that matter here:
// it covers the plan's whole keyword list in one pass instead of one word per
// command, and it renders values through CiExpandContainer, so an array,
// ds_map or bound method shows its contents/target instead of an opaque
// handle - which is exactly how the m_Quest* methods stayed invisible for six
// sessions.
static void CiSweepGlobals(const std::string& extra)
{
    static const char* const kSweepKeywords[] = {
        "use", "key", "press", "activate", "collect", "pickup",
        "interactable", "focus", "selected", "target", "hover", "quest", "interact",
    };
    std::vector<std::string> needles;
    for (const char* k : kSweepKeywords) needles.push_back(k);
    std::string remaining = extra;
    for (;;) {
        std::string next;
        std::string tok = FirstToken(remaining, next);
        if (tok.empty()) break;
        needles.push_back(Lower(tok));
        remaining = next;
    }

    CInstance* global = nullptr;
    AurieStatus st = g_Yytk->GetGlobalInstance(&global);
    if (!AurieSuccess(st) || !global) { Out("citrace sweep: GetGlobalInstance failed st=" + std::to_string((int)st)); return; }

    constexpr int kMaxSweepLines = 250;
    int matched = 0, printed = 0;
    RValue globalrv = RValue(global);
    g_Yytk->EnumInstanceMembers(globalrv, [&](const char* name, RValue* val) -> bool {
        if (!name) return false;
        const std::string lower = Lower(name);
        bool hit = false;
        for (const std::string& n : needles) if (!n.empty() && lower.find(n) != std::string::npos) { hit = true; break; }
        if (!hit) return false;
        ++matched;
        if (printed < kMaxSweepLines) {
            std::string d = val ? CiExpandContainer(*val) : "<null>";
            if (d.size() > 300) d = d.substr(0, 300) + "...(truncated)";
            Out("  global." + std::string(name) + " = " + d);
            ++printed;
        }
        return false;   // keep enumerating everything
    });
    Out("citrace sweep: " + std::to_string(matched) + " matches"
        + (matched > printed ? " (" + std::to_string(printed) + " printed, rest truncated)" : ""));
}


// ---- Phase C1 infrastructure (dev build only) -----------------------------
// docs/pet-quest-collector-plan-c-direct-invocation.md §3. Two tools, and
// neither is specific to this mod - they exist because Phase C1 needs them and
// every future native-analysis job in this toolkit needs them too.
//
//   symdump [start] [end]        - the whole runtime script table as
//                                  index,name,rva - i.e. a symbol file for a
//                                  stripped 280 MB binary.
//   citrace stackwalk [n]        - "who called this builtin", captured from
//                                  inside the already-firing
//                                  keyboard_check_pressed(70) hook.
//
// ---- symdump ---------------------------------------------------------------
// §3.2's anchor problem, solved from a different direction than the plan
// proposed. The plan's step 3 wanted to reverse-engineer the dispatcher's
// ID->name table (`base + id*0x18` at 0x14b488f40) so call-site IDs in
// decompiled output become readable. That is real work with an uncertain
// payoff. But the runtime already exposes the same mapping through a builtin
// nobody thought to iterate: `script_get_name(i)` answers it for any index,
// and `GetNamedRoutinePointer` turns the name into a live address. Walking the
// index space therefore produces the *whole* table directly - no disassembly,
// no guessing at a struct layout.
//
// What comes out is a complete `name -> RVA` symbol file for Hero_Siege.exe.
// Feed it to tools/ghidra/ImportSymbols.java and a stripped binary full of
// FUN_14xxxxxxx becomes a named one, permanently, for every future session and
// every other tool in this toolkit - not just this mod. That is worth more
// than the single answer Phase C1 was opened to get.
//
// It also covers ground hs-game-sdk cannot: the SDK's generated table has 6254
// script names, but it is a static extract, while this reads whatever the
// running build actually has - including the `anon@N@gml_Object_..._Create_0`
// closures that only exist at runtime and that this investigation has spent
// three sessions chasing by hand.
//
// Cost: two interpreter round trips per index. The default 100000-110000 range
// covers the observed script index space (measured 2026-09-11: CanISeeTarget
// #100465 at the low end, the Quest_Object_Parent_obj Create closures at
// #105270-105276) in ~20k calls, which is a fraction of a second - but it does
// land in one frame, so it is a deliberate one-shot command and never a tick.
// The range is a parameter so a wider sweep can be paged rather than taken as
// one long stall.

// ---- citrace dispatchdump: the builtin-function table ----------------------
// MEASURED 2026-09-11. This closes the thread session 7 flagged as "most
// promising next thread" and that Plan C carried forward as §3.2 step 3 -
// and it turned out to need no ID-scheme decoding at all, because the names
// are sitting in the table.
//
// Frame02 of the Phase C1 stack walk decompiled to the generic script/builtin
// dispatcher (0x14b488f40, the same function the 2026-09-10 session found by
// hand). Described rather than quoted, per agents.md: it indexes a table
// whose static base pointer lives at exe+0x1081B410, with a stride of 0x18
// bytes per entry, loads the callable function pointer from offset +8 of the
// entry, and calls it as (result, self, other, argc).
//
// Reading that table live (via the pre-existing `readmem`, no rebuild needed)
// showed each 0x18-byte entry is:
//
//     +0x00  const char*  name          <- a plain C string, e.g. "camera_create"
//     +0x08  void*        function      <- the native implementation
//     +0x10  int32        argument count
//
// Verified against real GameMaker signatures on the first ten entries:
// camera_create/0, camera_create_view/4, camera_destroy/1, camera_apply/1,
// camera_copy_transforms/2, camera_get_active/0 - every argc matches.
//
// So every dispatcher call site in every decompiled function becomes
// readable: read the call's ID operand, index this table, get the name. That
// is what made the prior Ghidra pass's 705 KB of output unreadable, and it is
// now mechanical.
//
// Together with `citrace symdump` (scripts, by name) this covers both halves
// of the runtime's callable surface - scripts AND builtins.
//
// The default table-pointer RVA below is specific to this build, derived from
// the Ghidra analysis above (Ghidra address 0x15081b410, image base
// 0x140000000). It is a parameter so a future build can be pointed at a new
// one without a rebuild, rather than silently reading the wrong address.
static constexpr unsigned long long kCiDispatchTablePtrRvaDefault = 0x1081B410ull;

static void CiDispatchDump(unsigned long long tablePtrRva, int maxEntries)
{
    if (maxEntries <= 0 || maxEntries > 20000) { Out("citrace dispatchdump: maxEntries out of range (1..20000)"); return; }
    HMODULE mod = GetModuleHandleA(nullptr);
    if (!mod) { Out("citrace dispatchdump: no main module"); return; }

    // The RVA names a POINTER to the table, not the table itself (the
    // decompile reads _DAT_15081b410 as a value and adds id*0x18 to it).
    unsigned char* pptr = (unsigned char*)mod + tablePtrRva;
    if (IsBadReadPtr(pptr, sizeof(void*))) { Out("citrace dispatchdump: table pointer address not readable - wrong RVA for this build?"); return; }
    unsigned char* table = *(unsigned char**)pptr;
    if (!table || IsBadReadPtr(table, 0x18)) { Out("citrace dispatchdump: table pointer does not lead anywhere readable"); return; }

    char hdr[220];
    sprintf_s(hdr, "citrace dispatchdump: table ptr at exe+0x%llX -> %p", tablePtrRva, (void*)table);
    Out(hdr);

    const std::string path = IPC_DIR + "\\builtins.csv";
    std::ofstream f(path, std::ios::trunc);
    if (!f) { Out("citrace dispatchdump: cannot open " + path); return; }
    f << "id,name,argc,rva\n";

    int written = 0, stoppedAt = -1;
    for (int i = 0; i < maxEntries; ++i) {
        unsigned char* entry = table + (unsigned long long)i * 0x18ull;
        if (IsBadReadPtr(entry, 0x18)) { stoppedAt = i; break; }
        const char* name = *(const char**)(entry + 0x00);
        void* fn = *(void**)(entry + 0x08);
        const int argc = *(int*)(entry + 0x10);
        // End of table: the first entry whose name pointer is not a readable
        // string. Checked rather than assumed, since the table's length is not
        // recorded anywhere we can see.
        if (!name || IsBadReadPtr((void*)name, 1)) { stoppedAt = i; break; }
        // Bound the name read so a garbage pointer that happens to be readable
        // cannot run away into unmapped memory.
        size_t len = 0;
        while (len < 128 && !IsBadReadPtr((void*)(name + len), 1) && name[len] != '\0') ++len;
        if (len == 0 || len >= 128) { stoppedAt = i; break; }
        std::string nm(name, len);
        // End-of-table detection: a real name is printable, space-free ASCII.
        // MEASURED 2026-09-11: an earlier, stricter version of this check
        // required alnum/underscore only and truncated the walk at id 174 on
        // "@@array_get@@" - GameMaker's internal builtins legitimately use '@'
        // in their names (the same convention as "@@GetInstance@@", which this
        // plugin already calls elsewhere). Checking for printability instead of
        // identifier-shape still catches binary garbage past the end without
        // rejecting valid entries.
        bool plausible = true;
        for (char c : nm) {
            const unsigned char u = (unsigned char)c;
            if (u < 33 || u > 126) { plausible = false; break; }
        }
        if (!plausible) { stoppedAt = i; break; }
        // Quotes would corrupt the CSV; no known builtin contains one, but the
        // walk reads whatever memory holds rather than what it should hold.
        std::string safe;
        for (char c : nm) { if (c == '"') safe += "\"\""; else safe += c; }
        nm = safe;

        unsigned long long rva = 0;
        if (fn) {
            HMODULE fnMod = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)fn, &fnMod);
            if (fnMod == mod) rva = (unsigned long long)((char*)fn - (char*)mod);
        }
        char rvabuf[32];
        if (rva) sprintf_s(rvabuf, "0x%llX", rva); else strcpy_s(rvabuf, "");
        f << i << ",\"" << nm << "\"," << argc << "," << rvabuf << "\n";
        ++written;
    }
    f.close();

    char b[260];
    sprintf_s(b, "citrace dispatchdump: %d builtins -> bp_ipc\\builtins.csv%s",
              written, stoppedAt >= 0 ? (" (table ended at id " + std::to_string(stoppedAt) + ")").c_str() : " (hit maxEntries - re-run with a larger limit)");
    Out(b);
    Out("  every dispatcher call site's ID operand now resolves to a name through this table.");
}


// ---- citrace dispatchtrace: every builtin the game calls, by name ----------
// The payoff of `citrace dispatchdump`, and the answer to a problem that cost
// sessions 4-7 their entire budget.
//
// Those sessions hooked builtins one guessed name at a time - 34 hooked call
// sites across 13 rebuild/relaunch cycles, every one measuring zero calls,
// because the right name was never among the guesses. The quest item's removal
// mechanism in particular has never been explained: instance_destroy,
// instance_deactivate_object, instance_deactivate_all and instance_change all
// measured 0 on confirmed collects, and C0.6 later showed
// instance_activate_object cannot bring a collected item back either.
//
// Guessing is no longer necessary. Frame02 of the Phase C1 stack walk showed
// that *every* builtin call in the game funnels through one dispatcher
// (0x14b488f40), which indexes a table by a numeric ID - and
// `citrace dispatchdump` has now read that table, so every ID resolves to a
// name. Hooking the dispatcher once therefore sees every builtin call the game
// makes, named, with no prior knowledge of which one matters.
//
// Armed explicitly, and by default on the next real F-press edge, so the
// capture covers exactly the collect window rather than the whole session.
//
// Cost control matters here: this is the single hottest function in the
// process (every builtin call in every object's every frame). So:
//   - the hook is installed lazily, only when dispatchtrace is first armed -
//     never as part of `citrace 1`, and never in a player build;
//   - while idle it does one relaxed atomic load and returns;
//   - while armed it writes one int to a preallocated array - no allocation,
//     no formatting, no I/O on the hot path. Names are resolved later, off
//     the hot path, when the capture is written out.
static constexpr unsigned long long kCiDispatchFnRvaDefault = 0xB488F40ull;
static constexpr int kCiDispCap = 60000;   // ~a few frames of a busy scene

typedef void* (*CiDispatchFn)(void*, void*, void*, int, long long, void*);
static CiDispatchFn g_CiOrigDispatch = nullptr;
static std::atomic<bool> g_CiDispArmed{ false };
static std::atomic<bool> g_CiDispArmOnFPress{ false };
static std::atomic<int>  g_CiDispN{ 0 };
static int g_CiDispSeq[kCiDispCap];

// Deliberately records only the builtin ID. Capturing `self`'s object_index
// too was tried and dropped: YYTK exposes CInstance's members differently
// across the versioned struct definitions in YYTK_Shared_Types.hpp, and the
// value is a nice-to-have rather than the payload - what matters is *which
// builtins the game calls*, which the ID alone answers. Leaving it out also
// keeps this function, the hottest in the process, down to one relaxed load
// and one store while armed.
static void* CiHookDispatch(void* self, void* other, void* result, int argc, long long id, void* args)
{
    if (g_CiDispArmed.load(std::memory_order_relaxed)) {
        const int n = g_CiDispN.fetch_add(1, std::memory_order_relaxed);
        if (n < kCiDispCap) g_CiDispSeq[n] = (int)id;
        else g_CiDispArmed.store(false, std::memory_order_relaxed);
    }
    return g_CiOrigDispatch ? g_CiOrigDispatch(self, other, result, argc, id, args) : nullptr;
}

static bool CiInstallDispatchHook(unsigned long long fnRva)
{
    if (g_CiOrigDispatch) return true;
    HMODULE mod = GetModuleHandleA(nullptr);
    if (!mod) { Out("citrace dispatchtrace: no main module"); return false; }
    PVOID src = (PVOID)((char*)mod + fnRva);
    PVOID tramp = nullptr;
    AurieStatus hs = MmCreateHook(g_ArSelfModule, "bp_dispatch", src, (PVOID)CiHookDispatch, &tramp);
    if (!AurieSuccess(hs) || !tramp) {
        Out("citrace dispatchtrace: MmCreateHook failed st=" + std::to_string((int)hs));
        return false;
    }
    g_CiOrigDispatch = (CiDispatchFn)tramp;
    char b[160];
    sprintf_s(b, "citrace dispatchtrace: hooked the builtin dispatcher at exe+0x%llX", fnRva);
    Out(b);
    return true;
}

// Called from the keyboard_check_pressed F-press edge, next to the stack walk.
static void CiDispatchArmOnPress()
{
    if (!g_CiDispArmOnFPress.exchange(false)) return;
    g_CiDispN.store(0, std::memory_order_relaxed);
    g_CiDispArmed.store(true, std::memory_order_relaxed);
}

// Off the hot path: resolve the captured IDs against the live builtin table
// (the same one `citrace dispatchdump` reads) and write the sequence out.
static void CiDispatchWrite(unsigned long long tablePtrRva)
{
    g_CiDispArmed.store(false, std::memory_order_relaxed);
    const int n = (std::min)(g_CiDispN.load(), kCiDispCap);
    if (n <= 0) { Out("citrace dispatchtrace: nothing captured (was it armed, and did an F press happen?)"); return; }

    unsigned char* table = nullptr;
    HMODULE mod = GetModuleHandleA(nullptr);
    if (mod) {
        unsigned char* pptr = (unsigned char*)mod + tablePtrRva;
        if (!IsBadReadPtr(pptr, sizeof(void*))) table = *(unsigned char**)pptr;
        if (table && IsBadReadPtr(table, 0x18)) table = nullptr;
    }
    auto nameOf = [&](int id) -> std::string {
        if (!table || id < 0) return "";
        unsigned char* e = table + (unsigned long long)id * 0x18ull;
        if (IsBadReadPtr(e, 0x18)) return "";
        const char* nm = *(const char**)e;
        if (!nm || IsBadReadPtr((void*)nm, 1)) return "";
        size_t len = 0;
        while (len < 128 && !IsBadReadPtr((void*)(nm + len), 1) && nm[len] != '\0') ++len;
        return (len && len < 128) ? std::string(nm, len) : std::string();
    };
    // Ordered sequence - what the game did, in the order it did it.
    const std::string seqPath = IPC_DIR + "\\dispatchseq.csv";
    std::ofstream f(seqPath, std::ios::trunc);
    f << "n,id,builtin\n";
    std::unordered_map<int, long> counts;
    for (int i = 0; i < n; ++i) {
        const int id = g_CiDispSeq[i];
        counts[id]++;
        f << i << "," << id << ",\"" << nameOf(id) << "\"\n";
    }
    f.close();

    // Summary - which builtins, how often. Far easier to eyeball than 60k rows.
    const std::string sumPath = IPC_DIR + "\\dispatchsummary.csv";
    std::ofstream g(sumPath, std::ios::trunc);
    g << "id,builtin,calls\n";
    std::vector<std::pair<long, int>> byCount;
    for (const auto& kv : counts) byCount.push_back({ kv.second, kv.first });
    std::sort(byCount.begin(), byCount.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    for (const auto& c : byCount) g << c.second << ",\"" << nameOf(c.second) << "\"," << c.first << "\n";
    g.close();

    Out("citrace dispatchtrace: " + std::to_string(n) + " builtin calls, " + std::to_string((int)counts.size())
        + " distinct -> bp_ipc\\dispatchseq.csv (ordered) + dispatchsummary.csv (counts)");
}

static void SymDump(int startIdx, int endIdx)
{
    if (endIdx <= startIdx) { Out("symdump: usage -> symdump [startIndex=100000] [endIndex=110000]"); return; }
    constexpr int kMaxSpan = 40000;   // keeps a single command's frame cost bounded
    if (endIdx - startIdx > kMaxSpan) {
        Out("symdump: range too wide (" + std::to_string(endIdx - startIdx) + " > " + std::to_string(kMaxSpan)
            + ") - page it, e.g. `symdump 100000 120000` then `symdump 120000 140000`");
        return;
    }

    const std::string path = IPC_DIR + "\\symbols.csv";
    std::ofstream f(path, std::ios::trunc);
    if (!f) { Out("symdump: cannot open " + path); return; }
    f << "index,name,rva,module\n";

    int named = 0, resolved = 0;
    HMODULE mainMod = GetModuleHandleA(nullptr);

    for (int i = startIdx; i < endIdx; ++i) {
        std::string name;
        try {
            RValue r = g_Yytk->CallBuiltin("script_get_name", { RValue((double)i) });
            if (r.m_Kind != VALUE_STRING) continue;
            name = r.ToString();
        } catch (...) { continue; }
        // GameMaker answers an unused index with a placeholder rather than an
        // error; both shapes have been seen, so filter on both.
        if (name.empty() || name == "<undefined>" || name[0] == '<') continue;
        ++named;

        PVOID p = nullptr;
        // Script assets are registered under the gml_Script_ prefix; a few
        // entries (builtin-backed functions) answer to the bare name instead,
        // so try both rather than silently dropping them.
        const std::string full = "gml_Script_" + name;
        if (!AurieSuccess(g_Yytk->GetNamedRoutinePointer(full.c_str(), &p)) || !p) {
            p = nullptr;
            if (!AurieSuccess(g_Yytk->GetNamedRoutinePointer(name.c_str(), &p))) p = nullptr;
        }
        if (!p) { f << i << ",\"" << name << "\",,\n"; continue; }

        PVOID src = nullptr;
        try { src = (PVOID)reinterpret_cast<CScript*>(p)->m_Functions->m_ScriptFunction; } catch (...) {}
        if (!src) { f << i << ",\"" << name << "\",,\n"; continue; }

        HMODULE mod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)src, &mod);
        const unsigned long long rva = mod ? (unsigned long long)((char*)src - (char*)mod) : 0ull;
        char rvabuf[32];
        sprintf_s(rvabuf, "0x%llX", rva);
        f << i << ",\"" << name << "\"," << rvabuf << "," << (mod == mainMod ? "exe" : "other") << "\n";
        ++resolved;
    }
    f.close();
    Out("symdump: indices " + std::to_string(startIdx) + ".." + std::to_string(endIdx)
        + " - " + std::to_string(named) + " named, " + std::to_string(resolved) + " with addresses -> bp_ipc\\symbols.csv");
    Out("  next: run tools\\ghidra\\ImportSymbols.java against that CSV to name the binary in Ghidra.");
}

// ---- citrace stackwalk -----------------------------------------------------
// §3.2 step 1, and the plan is right that it is the highest-value item in the
// phase: it converts this investigation's one confirmed live signal - F is
// read by keyboard_check_pressed with Self=Profile_Manager_obj - into the
// actual chain of native functions that read the interact key, which is
// exactly the set DecompileTargets.java should be pointed at. Every prior
// Ghidra pass picked its targets for being *hookable* rather than *involved*,
// which is why it stalled.
//
// Armed explicitly and counted down, because it must fire on the real
// keypress edge and nothing else. RtlCaptureStackBackTrace on a once-per-press
// edge costs nothing worth measuring. Return addresses are reported as
// module+RVA, which is directly what Ghidra wants - and with symbols.csv
// loaded, most frames come back named instead of numeric.
static std::atomic<int> g_CiStackWalkLeft{ 0 };

static void CiCaptureStackWalk(const char* whatFired)
{
    if (g_CiStackWalkLeft.load() <= 0) return;
    if (g_CiStackWalkLeft.fetch_sub(1) <= 0) { g_CiStackWalkLeft.store(0); return; }
    try {
        constexpr int kMaxFrames = 40;
        PVOID frames[kMaxFrames] = { nullptr };
        const USHORT got = RtlCaptureStackBackTrace(0, kMaxFrames, frames, nullptr);
        Out(std::string("citrace stackwalk: ") + whatFired + " - " + std::to_string((int)got) + " frames (nearest caller first)");
        HMODULE mainMod = GetModuleHandleA(nullptr);
        for (USHORT i = 0; i < got; ++i) {
            if (!frames[i]) continue;
            HMODULE mod = nullptr;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)frames[i], &mod);
            char modname[MAX_PATH] = { 0 };
            if (mod) GetModuleFileNameA(mod, modname, MAX_PATH);
            const char* shortName = modname;
            for (const char* c = modname; *c; ++c) if (*c == '\\' || *c == '/') shortName = c + 1;
            char line[400];
            sprintf_s(line, "  [%02d] %s+0x%llX%s", (int)i,
                      (mod ? shortName : "?"),
                      (unsigned long long)(mod ? (char*)frames[i] - (char*)mod : 0),
                      (mod == mainMod ? "   <-- Hero_Siege.exe, decompile this RVA" : ""));
            Out(line);
        }
        // Frames inside our own plugin and inside YYToolkit are the hook
        // trampoline, not the game - the interesting ones are the
        // Hero_Siege.exe rows, and they are marked above so the reader does
        // not have to work that out per frame.
    } catch (...) { Out("citrace stackwalk: EXCEPTION capturing backtrace"); }
}

// ---- citrace nativetrace: detour the collect chain at its real address -----
// The static read of the compiled bodies (docs/pet-quest-collector-c-research.md,
// "Reading the compiled code") says a quest-item collect is:
//
//     PlayerMovement -> PlayerMouseAction -> with(item) item.m_Questpickup(player.id)
//                    -> update_quest -> QuestSaveUpdate
//
// Every one of those is already hooked above and every one has measured zero
// calls. That looks like the reading is wrong. It is not - it is how the
// existing hooks work:
//
//   HookOneScript / HookRawNamedRoutine install by writing
//   `sc->m_Functions->m_ScriptFunction = detour` - they swap a function
//   pointer *inside the script-table entry*. That intercepts a call only if
//   the caller reads that table entry at call time.
//
// This build does not. Compiled GML calls another compiled script with a
// direct `call rel32` bound at compile time (read straight off the
// disassembly: PlayerMovement reaches PlayerMouseAction that way), and it
// calls a bound method through the function pointer captured in the method
// value when the closure was created. Neither ever looks at the table again,
// so a table-pointer swap is invisible to both.
//
// So those zeroes are a property of the instrument, not of the game. The fix
// is the same primitive `citrace dispatchtrace` already uses for the builtin
// dispatcher: MmCreateHook, which patches the bytes at the function's own
// address and therefore catches every caller, direct calls included.
//
// The two hooks are deliberately left installed side by side on the same
// functions, and `citrace nativetrace show` prints both counters, because the
// comparison is itself the measurement. CheckPlayerInteraction is the control:
// the static read shows it running from the Step event of every shrine, NPC,
// pile and stone in the room, so it must fire constantly. If its native
// counter climbs into the thousands while its table counter stays at 0, the
// blindness is demonstrated outright rather than argued.
//
// Read-only: every detour counts, optionally logs, and tail-calls the
// trampoline. Nothing here writes game state, so none of it sits behind the
// `confirm` gate (plan C §4 applies to mutation, and there is none).
static constexpr long kCiNatLogBudget = 6;   // per target, so one hot hook cannot drown out.txt

#define CINATIVE_HOOK(SAFE, LABEL) \
    static PFUNC_YYGMLScript g_CiNatOrig_##SAFE = nullptr; \
    static volatile long g_CiNatCalls_##SAFE = 0; \
    static volatile long g_CiNatLogged_##SAFE = 0; \
    static RValue& CiNat_##SAFE(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        const long n = InterlockedIncrement(&g_CiNatCalls_##SAFE); \
        if (g_CiNatLogged_##SAFE < kCiNatLogBudget \
            && InterlockedIncrement(&g_CiNatLogged_##SAFE) <= kCiNatLogBudget) { \
            try { \
                Out(std::string("citracenat " LABEL " #") + std::to_string(n) \
                    + " self=" + CiDescribeInstance(S) + " other=" + CiDescribeInstance(O) \
                    + " argc=" + std::to_string(argc) + AggroArgs(argc, A) \
                    + " selfvars=[" + CiQuestVars(S) + " ]"); \
            } catch (...) {} \
        } \
        return g_CiNatOrig_##SAFE ? g_CiNatOrig_##SAFE(S, O, R, argc, A) : R; \
    }

CINATIVE_HOOK(PlayerMouseAction, "PlayerMouseAction")
CINATIVE_HOOK(CheckPlayerInteraction, "CheckPlayerInteraction")
CINATIVE_HOOK(UpdateQuest, "update_quest")
CINATIVE_HOOK(Questpickup, "m_Questpickup(anon@2786)")
CINATIVE_HOOK(QuestInteract, "m_QuestInteract(anon@3858)")
CINATIVE_HOOK(QuestActivate, "m_QuestActivate(anon@1584)")
CINATIVE_HOOK(QuestDestructible, "m_QuestDestructible(anon@2113)")
CINATIVE_HOOK(QuestActive, "m_QuestActive(anon@4737)")
CINATIVE_HOOK(QuestUseKey, "m_QuestUseKey(anon@1400)")
CINATIVE_HOOK(LootGroundDeActiveStep, "m_LootGroundDeActiveStep(anon@5164)")
#undef CINATIVE_HOOK

struct CiNatTarget {
    const char*        label;
    const char*        runtimeName;   // exact runtime name, no prefix added here
    const char*        hookId;
    PVOID              detour;
    PFUNC_YYGMLScript* origSlot;
    PFUNC_YYGMLScript* tableOrig;     // the matching HookOneScript trampoline, if one took the entry
    volatile long*     nativeCalls;
    volatile long*     tableCalls;    // the matching HookOneScript counter, or nullptr
    volatile long*     logged;
};

// The seven m_Quest* closures and the two scripts either side of them. All
// batched into one build per agents.md - one relaunch, not ten.
#define CINAT_ENTRY(SAFE, LABEL, NAME, TABLEORIG, TABLECALLS) \
    { LABEL, NAME, "fp_cinat_" #SAFE, (PVOID)CiNat_##SAFE, &g_CiNatOrig_##SAFE, \
      TABLEORIG, &g_CiNatCalls_##SAFE, TABLECALLS, &g_CiNatLogged_##SAFE }

static CiNatTarget g_CiNatTargets[] = {
    CINAT_ENTRY(PlayerMouseAction, "PlayerMouseAction", "gml_Script_PlayerMouseAction",
                &g_OrigCi_PlayerMouseAction, &g_CiCalls_PlayerMouseAction),
    CINAT_ENTRY(CheckPlayerInteraction, "CheckPlayerInteraction", "gml_Script_CheckPlayerInteraction",
                &g_OrigCi_CheckPlayerInteraction, &g_CiCalls_CheckPlayerInteraction),
    CINAT_ENTRY(UpdateQuest, "update_quest", "gml_Script_update_quest",
                nullptr, nullptr),
    CINAT_ENTRY(Questpickup, "m_Questpickup", "gml_Script_anon@2786@gml_Object_Quest_Object_Parent_obj_Create_0",
                &g_OrigCi_Anon2786, &g_CiCalls_Anon2786),
    CINAT_ENTRY(QuestInteract, "m_QuestInteract", "gml_Script_anon@3858@gml_Object_Quest_Object_Parent_obj_Create_0",
                &g_OrigCi_Anon3858, &g_CiCalls_Anon3858),
    CINAT_ENTRY(QuestActivate, "m_QuestActivate", "gml_Script_anon@1584@gml_Object_Quest_Object_Parent_obj_Create_0",
                &g_OrigCi_Anon1584, &g_CiCalls_Anon1584),
    CINAT_ENTRY(QuestDestructible, "m_QuestDestructible", "gml_Script_anon@2113@gml_Object_Quest_Object_Parent_obj_Create_0",
                &g_OrigCi_Anon2113, &g_CiCalls_Anon2113),
    CINAT_ENTRY(QuestActive, "m_QuestActive", "gml_Script_anon@4737@gml_Object_Quest_Object_Parent_obj_Create_0",
                &g_OrigCi_Anon4737, &g_CiCalls_Anon4737),
    CINAT_ENTRY(QuestUseKey, "m_QuestUseKey", "gml_Script_anon@1400@gml_Object_Quest_Object_Parent_obj_Create_0",
                &g_OrigCi_Anon1400, &g_CiCalls_Anon1400),
    CINAT_ENTRY(LootGroundDeActiveStep, "m_LootGroundDeActiveStep", "gml_Script_anon@5164@gml_Object_Quest_Object_Parent_obj_Create_0",
                &g_OrigCi_Anon5164, &g_CiCalls_Anon5164),
};
#undef CINAT_ENTRY

static std::atomic<bool> g_CiNatInstalled{ false };

// Resolving the address is the one subtle part. If `citrace 1` already ran,
// the script-table entry no longer holds the game's function - it holds this
// plugin's table-swap detour, and the real body is in that hook's saved
// original. Patching the detour instead of the game would produce a hook that
// fires only when the (blind) table hook fires, i.e. never, which is exactly
// the failure this command exists to rule out.
static PVOID CiNativeAddressOf(const CiNatTarget& t)
{
    if (t.tableOrig && *t.tableOrig) return (PVOID)*t.tableOrig;
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(t.runtimeName, &p);
    if (!AurieSuccess(st) || !p) {
        Out(std::string("citrace nativetrace: ") + t.label + " - name not found st=" + std::to_string((int)st));
        return nullptr;
    }
    CScript* sc = reinterpret_cast<CScript*>(p);
    if (!sc || !sc->m_Functions) {
        Out(std::string("citrace nativetrace: ") + t.label + " - null functions");
        return nullptr;
    }
    return (PVOID)sc->m_Functions->m_ScriptFunction;
}

static void CiNativeTraceInstall()
{
    if (g_CiNatInstalled.exchange(true)) { Out("citrace nativetrace: already installed"); return; }
    HMODULE mainMod = GetModuleHandleA(nullptr);
    int ok = 0, failed = 0;
    for (CiNatTarget& t : g_CiNatTargets) {
        PVOID src = CiNativeAddressOf(t);
        if (!src) { ++failed; continue; }
        PVOID tramp = nullptr;
        AurieStatus hs = MmCreateHook(g_ArSelfModule, t.hookId, src, t.detour, &tramp);
        if (!AurieSuccess(hs) || !tramp) {
            Out(std::string("citrace nativetrace: ") + t.label + " - MmCreateHook failed st=" + std::to_string((int)hs));
            ++failed;
            continue;
        }
        *t.origSlot = reinterpret_cast<PFUNC_YYGMLScript>(tramp);
        char b[240];
        sprintf_s(b, "citrace nativetrace: detoured %s at exe+0x%llX%s", t.label,
                  (unsigned long long)((char*)src - (char*)mainMod),
                  (t.tableOrig && *t.tableOrig) ? "  (address taken from the existing table hook's original)" : "");
        Out(b);
        ++ok;
    }
    Out("citrace nativetrace: " + std::to_string(ok) + " detoured, " + std::to_string(failed) + " failed.");
    Out("  Now hover a quest item and collect it normally. Then run `citrace nativetrace show`.");
    Out("  RVAs above should match the static read: PlayerMouseAction 0x4E3A010, m_Questpickup 0x98732C0,");
    Out("  update_quest 0x52FCF30, CheckPlayerInteraction 0x436380. A mismatch means the build differs.");
}

static void CiNativeTraceReport()
{
    if (!g_CiNatInstalled.load()) { Out("citrace nativetrace: not installed - run `citrace nativetrace` first"); return; }
    Out("citrace nativetrace: native (MmCreateHook, patches the address) vs table (HookOneScript, swaps the script-table pointer)");
    for (const CiNatTarget& t : g_CiNatTargets) {
        const long nat = t.nativeCalls ? *t.nativeCalls : 0;
        const long tab = t.tableCalls ? *t.tableCalls : -1;
        std::string line = std::string("  ") + t.label + ": native=" + std::to_string(nat);
        line += (tab < 0) ? "  table=(not hooked)" : ("  table=" + std::to_string(tab));
        if (nat > 0 && tab == 0) line += "   <-- ran, and the table hook missed every call";
        if (nat == 0 && !*t.origSlot) line += "   (detour not installed)";
        Out(line);
    }
    Out("  CheckPlayerInteraction is the control: the static read has it running from every interactable's");
    Out("  Step event, so a native count of ~0 there would mean this instrument is wrong too.");
}

static void CiNativeTraceReset()
{
    for (CiNatTarget& t : g_CiNatTargets) {
        if (t.nativeCalls) InterlockedExchange(t.nativeCalls, 0);
        if (t.logged) InterlockedExchange(t.logged, 0);
    }
    Out("citrace nativetrace: counters and per-target log budgets reset.");
}

#endif // FORGEPACT_RELEASE (CiFindNearestQuestItem .. CiCaptureStackWalk)

static bool HhIsPlayerInstance(CInstance* instance)
{
    if (!instance) return false;
    try {
        const RValue value = instance->ToRValue();
        if (!g_Yytk->CallBuiltin("instance_exists", { value }).ToBoolean()) return false;
        static int playerObject = -1;
        if (playerObject < 0) playerObject = (int)g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") }).ToDouble();
        if (playerObject < 0) return false;
        const RValue object = g_Yytk->CallBuiltin("variable_instance_get", { value, RValue("object_index") });
        return (int)object.ToDouble() == playerObject
            || g_Yytk->CallBuiltin("object_is_ancestor", { object, RValue((double)playerObject) }).ToBoolean();
    } catch (...) { return false; }
}
static void HhSteal(CInstance* enemyInst, CInstance* killerHint, CInstance* other);
static RValue& Hook_EnemyDestroyKillProc(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    PERF_SCOPE(g_PerfKill);
    InterlockedIncrement(&g_HhHookCalls); InterlockedExchange(&g_HhLastArgc, (long)argc);
    if (g_HhEnabled.load() && S) {
        // Resolve roles from live objects before the original can clean them up.
        // Native captures use enemy self; older call-shape notes use player self.
        CInstance* third = argc >= 3 && A && A[2] ? HhResolveInstance(*A[2]) : nullptr;
        if (CallerIsEnemyInstance(S)) HhSteal(S, third, O);
        else if (CallerIsEnemyInstance(third)) HhSteal(third, S, O);
    }
    RValue& res = g_Orig_EnemyDestroyKillProc ? g_Orig_EnemyDestroyKillProc(S, O, R, argc, A) : R;
    SignatureDropOnKill(S);
    AngelicDropOnKill(S);
    return res;
}

// Every monster death reaches the steal through here, whichever trigger noticed it.
// A monster is handled once: the primary hook and the death-effect fallback can both fire.
static std::deque<int> g_HhHandledOrder;
static std::set<int> g_HhHandledIds;
static volatile long g_HhAltTrigger = 0;
static bool HeadhunterRunning() { return g_HhEnabled.load(); }
static bool HhTakeOnce(int id)
{
    if (id < 0) return true;   // no id to key on: let it through
    if (!g_HhHandledIds.insert(id).second) return false;
    g_HhHandledOrder.push_back(id);
    while (g_HhHandledOrder.size() > 256) { g_HhHandledIds.erase(g_HhHandledOrder.front()); g_HhHandledOrder.pop_front(); }
    return true;
}
static void HhReleaseClaim(int id)
{
    g_HhHandledIds.erase(id);
    g_HhHandledOrder.erase(std::remove(g_HhHandledOrder.begin(), g_HhHandledOrder.end(), id), g_HhHandledOrder.end());
}
static void HhSteal(CInstance* enemyInst, CInstance* killerHint, CInstance* other)
{
    if (!g_HhEnabled.load() || !enemyInst) return;
    int claimedId = -1;
    try {
        if (!CallerIsEnemyInstance(enemyInst)) return;
        RValue enemy = enemyInst->ToRValue();
        const double id = g_Yytk->CallBuiltin("variable_instance_get", { enemy, RValue("id") }).ToDouble();
        if (id < 0.0 || id > INT32_MAX || g_HhHandledIds.count((int)id)) return;
        CInstance* player = HhIsPlayerInstance(killerHint) ? killerHint : nullptr;
        if (!player && HhIsPlayerInstance(other)) player = other;
        if (!player) {
            RValue p;
            if (HhResolveLocalPlayer(p, nullptr)) player = HhResolveInstance(p);
        }
        if (!HhIsPlayerInstance(player)) return;
        if (!HhTakeOnce((int)id)) return;
        claimedId = (int)id;
        // A temporary missing player or failed BuffAdd must not block the next
        // death trigger. Keep the claim only once at least one buff was applied.
        if (HhOnKill(player, enemy)) return;
    } catch (...) {}
    if (claimedId >= 0) HhReleaseClaim(claimedId);
}
// Fallback trigger: the dying monster creates its own death effect.  Resolved lazily so a
// game update that renames the object simply turns the fallback off instead of breaking.
static int g_HhDeathEffectIdx = -2;
static bool HhIsDeathEffect(int objIdx)
{
    if (objIdx < 0) return false;
    if (g_HhDeathEffectIdx == -2) {
        g_HhDeathEffectIdx = -1;
        try { RValue r = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Death_Effect_obj") }); g_HhDeathEffectIdx = (int)r.ToDouble(); } catch (...) {}
    }
    return g_HhDeathEffectIdx >= 0 && objIdx == g_HhDeathEffectIdx;
}
// Second trigger, so a single broken hook cannot silence the mechanic: the dying monster
// creates its own death effect (measured: about one kill in six; the kill hook covers the
// rest).  DropMonsterGold was tried as a third and does not run with the monster as self.
static void HhDeathEffectTrigger(CInstance* S, int objIdx)
{
    if (!g_HhEnabled.load() || !S || !HhIsDeathEffect(objIdx)) return;
    if (!CallerIsEnemyInstance(S)) return;
    InterlockedIncrement(&g_HhAltTrigger);
    HhSteal(S, nullptr, nullptr);
}

// Observe the game's death-effects script even when it chooses not to create
// Enemy_Death_Effect_obj. This uses the dying enemy as self and shares the same
// delivery/duplicate guard as the kill-proc and visual-effect paths.
static PFUNC_YYGMLScript g_Orig_HhDeathEffects = nullptr;
static volatile long g_HhDeathScriptCalls = 0;
static RValue& Hook_HhDeathEffects(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_HhEnabled.load()) {
        InterlockedIncrement(&g_HhDeathScriptCalls);
        HhSteal(S, O, nullptr);
    }
    return g_Orig_HhDeathEffects ? g_Orig_HhDeathEffects(S, O, R, argc, A) : R;
}

static void InstallHeadhunterHook()
{
    if (g_HhHookInstalled) return;
    if (HookOneScript("EnemyDestroyKillProc", "fp_headhunter_kill", (PVOID)Hook_EnemyDestroyKillProc, &g_Orig_EnemyDestroyKillProc)) {
        g_HhHookInstalled = true;
        // The supplemental native detour that used to be layered on here is
        // gone, and deliberately: HookOneScript now installs one itself, for
        // every gameplay hook rather than for the one target a review
        // happened to verify. Keeping this would have patched the trampoline
        // HookOneScript just handed back in g_Orig_EnemyDestroyKillProc,
        // detouring our own code instead of the game's.
    }
}

static void EnableHeadhunter()
{
    InstallHeadhunterHook();
    if (!g_Orig_HhDeathEffects)
        HookOneScript("EnemyDestroyDeathEffects", "fp_hh_death_effects", (PVOID)Hook_HhDeathEffects, &g_Orig_HhDeathEffects);
    // The effect fallback belongs to Headhunter itself. It must not depend on
    // Density, Tyrant's Crown or Special Content having installed these hooks.
    InstallCreateHooks();
    g_HhEnabled.store(g_HhHookInstalled || g_Orig_HhDeathEffects || g_OrigICD || g_OrigICL);
}

// Called after the sidecar is loaded: arm the mechanic only when some forged
// item asks for it (or a command forced it), so ordinary players pay nothing.
static void HeadhunterAutoArm()
{
    bool wanted = g_HhForced.load();
    // 1.3.15: the hook arms whenever a Headhunter item is known; HhEquipped() (the worn
    // belt, read from the player's equippedItems) decides whether a kill steals anything.
    // Skip builtin entries: the plugin's own hardcoded Headhunter belt seed (added
    // purely so a dropped/traded copy is recognised, see AddBuiltInSignatureEntries)
    // always satisfied this loop, so every player - forged item or not - got the kill
    // hook installed and status logged on every launch, panel toggle notwithstanding.
    // Only a sidecar entry the player actually forged counts as "known" here
    // (bug found 2026-09-11: reported on an install with zero forged items).
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (!e.builtin && e.mechanic == "headhunter") { wanted = true; break; }
    if (!wanted) return;
    EnableHeadhunter();
    Out(std::string("headhunter: ") + (g_HhEnabled.load() ? "armed" : "hook failed") + " (" + std::to_string(g_HhDurationSec) + " s, " + std::to_string(g_HhMap.size()) + " mapped affixes)");
}

static void HeadhunterStatus(bool includeMap = true)
{
    std::string m;
    if (includeMap) for (const auto& kv : g_HhMap) m += kv.first + "->" + std::to_string((long long)kv.second.id) + " ";
    Out(std::string("headhunter: ") + (g_HhEnabled.load() ? "ON" : "off") + (g_HhForced.load() ? " (forced)" : "")
        + " hook=" + (g_HhHookInstalled ? "yes" : "no") + " dur=" + std::to_string(g_HhDurationSec) + "s"
        + " kills=" + std::to_string(g_HhKills) + " rare=" + std::to_string(g_HhRareKills)
        + " rarityFlag=" + std::to_string(g_HhRarityKills) + " withAffixData=" + std::to_string(g_HhAffixKills)
        + " buffs=" + std::to_string(g_HhBuffsApplied) + " skippedNoBelt=" + std::to_string(g_HhSkippedNotEquipped)
        + " killHook=" + std::to_string(g_HhHookCalls) + " argc=" + std::to_string(g_HhLastArgc)
        + " deathEffect=" + std::to_string(g_HhAltTrigger)
        + " deathScript=" + std::to_string(g_HhDeathScriptCalls)
        + " deathHook=" + (g_Orig_HhDeathEffects ? "yes" : "no")
        + " effectHooks=" + ((g_OrigICD || g_OrigICL) ? "yes" : "no")
        + " default=" + (g_HhDefaultOn ? std::to_string((long long)g_HhDefault.id) : std::string("off"))
        + (includeMap ? " map=[" + m + "]" : ""));
}

// Record real combat activity without requiring the player to reapply settings.
// The frame loop calls this at most once every 120 frames; subsequent summaries
// are limited to one per 30 seconds, and unchanged/disabled sessions stay quiet.
static void HeadhunterActivityTick()
{
    if (!g_HhEnabled.load()) return;
    const long hook = g_HhHookCalls, death = g_HhDeathScriptCalls, effect = g_HhAltTrigger;
    if (hook == 0 && death == 0 && effect == 0) return;
    static long lastHook = 0, lastDeath = 0, lastEffect = 0;
    static double lastReportMs = -30000.0;
    if (hook == lastHook && death == lastDeath && effect == lastEffect) return;
    const double now = HhNowMs();
    if (now - lastReportMs < 30000.0) return;
    lastHook = hook; lastDeath = death; lastEffect = effect; lastReportMs = now;
    HeadhunterStatus(false);
}

#define DROP_HOOK(NAME) \
    static PFUNC_YYGMLScript g_Orig_##NAME = nullptr; \
    static volatile long g_cnt_##NAME = 0; \
    static int g_mult_##NAME = 1; \
    static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        BP_DIAG_INCREMENT(g_cnt_##NAME); \
        for (int i = 1; i < g_mult_##NAME; i++) { RValue t; if (g_Orig_##NAME) g_Orig_##NAME(S, O, t, argc, A); } \
        RValue& _res = g_Orig_##NAME ? g_Orig_##NAME(S, O, R, argc, A) : R; \
        BP_LOGDROP(#NAME, _res, argc, A); \
        return _res; \
    }

#ifndef FORGEPACT_RELEASE
  #define HH_CREATE_TRACE(NAME) do { if (g_HhTrace) { std::string _a; for (int _i = 0; _i < argc && _i < 4; ++_i) _a += " a" + std::to_string(_i) + "=" + (A && A[_i] ? Describe(*A[_i]) : std::string("?")); Out(std::string("create " #NAME " self=") + HhSelfName(S) + " other=" + HhSelfName(O) + " argc=" + std::to_string(argc) + _a); } } while (0)
#else
  #define HH_CREATE_TRACE(NAME) ((void)0)
#endif
#define ITEM_CREATE_HOOK(NAME) \
    static PFUNC_YYGMLScript g_Orig_##NAME = nullptr; \
    static volatile long g_cnt_##NAME = 0; \
    static int g_mult_##NAME = 1; \
    static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        BP_DIAG_INCREMENT(g_cnt_##NAME); \
        HH_CREATE_TRACE(NAME); \
        RValue& _res = g_Orig_##NAME ? g_Orig_##NAME(S, O, R, argc, A) : R; \
        CustomForgePostProcess(_res, argc, A, strcmp(#NAME, "CreateItemNew") == 0); \
        BP_LOGDROP(#NAME, _res, argc, A); \
        return _res; \
    }

// Player stat multipliers (stat/statadd commands) and the XP combat-text
// display fix moved to ForgePact::StatsManager (module includes anchor
// after HhResolveLocalPlayer, above).
// ===== Enemy movement speed (World -> Enemy Movement Speed) =====
// PathFindStartPath is the one place the game turns an enemy's base speed
// into path speed:  moveSpeedCur = moveSpeed * movementSpdMultiplier
// (x1.35 while sprinting) -> path_start(myPath, moveSpeedCur, ...).
// Scaling moveSpeed only for the duration of that call keeps the walk
// animation in step with the path speed and cannot compound: the base value
// is restored right after, before the enemy's own slow/debuff logic rewrites
// movementSpdMultiplier.  Goblins (GoblinMovement) and online client
// movement use their own paths and are intentionally left alone.
static PFUNC_YYGMLScript g_OrigPathFindStartPath = nullptr;
static double g_EnemySpeedMult = 1.0;
static bool   g_EnemySpeedCtOnly = true;
static volatile long g_EnemySpeedCalls = 0;
static volatile long g_EnemySpeedApplied = 0;

// IsChaosTower only looks at the room name, so a 250 ms cache is exact enough
// and keeps a script call off the per-enemy path-start hot path.
static bool InChaosTowerCached()
{
    static ULONGLONG last = 0;
    static bool inside = false;
    ULONGLONG now = GetTickCount64();
    if (now - last > 250) {
        last = now;
        try { inside = g_Yytk->CallGameScript("gml_Script_IsChaosTower", {}).ToBoolean(); }
        catch (...) { inside = false; }
    }
    return inside;
}

static RValue& Hook_PathFindStartPath(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    // Real counters even in player builds: a path start is a per-enemy,
    // per-second event, not a per-frame hot path, and the status line is the
    // only way a player can prove the hook is doing something.
    InterlockedIncrement(&g_EnemySpeedCalls);
    const double mult = g_EnemySpeedMult;
    if (mult == 1.0 || !S || (g_EnemySpeedCtOnly && !InChaosTowerCached()))
        return g_OrigPathFindStartPath ? g_OrigPathFindStartPath(S, O, R, argc, A) : R;
    RValue inst = RValue(S);
    RValue base;
    bool scaled = false;
    try {
        base = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("moveSpeed") });
        g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("moveSpeed"), RValue(base.ToDouble() * mult) });
        scaled = true;
    } catch (...) {}
    RValue& r = g_OrigPathFindStartPath ? g_OrigPathFindStartPath(S, O, R, argc, A) : R;
    if (scaled) {
        try { g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("moveSpeed"), base }); } catch (...) {}
        InterlockedIncrement(&g_EnemySpeedApplied);
    }
    return r;
}

static void InstallEnemySpeedHook()
{
    if (g_OrigPathFindStartPath) return;
    HookOneScript("PathFindStartPath", "fp_enemy_speed", (PVOID)Hook_PathFindStartPath, &g_OrigPathFindStartPath);
}

// enemyspeed                  -> status
// enemyspeed <mult> [ct|all]  -> e.g. "enemyspeed 1.5 ct"; x1 = vanilla
static void EnemySpeedCmd(const std::string& rest)
{
    std::string a1, a2; a1 = FirstToken(rest, a2);
    a1 = TrimCopy(a1);
    if (!a1.empty() && Lower(a1) != "status") {
        double mult = 1.0;
        try { mult = std::stod(a1); } catch (...) { Out("enemyspeed: usage enemyspeed <mult> [ct|all]"); return; }
        if (!(mult >= 1.0 && mult <= 4.0)) mult = 1.0;   // NaN or out of range -> vanilla
        std::string scope = Lower(TrimCopy(a2));
        if (scope == "ct") g_EnemySpeedCtOnly = true;
        else if (scope == "all") g_EnemySpeedCtOnly = false;
        g_EnemySpeedMult = mult;
        // Shipped builds start without this hook; x1 is native behaviour.
        if (mult > 1.0) InstallEnemySpeedHook();
    }
    char b[200];
    sprintf_s(b, "enemyspeed: x%.2f %s | %s | path starts=%ld scaled=%ld",
              g_EnemySpeedMult, g_EnemySpeedCtOnly ? "Chaos Tower only" : "every zone",
              g_OrigPathFindStartPath ? "hook installed" : "vanilla (no hook)",
              g_EnemySpeedCalls, g_EnemySpeedApplied);
    Out(b);
}


// ===== Relic Drop Pool Filter Mod (Remove owned relics from drop pool) =====
static PFUNC_YYGMLScript g_Orig_DropRelic = nullptr;
static volatile long g_cnt_DropRelic = 0;
static int g_mult_DropRelic = 1;

// (An earlier container-walking maxed-relic scan lived here; it was never
// actually called - GetPlayerMaxedRelics uses the SDK's own
// HeroSiege::Player::GetMaxedRelicIds - and was deleted as dead code rather
// than migrated into ForgePact::RelicFilterMod.)

static RValue& Hook_DropRelic(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    BP_DIAG_INCREMENT(g_cnt_DropRelic);

    std::unordered_set<int> maxedRelics;
    std::vector<std::pair<int, double>> modifiedBases;

    if (ForgePact::RelicFilterMod::Instance().IsEnabled()) {
        ForgePact::RelicFilterMod::Instance().GetPlayerMaxedRelics(maxedRelics);

        if (!maxedRelics.empty() && maxedRelics.size() < static_cast<size_t>(kSeason10RelicRepoCount)) {
            for (int rId : maxedRelics) {
                if (!RepoIndexValid(16, rId)) continue;
                RValue st;
                if (!RepoStruct(16, rId, st)) continue;
                try {
                    RValue dr = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("droprate") });
                    if (dr.m_Kind == VALUE_OBJECT) {
                        RValue curBase = g_Yytk->CallBuiltin("variable_struct_get", { dr, RValue("base") });
                        modifiedBases.push_back({ rId, curBase.ToDouble() });
                        g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(1e18) });
                    }
                } catch (...) {}
            }
        }
    }

    for (int i = 1; i < g_mult_DropRelic; i++) {
        RValue t;
        if (g_Orig_DropRelic) g_Orig_DropRelic(S, O, t, argc, A);
    }
    RValue& _res = g_Orig_DropRelic ? g_Orig_DropRelic(S, O, R, argc, A) : R;

    for (const auto& p : modifiedBases) {
        RValue st;
        if (RepoStruct(16, p.first, st)) {
            try {
                RValue dr = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("droprate") });
                if (dr.m_Kind == VALUE_OBJECT) {
                    g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(p.second) });
                }
            } catch (...) {}
        }
    }

    // Secondary guarantee: If dropped result or instance is in maxedRelics, reroll to unmaxed relic
    if (ForgePact::RelicFilterMod::Instance().IsEnabled() && !maxedRelics.empty() && maxedRelics.size() < static_cast<size_t>(kSeason10RelicRepoCount)) {
        std::vector<int> validRelics;
        for (int i = 0; i < kSeason10RelicRepoCount; ++i) {
            if (maxedRelics.find(i) == maxedRelics.end()) {
                validRelics.push_back(i);
            }
        }
        if (!validRelics.empty()) {
            try {
                if (_res.m_Kind == VALUE_OBJECT && _res.m_Object) {
                    if (g_Yytk->CallBuiltin("variable_struct_exists", { _res, RValue("b") }).ToBoolean()) {
                        int b = static_cast<int>(g_Yytk->CallBuiltin("variable_struct_get", { _res, RValue("b") }).ToDouble());
                        if (maxedRelics.find(b) != maxedRelics.end()) {
                            int pick = validRelics[std::rand() % validRelics.size()];
                            g_Yytk->CallBuiltin("variable_struct_set", { _res, RValue("b"), RValue(static_cast<double>(pick)) });
                        }
                    }
                }
            } catch (...) {}
        }
    }

    BP_LOGDROP("DropRelic", _res, argc, A);
    return _res;
}
// The 19 domain hooks above (DropBossGems .. DropOreMaterials, including
// DropKeys' dev-only diagnostic variant) moved to ForgePact::DropManager
// (module includes anchor near the top of the file, after FirstToken).
// DropRelic stays here - shared chokepoint with RelicFilterMod, see its
// own comment. LootGroundCreate/LootGroundCreateFromItem below are a
// separate research-tracing feature, not part of dropmult, and stay too.
// Esyayi YERE koyan fonksiyon - "yaratildi" ile "dustu" farkini olcmek icin.
DROP_HOOK(LootGroundCreate)
// LootGroundCreate calisma aninda HIC cagrilmadi (olculdu: 0).
// Gercek yere-koyma yolu bu olmali.
DROP_HOOK(LootGroundCreateFromItem)
// item CREATION hooks: fire for every item built (incl. all jewels on save load).
// LogDrop captures (raw definition n -> computed itemStatStruct) automatically.
ITEM_CREATE_HOOK(CreateItemNew)
ITEM_CREATE_HOOK(CreateItemInit)
ITEM_CREATE_HOOK(GenerateItemRandomStats)

// ---- ITEM DICTIONARY: passively learn (raw item fields -> displayed name/stats) ----
// Hook the item naming/stat functions; each time the game shows a REAL item, log
// {raw item json, output string}. Dedup so each distinct item is logged once.
// Build a reusable (raw n-array -> affix) dictionary just by browsing inventory.
static std::unordered_set<std::string> g_SeenItemJson;
static void LogItemDict(const char* fn, RValue* itemRV, RValue& out)
{
    try {
        if (!itemRV || itemRV->m_Kind != VALUE_OBJECT) return;
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        RValue jsItem; g_Yytk->CallBuiltinEx(jsItem, "json_stringify", g, g, { *itemRV });
        std::string itemStr = jsItem.ToString();
        if (itemStr.find("\"b\":") == std::string::npos) return; // must look like an item
        std::string key = std::string(fn) + "|" + itemStr;
        if (!g_SeenItemJson.insert(key).second) return;
        RValue jsOut; g_Yytk->CallBuiltinEx(jsOut, "json_stringify", g, g, { out });
        std::ofstream of(IPC_DIR + "\\itemdict.jsonl", std::ios::app);
        of << "{\"fn\":\"" << fn << "\",\"item\":" << itemStr << ",\"out\":" << jsOut.ToString() << "}\n";
    } catch (...) {}
}
// SWEEP: when armed, on the next hovered item, vary one n-slot across [lo..hi],
// re-name via the original namer for each, log (value -> name). Restores the slot after.
// One hover of one item -> the entire affix-pool mapping. Scalable: no need to obtain items.
static volatile bool g_SweepArmed = false;
static int g_SweepLo = 0, g_SweepHi = 0, g_SweepSlot = 0;
static PFUNC_YYGMLScript g_Orig_GetItemTooltipString = nullptr;
static RValue& Hook_GetItemTooltipString(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    RValue& r = g_Orig_GetItemTooltipString ? g_Orig_GetItemTooltipString(S, O, R, argc, A) : R;
    if (argc >= 1 && A) LogItemDict("tooltip", A[0], r);
    if (g_SweepArmed && argc >= 1 && A && A[0] && A[0]->m_Kind == VALUE_OBJECT && g_Orig_GetItemTooltipString) {
        g_SweepArmed = false;
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        RValue narr; bool got = false; RValue saved;
        try {
            g_Yytk->CallBuiltinEx(narr, "variable_struct_get", g, g, { *A[0], RValue("n") });
            if (narr.m_Kind == VALUE_ARRAY) {
                g_Yytk->CallBuiltinEx(saved, "array_get", g, g, { narr, RValue((double)g_SweepSlot) });
                got = true;
                RValue jb; g_Yytk->CallBuiltinEx(jb, "json_stringify", g, g, { *A[0] });
                std::ofstream of(IPC_DIR + "\\sweep.txt", std::ios::app);
                of << "=== SWEEP n[" << g_SweepSlot << "]=" << g_SweepLo << ".." << g_SweepHi << " base=" << jb.ToString() << " ===\n";
                int lo = g_SweepLo, hi = g_SweepHi; if (hi - lo > 1000) hi = lo + 1000;
                for (int i = lo; i <= hi; i++) {
                    RValue d; g_Yytk->CallBuiltinEx(d, "array_set", g, g, { narr, RValue((double)g_SweepSlot), RValue((double)i) });
                    RValue nm; g_Orig_GetItemTooltipString(S, O, nm, argc, A);
                    std::string s = nm.ToString();
                    for (auto& ch : s) if (ch == '\n' || ch == '\r') ch = ' ';
                    of << i << "\t" << s << "\n";
                }
                of << "=== SWEEP done ===\n";
                Out("sweep: done, wrote sweep.txt");
            } else { Out("sweep: hovered item has no n array"); }
        } catch (...) { Out("sweep: EXCEPTION"); }
        if (got) { try { RValue d; g_Yytk->CallBuiltinEx(d, "array_set", g, g, { narr, RValue((double)g_SweepSlot), saved }); } catch (...) {} }
    }
    return r;
}
#ifndef FORGEPACT_RELEASE
static int g_TipTraceLeft = 0;   // "tiptrace [n]": log the next n tooltip/stat-string calls
static int g_TipDrawTraceLeft = 0;
static std::string TipTraceArgs(int argc, RValue** A)
{
    std::string a;
    for (int i = 0; i < argc && i < 12; ++i) {
        std::string d = A && A[i] ? Describe(*A[i]) : std::string("?");
        if (A && A[i] && A[i]->m_Kind == VALUE_OBJECT) {
            try {
                RValue isItem = g_Yytk->CallBuiltin("variable_struct_exists", { *A[i], RValue("itemInfoStruct") });
                if (isItem.ToBoolean()) d = "item-struct";
                else {
                    CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
                    RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { *A[i] });
                    d = "struct" + js.ToString();
                }
            } catch (...) {}
        }
        if (d.size() > 220) d = d.substr(0, 220) + "...";
        for (auto& ch : d) if (ch == '\n' || ch == '\r') ch = '~';
        a += " a" + std::to_string(i) + "=" + d;
    }
    return a;
}
#endif
static PFUNC_YYGMLScript g_Orig_GetItemStatString = nullptr;
static RValue& Hook_GetItemStatString(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    RValue& r = g_Orig_GetItemStatString ? g_Orig_GetItemStatString(S, O, R, argc, A) : R;
#ifndef FORGEPACT_RELEASE
    if (g_TipTraceLeft > 0) {
        --g_TipTraceLeft;
        std::string out = r.ToString(); if (out.size() > 160) out = out.substr(0, 160) + "...";
        for (auto& ch : out) if (ch == '\n' || ch == '\r') ch = '~';
        Out("tiptrace GetItemStatString argc=" + std::to_string(argc) + TipTraceArgs(argc, A) + " -> \"" + out + "\"");
    }
#endif
    if (argc >= 1 && A) LogItemDict("statstr", A[0], r);
    // SWEEP: when armed and this is an uncut jewel, vary definition n[slot] across [lo..hi],
    // recompute via ReCreateItem, dump each resulting item. One jewel -> whole affix pool.
    if (g_SweepArmed && argc >= 1 && A && A[0] && A[0]->m_Kind == VALUE_OBJECT) {
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        try {
            RValue defn; g_Yytk->CallBuiltinEx(defn, "variable_struct_get", g, g, { *A[0], RValue("itemDefinitionStruct") });
            if (defn.m_Kind == VALUE_OBJECT) {
                RValue bv; g_Yytk->CallBuiltinEx(bv, "variable_struct_get", g, g, { defn, RValue("b") });
                int bb = (int)bv.ToDouble();
                RValue narr; g_Yytk->CallBuiltinEx(narr, "variable_struct_get", g, g, { defn, RValue("n") });
                if (bb >= 97 && bb <= 111 && narr.m_Kind == VALUE_ARRAY) {
                    g_SweepArmed = false;
                    RValue saved; g_Yytk->CallBuiltinEx(saved, "array_get", g, g, { narr, RValue((double)g_SweepSlot) });
                    std::ofstream of(IPC_DIR + "\\sweep.jsonl", std::ios::app);
                    int lo = g_SweepLo, hi = g_SweepHi; if (hi - lo > 1000) hi = lo + 1000;
                    for (int i = lo; i <= hi; i++) {
                        RValue d; g_Yytk->CallBuiltinEx(d, "array_set", g, g, { narr, RValue((double)g_SweepSlot), RValue((double)i) });
                        RValue fresh; AurieStatus st = g_Yytk->CallGameScriptEx(fresh, "gml_Script_ReCreateItem", S, S, { *A[0] });
                        RValue tgt = (AurieSuccess(st) && fresh.m_Kind == VALUE_OBJECT) ? fresh : *A[0];
                        RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { tgt });
                        of << "{\"i\":" << i << ",\"it\":" << js.ToString() << "}\n";
                    }
                    RValue d2; g_Yytk->CallBuiltinEx(d2, "array_set", g, g, { narr, RValue((double)g_SweepSlot), saved });
                    Out("sweep: done (uncut jewel b=" + std::to_string(bb) + ") -> sweep.jsonl");
                }
            }
        } catch (...) { Out("sweep(statstr): EXCEPTION"); }
    }
    return r;
}

// Drop carpani kancalari - HER IKI derlemede kurulur; `dropmult` bunlara dayanir.
static bool g_DropMultHooksInstalled = false;
static void InstallDropMultHooks()
{
    if (g_DropMultHooksInstalled) return;
    // Mark before installing so a partially unavailable optional script cannot
    // cause duplicate MmCreateHook attempts on the hooks that did succeed.
    g_DropMultHooksInstalled = true;
    HookOneScript("DropRelic",           "bp_drelic",   (PVOID)Hook_DropRelic,           &g_Orig_DropRelic);
    ForgePact::DropManager::Instance().InstallHooks();
}

// ===== Forged tooltip rows (Custom Forge `affix=` / Headhunter) ==================
// Live-traced 2026-09-04: DrawInventoryItemV2(x, y, scale, item, ...) draws the whole
// inventory tooltip and calls DrawInventoryStatsNew(x, y, item, statId, label, format,
// style, ...) once per known stat.  That helper draws a row only when the item has the
// stat and returns the row height (30) or 0; the caller adds the return value to its y
// cursor.  Forged rows go in front of the first real stat row (format 2 = percent,
// 3 = flat): ours are drawn at y, the game's row is handed y + rows*30, and the combined
// height is returned so everything below (stats, lore, requirements, the box itself)
// moves down with it.
static PFUNC_YYGMLScript g_Orig_DrawInventoryItemV2 = nullptr;
static PFUNC_YYGMLScript g_Orig_DrawInventoryStatsNew = nullptr;
static int g_TipStatCallsInTooltip = 0;
static std::vector<std::string> g_TipRows;   // rows still to draw in the current tooltip pass
static bool g_TipRowsPending = false;
static bool g_TipInsideStat = false;
static const double kTipRowHeight = 30.0;

static std::string HhTooltipLine()
{
    return "Steals the affixes of slain rare monsters for " + std::to_string((int)(g_HhDurationSec + 0.5)) + "s";
}

// Rows for a forged item: explicit fp_affix text (split on newlines, max 3), else the
// built-in Headhunter line for mechanic=headhunter items, else nothing.
static std::vector<std::string> ForgedTooltipRows(const RValue& item)
{
    std::vector<std::string> rows;
    try {
        if (item.m_Kind != VALUE_OBJECT || !item.m_Object) return rows;
        std::string text;
        RValue hasAffix = g_Yytk->CallBuiltin("variable_struct_exists", { item, RValue("fp_affix") });
        if (hasAffix.ToBoolean()) text = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("fp_affix") }).ToString();
        else if (HhItemIsHeadhunter(item)) text = HhTooltipLine();
        else if (ForgedItemMechanicIs(item, "tyrant")) text = "Rare monsters hunt you\nMonsters near you rise to rare more often\nRares bear one more affix";
        else if (ForgedItemMechanicIs(item, "beacon")) text = "Every monster on the map hunts you\nThey never lose your trail";
        if (text.empty()) return rows;
        std::stringstream stream(text);
        std::string row;
        while (std::getline(stream, row, '\n')) {
            row = TrimCopy(row);
            if (!row.empty() && rows.size() < 3) rows.push_back(row);
        }
    } catch (...) { rows.clear(); }
    return rows;
}

// Draws one tooltip text row the way DrawInventoryStatsNew does (centred on x, current
// font, 2 px dark outline) in gold, restoring the draw state afterwards.
static void HhDrawTooltipLine(double x, double y, const std::string& text)
{
    RValue prevHalign = g_Yytk->CallBuiltin("draw_get_halign", {});
    RValue prevColour = g_Yytk->CallBuiltin("draw_get_colour", {});
    g_Yytk->CallBuiltin("draw_set_halign", { RValue(1.0) });
    g_Yytk->CallBuiltin("draw_set_colour", { RValue(0.0) });
    const double o = 2.0;
    g_Yytk->CallBuiltin("draw_text", { RValue(x - o), RValue(y), RValue(text) });
    g_Yytk->CallBuiltin("draw_text", { RValue(x + o), RValue(y), RValue(text) });
    g_Yytk->CallBuiltin("draw_text", { RValue(x), RValue(y - o), RValue(text) });
    g_Yytk->CallBuiltin("draw_text", { RValue(x), RValue(y + o), RValue(text) });
    RValue gold = g_Yytk->CallBuiltin("make_colour_rgb", { RValue(242.0), RValue(196.0), RValue(98.0) });
    g_Yytk->CallBuiltin("draw_set_colour", { gold });
    g_Yytk->CallBuiltin("draw_text", { RValue(x), RValue(y), RValue(text) });
    g_Yytk->CallBuiltin("draw_set_colour", { prevColour });
    g_Yytk->CallBuiltin("draw_set_halign", { prevHalign });
}

static bool TipStatPresent(RValue** A, int argc)
{
    try {
        if (argc < 4 || !A || !A[2] || !A[3] || A[2]->m_Kind != VALUE_OBJECT) return false;
        RValue has = g_Yytk->CallBuiltin("variable_struct_exists", { *A[2], RValue("itemStatStruct") });
        if (!has.ToBoolean()) return false;
        RValue stats = g_Yytk->CallBuiltin("variable_struct_get", { *A[2], RValue("itemStatStruct") });
        std::string key = std::to_string((long long)A[3]->ToDouble());
        RValue present = g_Yytk->CallBuiltin("variable_struct_exists", { stats, RValue(key) });
        return present.ToBoolean();
    } catch (...) { return false; }
}

static RValue& Hook_DrawInventoryItemV2(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
#ifndef FORGEPACT_RELEASE
    bool trace = g_TipTraceLeft > 0;
    if (trace) {
        --g_TipTraceLeft;
        Out("tiptrace DrawInventoryItemV2 self=" + HhSelfName(S) + " other=" + HhSelfName(O) + " argc=" + std::to_string(argc) + TipTraceArgs(argc, A));
    }
#endif
    g_TipRows.clear();
    g_TipRowsPending = false;
    try {
        if (argc > 3 && A && A[3]) { g_TipRows = ForgedTooltipRows(*A[3]); g_TipRowsPending = !g_TipRows.empty(); }
    } catch (...) { g_TipRows.clear(); g_TipRowsPending = false; }
    g_TipStatCallsInTooltip = 0;
    RValue& r = g_Orig_DrawInventoryItemV2 ? g_Orig_DrawInventoryItemV2(S, O, R, argc, A) : R;
#ifndef FORGEPACT_RELEASE
    if (trace) Out("   DrawInventoryItemV2 -> " + Describe(r) + " statLines=" + std::to_string(g_TipStatCallsInTooltip) + " forgedRows=" + std::to_string(g_TipRows.size()));
#endif
    g_TipRowsPending = false;
    return r;
}

static RValue& Hook_DrawInventoryStatsNew(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    ++g_TipStatCallsInTooltip;
#ifndef FORGEPACT_RELEASE
    bool trace = false;
    if (g_TipTraceLeft > 0) {
        double fmt = -1.0; try { if (argc > 5 && A && A[5] && (A[5]->m_Kind == VALUE_REAL || A[5]->m_Kind == VALUE_INT32 || A[5]->m_Kind == VALUE_INT64)) fmt = A[5]->ToDouble(); } catch (...) {}
        trace = g_TipStatCallsInTooltip <= 2 || (fmt != 2.0 && fmt != 3.0) || TipStatPresent(A, argc);
        if (trace) --g_TipTraceLeft;
    }
#endif
    auto isNum = [](const RValue* v) { return v && (v->m_Kind == VALUE_REAL || v->m_Kind == VALUE_INT32 || v->m_Kind == VALUE_INT64); };
    if (g_TipRowsPending && argc > 5 && A && isNum(A[0]) && isNum(A[1]) && isNum(A[5])) {
        double fmt = -1.0; try { fmt = A[5]->ToDouble(); } catch (...) { fmt = -1.0; }
        if ((fmt == 2.0 || fmt == 3.0) && TipStatPresent(A, argc)) {
            try {
                const double x = A[0]->ToDouble(), y = A[1]->ToDouble();
                double extra = 0.0;
                for (const std::string& row : g_TipRows) { HhDrawTooltipLine(x, y + extra, row); extra += kTipRowHeight; }
                g_TipRowsPending = false;
                RValue shifted(y + extra);
                RValue* savedY = A[1];
                A[1] = &shifted;
                RValue& rr = g_Orig_DrawInventoryStatsNew ? g_Orig_DrawInventoryStatsNew(S, O, R, argc, A) : R;
                A[1] = savedY;
#ifndef FORGEPACT_RELEASE
                if (g_TipTraceLeft > 0) Out("tiptrace forged rows at y=" + std::to_string(y) + " (" + std::to_string(g_TipRows.size()) + " rows), game row moved to y=" + std::to_string(y + extra) + " -> " + Describe(rr));
#endif
                R = RValue(rr.ToDouble() + extra);
                return R;
            } catch (...) { g_TipRowsPending = false; }
        }
    }
    g_TipInsideStat = g_TipRowsPending;
    RValue& r = g_Orig_DrawInventoryStatsNew ? g_Orig_DrawInventoryStatsNew(S, O, R, argc, A) : R;
    g_TipInsideStat = false;
#ifndef FORGEPACT_RELEASE
    if (trace) Out("tiptrace DrawInventoryStatsNew #" + std::to_string(g_TipStatCallsInTooltip) + " present=" + (TipStatPresent(A, argc) ? "yes" : "no") + " argc=" + std::to_string(argc) + TipTraceArgs(argc, A) + " -> " + Describe(r));
#endif
    return r;
}

static bool g_ForgedTooltipHooksAttempted = false;
static void InstallForgedTooltipHooks()
{
    if (g_ForgedTooltipHooksAttempted) return;
    g_ForgedTooltipHooksAttempted = true;
    HookOneScript("DrawInventoryItemV2",  "fp_tip_item", (PVOID)Hook_DrawInventoryItemV2,  &g_Orig_DrawInventoryItemV2);
    HookOneScript("DrawInventoryStatsNew","fp_tip_stat", (PVOID)Hook_DrawInventoryStatsNew,&g_Orig_DrawInventoryStatsNew);
}

#ifndef FORGEPACT_RELEASE
static PFUNC_YYGMLScript g_Orig_DrawTextOutline = nullptr;
static RValue& Hook_TraceDrawTextOutline(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    if (g_TipInsideStat && g_TipDrawTraceLeft > 0) {
        --g_TipDrawTraceLeft;
        Out("tiptrace draw_text_outline self=" + HhSelfName(S) + " argc=" + std::to_string(argc) + TipTraceArgs(argc, A));
    }
    return g_Orig_DrawTextOutline ? g_Orig_DrawTextOutline(S, O, R, argc, A) : R;
}
static PFUNC_YYGMLScript g_Orig_DrawTooltip = nullptr;
static RValue& Hook_DrawTooltip(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    if (g_TipTraceLeft > 0) {
        --g_TipTraceLeft;
        Out("tiptrace DrawTooltip argc=" + std::to_string(argc) + TipTraceArgs(argc, A));
    }
    return g_Orig_DrawTooltip ? g_Orig_DrawTooltip(S, O, R, argc, A) : R;
}
#endif


#ifndef FORGEPACT_RELEASE
// --- spawner CheckSpawn trace (Beacon: spawn-as-if-near groundwork) --------------------
static int g_SpawnTraceLeft = 0;
static long g_SpawnCheckCalls = 0, g_SpawnCheckSpawned = 0;
static double g_SpawnDistMin = -1, g_SpawnDistMax = -1, g_SpawnNoMin = -1;   // spawn distances seen while tracing
static PFUNC_YYGMLScript g_OrigCreatorCheckSpawn = nullptr;
static RValue& Hook_TraceCreatorCheckSpawn(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    ++g_SpawnCheckCalls;
    bool tr = g_SpawnTraceLeft > 0;
    double dist = -1, cx = 0, cy = 0; int before = -1;
    RValue enemyObj;
    if (tr && S) {
        try {
            RValue inst = S->ToRValue();
            cx = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
            cy = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
            RValue player; if (HhResolveLocalPlayer(player)) {
                double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
                double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
                dist = std::sqrt((cx - px) * (cx - px) + (cy - py) * (cy - py));
            }
            enemyObj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
            before = (int)g_Yytk->CallBuiltin("instance_number", { enemyObj }).ToDouble();
        } catch (...) {}
    }
    std::string args = tr ? AggroArgs(argc, A) : std::string();
    RValue& r = g_OrigCreatorCheckSpawn ? g_OrigCreatorCheckSpawn(S, O, R, argc, A) : R;
    if (tr) {
        int after = before;
        try { if (before >= 0) after = (int)g_Yytk->CallBuiltin("instance_number", { enemyObj }).ToDouble(); } catch (...) {}
        const bool spawned = after > before;
        if (spawned) { ++g_SpawnCheckSpawned; if (dist >= 0) { if (g_SpawnDistMin < 0 || dist < g_SpawnDistMin) g_SpawnDistMin = dist; if (dist > g_SpawnDistMax) g_SpawnDistMax = dist; } }
        else if (dist >= 0 && (g_SpawnNoMin < 0 || dist < g_SpawnNoMin)) g_SpawnNoMin = dist;
        // log every spawn, plus one in twenty of the silent checks so the budget lasts
        if (spawned || (g_SpawnCheckCalls % 20) == 0) {
            --g_SpawnTraceLeft;
            Out(std::string(spawned ? "spawn SPAWNED  " : "spawn check    ") + "self=" + TyInstName(S ? S->ToRValue() : RValue()) + " argc=" + std::to_string(argc) + args
                + " dist=" + std::to_string((long long)dist) + " at=" + std::to_string((long long)cx) + "," + std::to_string((long long)cy)
                + " -> " + Describe(r) + " enemies " + std::to_string(before) + "->" + std::to_string(after));
        }
    }
    return r;
}
static void InstallSpawnTraceHook()
{
    if (g_OrigCreatorCheckSpawn) return;
    HookOneScriptTable("anon@849@gml_Object_Enemy_Creator_obj_Create_0", "bp_tr_checkspawn", (PVOID)Hook_TraceCreatorCheckSpawn, &g_OrigCreatorCheckSpawn);
}
#endif
#ifndef FORGEPACT_RELEASE
// Esya inceleme/duzenleme kancalari - yalnizca gelistirme derlemesi.

static void InstallItemInspectHooks()
{
    InstallSpawnTraceHook();
    InstallAggroTraceHooks();
    InstallBeaconHook();
    InstallTyrantHook();   // research build: always, for raritytrace / experiments
    HookOneScriptTable("DrawTooltip",         "bp_drawtip",  (PVOID)Hook_DrawTooltip,         &g_Orig_DrawTooltip);
    InstallForgedTooltipHooks();   // research build: always, so tiptrace can watch any item
    HookOneScriptTable("draw_text_outline",   "bp_dto",      (PVOID)Hook_TraceDrawTextOutline, &g_Orig_DrawTextOutline);
    HookOneScriptTable("GetItemTooltipString","bp_gitip",    (PVOID)Hook_GetItemTooltipString,&g_Orig_GetItemTooltipString);
    HookOneScriptTable("GetItemStatString",   "bp_gistat",   (PVOID)Hook_GetItemStatString,   &g_Orig_GetItemStatString);
    if (!g_Orig_CreateItemNew)
        HookOneScriptTable("CreateItemNew",       "bp_citemn",   (PVOID)Hook_CreateItemNew,       &g_Orig_CreateItemNew);
    if (!g_Orig_CreateItemInit)
        HookOneScriptTable("CreateItemInit",      "bp_citemi",   (PVOID)Hook_CreateItemInit,      &g_Orig_CreateItemInit);
    if (!g_Orig_GenerateItemRandomStats)
        HookOneScriptTable("GenerateItemRandomStats","bp_girs",  (PVOID)Hook_GenerateItemRandomStats,&g_Orig_GenerateItemRandomStats);
    HookOneScriptTable("LootGroundCreate",    "bp_lgc",      (PVOID)Hook_LootGroundCreate,    &g_Orig_LootGroundCreate);
    HookOneScriptTable("LootGroundCreateFromItem", "bp_lgcfi", (PVOID)Hook_LootGroundCreateFromItem, &g_Orig_LootGroundCreateFromItem);
}
#endif

static void InstallCustomForgeItemHooks()
{
    if (g_CustomForgeHooksAttempted || g_CustomForgeEntries.empty()) return;
    g_CustomForgeHooksAttempted = true;
    if (!g_Orig_CreateItemNew)
        HookOneScript("CreateItemNew", "fp_customforge_new",
                      (PVOID)Hook_CreateItemNew, &g_Orig_CreateItemNew);
    if (!g_Orig_CreateItemInit)
        HookOneScript("CreateItemInit", "fp_customforge_init",
                      (PVOID)Hook_CreateItemInit, &g_Orig_CreateItemInit);
    if (!g_Orig_GenerateItemRandomStats)
        HookOneScript("GenerateItemSpecialStats", "fp_specialstats", (PVOID)Hook_GenerateItemSpecialStats, &g_Orig_GenerateItemSpecialStats);
        HookOneScript("GetRuneword", "fp_runeword", (PVOID)Hook_GetRuneword, &g_Orig_GetRuneword);
        HookOneScript("GenerateItemRandomStats", "fp_customforge_stats",
                      (PVOID)Hook_GenerateItemRandomStats, &g_Orig_GenerateItemRandomStats);
    g_CustomForgeHooksActive = g_Orig_CreateItemNew || g_Orig_CreateItemInit ||
                               g_Orig_GenerateItemRandomStats;
    for (const CustomForgeEntry& entry : g_CustomForgeEntries) {
        if (!entry.affix.empty() || !entry.mechanic.empty()) { InstallForgedTooltipHooks(); break; }
    }
    WriteCustomForgeStatus(g_CustomForgeHooksActive ? "runtime hooks installed"
                                                     : "runtime hook installation failed");
}


static void DropStats()
{
    char b[400];
    sprintf_s(b, "dropstats: Relic c=%ld x%d", g_cnt_DropRelic, g_mult_DropRelic);
    Out(b);
    ForgePact::DropManager::Instance().PrintStats();
}

static void SetDropMult(const std::string& name, int n)
{
    std::string l = Lower(name);
    if (n < 1) n = 1;
    // Shipped builds start without drop hooks. Install them only when a real
    // multiplier is requested; x1 is native behaviour and needs no interception.
    // (Also installs the shared DropRelic hook - see InstallDropMultHooks.)
    if (n > 1) InstallDropMultHooks();
    // "relic" is the one target ForgePact::DropManager doesn't own - see its
    // class comment (Hook_DropRelic is shared with RelicFilterMod).
    if (l == "relic") { g_mult_DropRelic = n; Out("dropmult " + name + " -> " + std::to_string(n)); return; }
    ForgePact::DropManager::Instance().SetMultiplier(name, n);
}

// ===== Chaos Tower spawn-rate hooks (instrument + force) =====
static PFUNC_YYGMLScript g_OrigRandomCT = nullptr; static volatile long g_ctRandomCalls = 0;
static PFUNC_YYGMLScript g_OrigZoneGenCT = nullptr; static volatile long g_ctZoneGenCalls = 0;
static double g_ctForce = NAN;          // if set, override RandomChaosTower return (scalar)
static int g_ctArrayN = 0;              // if >0, RandomChaosTower returns array of N zone numbers (1..N)
static std::vector<double> g_ctCustom;  // if non-empty, RandomChaosTower returns this exact array
static std::string g_ctReturnLog;       // distinct natural returns of RandomChaosTower

static std::string g_ctArrayDump;
static std::string g_ctCaller;

static RValue& HookRandomCT(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    void* ret = _ReturnAddress();
    InterlockedIncrement(&g_ctRandomCalls);
    RValue& r = g_OrigRandomCT ? g_OrigRandomCT(S, O, R, argc, A) : R;
    try {
        if (g_Base && g_ctCaller.empty()) {
            char rb[32]; sprintf_s(rb, "rva=0x%llX", (unsigned long long)((uintptr_t)ret - g_Base));
            g_ctCaller = rb;
        }
        if (g_ctReturnLog.size() < 200) {
            std::string d = Describe(r);
            if (g_ctReturnLog.find(d) == std::string::npos) g_ctReturnLog += "{" + d + "}";
        }
        // dump array contents once via json_stringify (reliable)
        if (g_ctArrayDump.empty() && r.m_Kind == VALUE_ARRAY) {
            try {
                RValue js = g_Yytk->CallBuiltin("json_stringify", { r });
                g_ctArrayDump = js.ToString();
            } catch (...) { g_ctArrayDump = "(json_stringify failed)"; }
        }
    } catch (...) {}
    // override: return an exact custom array (set via ctarray) to control chaos tower zones
    if (!g_ctCustom.empty()) {
        std::vector<RValue> v;
        for (double d : g_ctCustom) v.push_back(RValue(d));
        R = RValue(v);
        return R;
    }
    // override: return a bigger array of zone numbers (1..N) to spawn more chaos towers
    if (g_ctArrayN > 0) {
        std::vector<RValue> v;
        int n = g_ctArrayN; if (n > 30) n = 30;
        for (int i = 1; i <= n; i++) v.push_back(RValue((double)i));
        R = RValue(v);
        return R;
    }
    if (!std::isnan(g_ctForce)) { R = RValue(g_ctForce); return R; }
    return r;
}
static RValue& HookZoneGenCT(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    InterlockedIncrement(&g_ctZoneGenCalls);
    return g_OrigZoneGenCT ? g_OrigZoneGenCT(S, O, R, argc, A) : R;
}

#ifndef FORGEPACT_RELEASE
// ---- GPV / SPV: oyunun sayisal anahtarli ortak deger deposu ----------------
// Bulundu (2026-08-28): Blood Pact Edit ekrani, LoadDrops ve Controller_obj'in
// Create olayi hep bu ikiliyi kullaniyor.  Modifiyerler isimle degil KIMLIKLE
// saklandigi icin isim aramalari bosa cikmisti.  Burada her okuma/yazmayi
// kimligiyle birlikte kaydediyoruz; kimlik -> anlam eslemesi boyle cikacak.
static PFUNC_YYGMLScript g_OrigGPV = nullptr;
static PFUNC_YYGMLScript g_OrigSPV = nullptr;
static int g_GpvLog = 0;
static std::map<std::string, long> g_GpvGorulen;   // "id=deger" -> kac kez

static void GpvYaz(const char* etiket, int argc, RValue** A, RValue* sonuc)
{
    try {
        std::string s = etiket;
        for (int i = 0; i < argc && i < 4; i++)
            s += std::string(" arg") + std::to_string(i) + "=" + (A && A[i] ? Describe(*A[i]) : "(null)");
        if (sonuc) s += " -> " + Describe(*sonuc);
        auto it = g_GpvGorulen.find(s);
        if (it != g_GpvGorulen.end()) { it->second++; return; }   // tekrarlari sikistir
        g_GpvGorulen[s] = 1;
        std::ofstream f(IPC_DIR + "\\gpv.txt", std::ios::app);
        f << s << "\n";
        f.flush();
    } catch (...) {}
}

static RValue& HookGPV(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    RValue& r = g_OrigGPV ? g_OrigGPV(S, O, R, argc, A) : R;
    if (g_GpvLog > 0) { GpvYaz("GET", argc, A, &r); }
    return r;
}

static RValue& HookSPV(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_GpvLog > 0) { GpvYaz("SET", argc, A, nullptr); }
    return g_OrigSPV ? g_OrigSPV(S, O, R, argc, A) : R;
}
#endif

#ifndef FORGEPACT_RELEASE
// ---- scount: HERHANGI bir GML betigini say ve donusunu kaydet -------------
// Bugun uc kez ayni seye ihtiyac duyuldu (StatMagicFind cagriliyor mu,
// ZoneGenChaosTower cagriliyor mu, EnemyGiveExperience cagriliyor mu) ve her
// seferinde elle kanca yazildi.  Burada sabit sayida genel yuva var; komutla
// istenen betige baglanir.
struct SayacYuvasi {
    const char*        ad;      // bagli betik (bos = kullanilmiyor)
    PFUNC_YYGMLScript  orij;
    volatile long      sayi;
    std::string        ornek;   // ilk birkac cagrinin arguman/donus ozeti
};
static SayacYuvasi g_Yuva[8] = {};

template <int N>
static RValue& HookSayac(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    SayacYuvasi& y = g_Yuva[N];
    InterlockedIncrement(&y.sayi);
    RValue& r = y.orij ? y.orij(S, O, R, argc, A) : R;
    if (y.sayi <= 3) {
        try {
            std::string s = "  #" + std::to_string(y.sayi) + " argc=" + std::to_string(argc);
            for (int i = 0; i < argc && i < 3; i++)
                s += " a" + std::to_string(i) + "=" + (A && A[i] ? Describe(*A[i]) : "(null)");
            // Stat* betikleri DIZI donduruyor (olculdu 2026-08-28).  Describe
            // yalnizca "array" yaziyor; hangi elemanin gercek stat oldugunu
            // gormek icin icerigi de dokuyoruz.
            s += " -> " + Describe(r);
            if (r.m_Kind == VALUE_ARRAY || r.m_Kind == VALUE_OBJECT) {
                try {
                    std::string js = g_Yytk->CallBuiltin("json_stringify", { r }).ToString();
                    if (js.size() > 300) js = js.substr(0, 300) + "...";
                    s += " = " + js;
                } catch (...) {}
            }
            s += "\n";
            y.ornek += s;
        } catch (...) {}
    }
    return r;
}

static PVOID SayacKancasi(int n)
{
    switch (n) {
        case 0: return (PVOID)HookSayac<0>;   case 1: return (PVOID)HookSayac<1>;
        case 2: return (PVOID)HookSayac<2>;   case 3: return (PVOID)HookSayac<3>;
        case 4: return (PVOID)HookSayac<4>;   case 5: return (PVOID)HookSayac<5>;
        case 6: return (PVOID)HookSayac<6>;   case 7: return (PVOID)HookSayac<7>;
    }
    return nullptr;
}

static void SCountCmd(const std::string& rest)
{
    std::string ad = rest;
    while (!ad.empty() && std::isspace((unsigned char)ad.back())) ad.pop_back();

    if (ad.empty() || Lower(ad) == "stat") {
        for (int i = 0; i < 8; i++) {
            if (!g_Yuva[i].ad) continue;
            Out(std::string("scount[") + std::to_string(i) + "] " + g_Yuva[i].ad
                + " -> " + std::to_string(g_Yuva[i].sayi) + " cagri");
            if (!g_Yuva[i].ornek.empty()) Out(g_Yuva[i].ornek);
        }
        return;
    }
    for (int i = 0; i < 8; i++) {
        if (g_Yuva[i].ad) continue;
        static char kimlik[8][16];
        sprintf_s(kimlik[i], "fp_sc%d", i);
        g_Yuva[i].ad = _strdup(ad.c_str());
        if (!HookOneScriptTable(ad.c_str(), kimlik[i], SayacKancasi(i), &g_Yuva[i].orij)) {
            g_Yuva[i].ad = nullptr;
            Out("scount: " + ad + " kancalanamadi");
        }
        return;
    }
    Out("scount: bos yuva kalmadi (8/8)");
}
#endif

#ifndef FORGEPACT_RELEASE
// ---- Bolge uretim izi -----------------------------------------------------
// ZoneGenChaosTower'i KIMIN cagirdigi statik olarak bulunamadi: dogrudan
// cagri, betik tanimlayicisi, degisken slotu ve isim metni - dordu de sifir
// dondu (arac bilinen bir dogru cevapla test edildi).
//
// O yuzden soruyu tersten soruyoruz: bir bolge uretilirken hangi ZoneGen
// adimlari SIRAYLA calisiyor?  Chaos Tower listede hic yoksa karar daha
// yukarida veriliyor; varsa hangi adimda elendigi gorunur.
static int g_ZgLog = 0;
static volatile long g_ZgSira = 0;

static void ZgYaz(const char* ad, int argc, RValue** A, void* donus = nullptr)
{
    if (g_ZgLog <= 0) return;
    try {
        long n = InterlockedIncrement(&g_ZgSira);
        std::ofstream f(IPC_DIR + "\\zonegen.txt", std::ios::app);
        f << "[" << n << "] " << ad << " argc=" << argc;
        // Cagiranin donus adresi.  Statik arama bu betiklerin cagiranini
        // bulamadi (dogrudan cagri, tanimlayici, slot, isim - dordu de sifir),
        // ama RandomChaosTower'da ayni teknik surucuyu vermisti.
        if (donus && g_Base)
            f << "  cagiran_rva=0x" << std::hex
              << (unsigned long long)((uintptr_t)donus - g_Base) << std::dec;
        for (int i = 0; i < argc && i < 3; i++)
            f << "  arg" << i << "=" << (A && A[i] ? Describe(*A[i]) : std::string("(null)"));
        f << "\n";
        f.flush();
    } catch (...) {}
}

#define ZG_HOOK(NAME) \
    static PFUNC_YYGMLScript g_OrigZg_##NAME = nullptr; \
    static RValue& HookZg_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        ZgYaz(#NAME, argc, A, _ReturnAddress()); \
        return g_OrigZg_##NAME ? g_OrigZg_##NAME(S, O, R, argc, A) : R; \
    }

ZG_HOOK(ZoneGenGenerateKeyPresets)
ZG_HOOK(ZoneGenPlacePreset)
ZG_HOOK(ZoneGenFindNextPreset)
ZG_HOOK(ZoneGenPopulateGroundPresets)
ZG_HOOK(ZoneGenPopulatePresetObjects)
ZG_HOOK(PushPresetToGrid)
ZG_HOOK(ZoneGenMakeZoneWalls)
ZG_HOOK(ZoneGenRestart)

static void ZoneGenLogKur()
{
    struct K { const char* ad; const char* kimlik; PVOID k; PFUNC_YYGMLScript* o; };
    static const K kTablo[] = {
        { "ZoneGenGenerateKeyPresets",   "fp_zg1", (PVOID)HookZg_ZoneGenGenerateKeyPresets,   &g_OrigZg_ZoneGenGenerateKeyPresets },
        { "ZoneGenPlacePreset",          "fp_zg2", (PVOID)HookZg_ZoneGenPlacePreset,          &g_OrigZg_ZoneGenPlacePreset },
        { "ZoneGenFindNextPreset",       "fp_zg3", (PVOID)HookZg_ZoneGenFindNextPreset,       &g_OrigZg_ZoneGenFindNextPreset },
        { "ZoneGenPopulateGroundPresets","fp_zg4", (PVOID)HookZg_ZoneGenPopulateGroundPresets,&g_OrigZg_ZoneGenPopulateGroundPresets },
        { "ZoneGenPopulatePresetObjects","fp_zg5", (PVOID)HookZg_ZoneGenPopulatePresetObjects,&g_OrigZg_ZoneGenPopulatePresetObjects },
        { "PushPresetToGrid",            "fp_zg6", (PVOID)HookZg_PushPresetToGrid,            &g_OrigZg_PushPresetToGrid },
        { "ZoneGenMakeZoneWalls",        "fp_zg7", (PVOID)HookZg_ZoneGenMakeZoneWalls,        &g_OrigZg_ZoneGenMakeZoneWalls },
        { "ZoneGenRestart",              "fp_zg8", (PVOID)HookZg_ZoneGenRestart,              &g_OrigZg_ZoneGenRestart },
    };
    int kurulan = 0;
    for (const auto& k : kTablo) {
        if (!*k.o) HookOneScriptTable(k.ad, k.kimlik, k.k, k.o);
        if (*k.o) kurulan++;
    }
    Out("zonegenlog: " + std::to_string(kurulan) + "/8 kanca kurulu -> bp_ipc\\zonegen.txt");
}
#endif

static void InstallChaosTowerHooks()
{
    HookOneScript("RandomChaosTower", "bp_randomct", (PVOID)HookRandomCT, &g_OrigRandomCT);
    HookOneScript("ZoneGenChaosTower", "bp_zonegenct", (PVOID)HookZoneGenCT, &g_OrigZoneGenCT);
}

static void ChaosTowerStats()
{
    char b[256];
    sprintf_s(b, "ctstats: RandomChaosTower calls=%ld | ZoneGenChaosTower calls=%ld | force=%s",
        g_ctRandomCalls, g_ctZoneGenCalls, std::isnan(g_ctForce) ? "off" : std::to_string(g_ctForce).c_str());
    Out(b);
    Out("  RandomChaosTower returns seen: " + (g_ctReturnLog.empty() ? std::string("(none)") : g_ctReturnLog));
    Out("  RandomChaosTower array: " + (g_ctArrayDump.empty() ? std::string("(none)") : g_ctArrayDump));
    Out("  RandomChaosTower caller: " + (g_ctCaller.empty() ? std::string("(none)") : g_ctCaller));
}

// Hook on GetSlotBloodPact(slot) -> pact id. Logs natural return; can force non-zero.
static RValue& HookGetSlotBloodPact(CInstance* Self, CInstance* Other, RValue& Result, int argc, RValue** Args)
{
    InterlockedIncrement(&g_SlotCalls);
    RValue& r = g_OrigGetSlot ? g_OrigGetSlot(Self, Other, Result, argc, Args) : Result;
    double orig = 0.0;
    try { orig = r.ToDouble(); } catch (...) {}
    if (g_SlotLog.size() < 900) {
        std::string a = (argc >= 1 && Args && Args[0]) ? Args[0]->ToString() : "?";
        std::string entry = "(" + a + "->" + std::to_string((long long)orig) + ")";
        if (g_SlotLog.find(entry) == std::string::npos) g_SlotLog += entry;
    }
    if (!std::isnan(g_ForceSlot)) {
        Result = RValue(g_ForceSlot);
        return Result;
    }
    return r;
}

static void InstallSlotHook()
{
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer("gml_Script_GetSlotBloodPact", &p);
    if (!AurieSuccess(st) || !p) { Out("InstallSlotHook: not found st=" + std::to_string((int)st)); return; }
    CScript* sc = reinterpret_cast<CScript*>(p);
    PVOID src = nullptr;
    try { src = (PVOID)sc->m_Functions->m_ScriptFunction; } catch (...) {}
    if (!src) { Out("InstallSlotHook: null src"); return; }
    PVOID tramp = nullptr;
    AurieStatus hs = MmCreateHook(g_ArSelfModule, "bp_getslot", src, (PVOID)HookGetSlotBloodPact, &tramp);
    if (!AurieSuccess(hs)) { Out("InstallSlotHook: MmCreateHook failed st=" + std::to_string((int)hs)); return; }
    g_OrigGetSlot = reinterpret_cast<PFUNC_YYGMLScript>(tramp);
    Out("HOOK INSTALLED on GetSlotBloodPact");
}

// ===== IsLoggedIn hook (force the online-login gate TRUE) =====
static PFUNC_YYGMLScript g_OrigIsLoggedIn = nullptr;
static bool g_ForceLogin = false;
static volatile long g_LoginCalls = 0;
static RValue& HookIsLoggedIn(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    InterlockedIncrement(&g_LoginCalls);
    if (g_ForceLogin) { R = RValue(true); return R; }
    return g_OrigIsLoggedIn ? g_OrigIsLoggedIn(S, O, R, argc, A) : R;
}
static void InstallLoginHook()
{
    HookOneScript("IsLoggedIn", "bp_islogged", (PVOID)HookIsLoggedIn, &g_OrigIsLoggedIn);
}

// ===== DIAGNOSTIC: log how the game deals damage to enemies (learn the signature) =====
static PFUNC_YYGMLScript g_OrigHitReg = nullptr;
static volatile long g_HitRegCalls = 0;
static RValue& HookHitReg(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    long n = InterlockedIncrement(&g_HitRegCalls);
    if (n <= 10) {
        std::string line = "HitReg#" + std::to_string(n) + " argc=" + std::to_string(argc) + " args:";
        for (int i = 0; i < argc && i < 10; i++)
            line += " [" + std::to_string(i) + "]=" + ((A && A[i]) ? Describe(*A[i]) : "?");
        std::ofstream f(IPC_DIR + "\\hitreg.txt", std::ios::app);
        f << line << "\n";
    }
    return g_OrigHitReg ? g_OrigHitReg(S, O, R, argc, A) : R;
}
static void InstallHitRegHook()
{
    HookOneScript("EnemyHitRegDamageParent", "bp_hitreg", (PVOID)HookHitReg, &g_OrigHitReg);
}

// ===== DIAGNOSTIC: observe how a buff is applied to the player (learn the signature) =====
static PFUNC_YYGMLScript g_OrigBuffAdd = nullptr, g_OrigCABuffAdd = nullptr;
static volatile long g_BuffAddCalls = 0;
static void LogBuffCall(const char* tag, CInstance* S, int argc, RValue** A)
{
    long n = InterlockedIncrement(&g_BuffAddCalls);
    if (n > 8) return;
    std::string line = std::string(tag) + "#" + std::to_string(n) + " argc=" + std::to_string(argc) + " args:";
    for (int i = 0; i < argc && i < 12; i++) {
        std::string v = "?";
        if (A && A[i]) {
            if (A[i]->m_Kind == VALUE_ARRAY || A[i]->m_Kind == VALUE_OBJECT) {
                try { v = "json:" + g_Yytk->CallBuiltin("json_stringify", { *A[i] }).ToString(); } catch (...) { v = Describe(*A[i]); }
            } else v = Describe(*A[i]);
        }
        line += " [" + std::to_string(i) + "]=" + v;
    }
    std::ofstream f(IPC_DIR + "\\buffadd.txt", std::ios::app);
    f << line << "\n";
}
static RValue& HookBuffAdd(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    LogBuffCall("BuffAdd", S, argc, A);
    return g_OrigBuffAdd ? g_OrigBuffAdd(S, O, R, argc, A) : R;
}
static RValue& HookCABuffAdd(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    LogBuffCall("CA_playerBuffAdd", S, argc, A);
    return g_OrigCABuffAdd ? g_OrigCABuffAdd(S, O, R, argc, A) : R;
}
static void InstallBuffHooks()
{
    HookOneScript("BuffAdd", "bp_buffadd", (PVOID)HookBuffAdd, &g_OrigBuffAdd);
    HookOneScript("CA_playerBuffAdd", "bp_cabuffadd", (PVOID)HookCABuffAdd, &g_OrigCABuffAdd);
}

// ===== IsMyPlayer hook: make the game treat our co-op puppet as NOT-my-player =====
// so the local keyboard/mouse input is NOT applied to it (it's network-driven).
static double g_PuppetId = -1.0;  // co-op puppet instance id (IsMyPlayer hook + render)
// puppet/render config globals (declared early so coop.ini auto-start can set them)
static std::atomic<bool> g_CoopRender{ false };
static std::string g_PuppetObjName = "Player_obj";
static int g_PuppetObjIdx = -1;
static PFUNC_YYGMLScript g_OrigIsMyPlayer = nullptr;
static bool g_HookPuppetInput = false;  // DEFAULT OFF (caused a hang); toggle via puppetinput cmd
static RValue& HookIsMyPlayer(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_HookPuppetInput && argc >= 1 && A && A[0]) {
        try {
            RValue inst = *A[0];   // the instance ref being queried
            RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { inst, RValue("coop_puppet") });
            if (ex.ToBoolean()) {
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("coop_puppet") });
                if (v.ToDouble() == 1.0) { R = RValue(false); return R; }  // puppet: NOT my player
            }
        } catch (...) {}
    }
    return g_OrigIsMyPlayer ? g_OrigIsMyPlayer(S, O, R, argc, A) : R;
}
static void InstallIsMyPlayerHook()
{
    HookOneScript("IsMyPlayer", "bp_ismyplayer", (PVOID)HookIsMyPlayer, &g_OrigIsMyPlayer);
}

static void LoadConfig()
{
    g_Config.clear();
    std::string path = IPC_DIR + "\\config.json";
    std::ifstream f(path, std::ios::binary);
    if (!f) { Out("LoadConfig: cannot open " + path); return; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();
    try {
        RValue parsed = g_Yytk->CallBuiltin("json_parse", { RValue(content) });
        g_Yytk->EnumInstanceMembers(parsed,
            [&](const char* name, RValue* val) -> bool {
                if (name && val) {
                    try { g_Config[name] = val->ToDouble(); } catch (...) {}
                }
                return false;
            });
        Out("LoadConfig: " + std::to_string(g_Config.size()) + " modifiers loaded");
    } catch (...) { Out("LoadConfig: json_parse EXCEPTION"); }
}

// Bazi ayarlar cmd.txt ile GEC kaliyor.  Ornek: RandomChaosTower yalnizca
// Controller_obj'in Create olayinda, yani oyun acilirken BIR KEZ cagriliyor
// ve kulenin cikabilecegi 10 bolgeyi orada seciyor.  Panel komutu ilk
// cerceveden sonra islendigi icin o secime yetisemiyor.
//
// startup.txt eklentinin YUKLENDIGI anda okunur - hicbir oyun kodu daha
// calismamistir.  Yalnizca oyun cagrisi gerektirmeyen, duz degisken atayan
// komutlar burada gecerli.
static void LoadStartup()
{
    std::ifstream f(IPC_DIR + "\\startup.txt", std::ios::binary);
    if (!f) return;
    std::string satir;
    int uygulanan = 0;
    while (std::getline(f, satir)) {
        while (!satir.empty() && (satir.back() == '\r' || satir.back() == '\n' || satir.back() == ' '))
            satir.pop_back();
        if (satir.empty() || satir[0] == '#') continue;
        std::string kalan;
        std::string komut = Lower(FirstToken(satir, kalan));
        if (komut == "ctsize") {
            try { g_ctArrayN = std::stoi(kalan); uygulanan++; } catch (...) {}
        } else if (komut == "ctarray") {
            g_ctCustom.clear();
            std::stringstream ss(kalan);
            std::string tek;
            while (std::getline(ss, tek, ',')) {
                try { g_ctCustom.push_back(std::stod(tek)); } catch (...) {}
            }
            if (!g_ctCustom.empty()) uygulanan++;
        }
    }
    if (uygulanan) Out("LoadStartup: " + std::to_string(uygulanan) + " erken ayar uygulandi");
}

static void InstallHook()
{
    if (g_HookInstalled) {
#ifndef FORGEPACT_RELEASE
        InstallCreateHooks();
        InstallNecroBalanceHooks();
#endif
        Out("hook already installed");
        return;
    }
    g_Base = (uintptr_t)GetModuleHandleA(nullptr);

    // Load the editor-authored sidecar before choosing the release hook set.
    // This remains inert when the user has not forged any custom items.
    // Runs in every build - a release build with no forged items just finds
    // nothing to load, but a release build that skipped this outright would
    // silently drop custom-item stats/names/tooltips and Headhunter/Tyrant's
    // Crown/Beacon auto-arm for players who used the Item Editor.
    LoadCustomForgeEntries();
    InstallCustomForgeItemHooks();
    HeadhunterAutoArm();
    TyrantAutoArm();
    BeaconAutoArm();
    if (g_SigDropPct > 0.0 || g_AngelicDropOneIn > 0.0) InstallHeadhunterHook();   // kill hook carries the signature and angelic drops

#ifdef FORGEPACT_RELEASE
    // Yayin derlemesi: arastirma kancasi ve teshis gunlugu yok.
    // Functional hooks are installed on-demand when commands arrive.
    g_HookInstalled = true;
    Out("BloodPact: yayin modu (density + ozel icerik + minimap)");
    return;
#else
    // Development builds install the complete research surface eagerly.
    InstallCreateHooks();
    InstallDropMultHooks();
    InstallNecroBalanceHooks();

    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer("gml_Script_GetBloodPactInfo", &p);
    if (!AurieSuccess(st) || !p) { Out("InstallHook: cannot find GetBloodPactInfo st=" + std::to_string((int)st)); return; }
    CScript* sc = reinterpret_cast<CScript*>(p);
    PVOID src = nullptr;
    try { src = (PVOID)sc->m_Functions->m_ScriptFunction; } catch (...) {}
    if (!src) { Out("InstallHook: null source fn"); return; }

    PVOID tramp = nullptr;
    AurieStatus hs = MmCreateHook(g_ArSelfModule, "bp_getinfo", src, (PVOID)HookGetBloodPactInfo, &tramp);
    if (!AurieSuccess(hs)) { Out("InstallHook: MmCreateHook failed st=" + std::to_string((int)hs)); return; }
    g_OrigGetInfo = reinterpret_cast<PFUNC_YYGMLScript>(tramp);
    g_HookInstalled = true;
    char buf[160];
    sprintf_s(buf, "HOOK INSTALLED on GetBloodPactInfo src=%p tramp=%p", src, tramp);
    Out(buf);
    InstallSlotHook();
    InstallLoginHook();
    InstallIsMyPlayerHook();
    InstallHitRegHook();
    InstallBuffHooks();
    InstallEnemyHooks();
    InstallChaosTowerHooks();
    InstallItemInspectHooks();
#endif
}

static void HookStats()
{
    char buf[320];
    sprintf_s(buf, "hookstats: installed=%d cfg=%zu getInfoCalls=%ld overrides=%ld slotCalls=%ld forceSlot=%s",
        (int)g_HookInstalled, g_Config.size(), g_HookCalls, g_HookOverrides, g_SlotCalls,
        std::isnan(g_ForceSlot) ? "off" : std::to_string(g_ForceSlot).c_str());
    Out(buf);
    Out("  getInfo keys: " + (g_LastKeys.empty() ? std::string("(none)") : g_LastKeys));
    Out("  getInfo callers(rva): " + (g_CallerLog.empty() ? std::string("(none)") : g_CallerLog));
    Out("  getSlot seen: " + (g_SlotLog.empty() ? std::string("(none)") : g_SlotLog));
}

// Returns the player instance id RValue, or a real <0 if not found.
static RValue GetPlayerId()
{
    RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
    return g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
}

// Read an instance variable from the player.
static void PlayerVarGet(const std::string& var)
{
    try {
        RValue pid = GetPlayerId();
        if (pid.ToDouble() < 0) { Out("iget: no player"); return; }
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { pid, RValue(var) });
        if (!ex.ToBoolean()) { Out("iget '" + var + "' -> (player has no such var)"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue(var) });
        Out("iget '" + var + "' -> " + Describe(v));
    } catch (...) { Out("iget EXCEPTION"); }
}

// Write an instance variable on the player.
static void PlayerVarSet(const std::string& var, double val)
{
    try {
        RValue pid = GetPlayerId();
        if (pid.ToDouble() < 0) { Out("iset: no player"); return; }
        g_Yytk->CallBuiltin("variable_instance_set", { pid, RValue(var), RValue(val) });
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue(var) });
        Out("iset '" + var + "' = " + std::to_string(val) + " -> now " + Describe(v));
    } catch (...) { Out("iset EXCEPTION"); }
}

// Auto map-reveal moved to ForgePact::MapRevealManager (see the module
// includes anchor after HhResolveLocalPlayer, above). CompSetBuffs and
// RunCommand's "reveal" handler below call it directly.

// naddr <ScriptName> -- print a gml script's native function address + containing module base + RVA,
// so we can decompile JUST that function in Ghidra (no full-exe analysis needed).
static void NAddr(const std::string& name)
{
    std::string full = "gml_Script_" + name;
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(full.c_str(), &p);
    if (!AurieSuccess(st) || !p) { Out("naddr " + name + ": not found st=" + std::to_string((int)st)); return; }
    CScript* sc = reinterpret_cast<CScript*>(p);
    PVOID src = nullptr;
    try { src = (PVOID)sc->m_Functions->m_ScriptFunction; } catch (...) {}
    if (!src) { Out("naddr " + name + ": null func"); return; }
    HMODULE mod = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)src, &mod);
    char modname[MAX_PATH] = { 0 };
    if (mod) GetModuleFileNameA(mod, modname, MAX_PATH);
    char buf[600];
    sprintf_s(buf, "naddr %s: func=%p modbase=%p rva=0x%llX mod=%s",
        name.c_str(), src, (void*)mod,
        (unsigned long long)((char*)src - (char*)mod), modname);
    Out(buf);
}

// naddrall -- naddr, in bulk, for every script this plugin ever hooks by name
// (kept in sync by hand with the HookOneScript("<Name>", ...) call sites and
// the StatsManager multiplier table; NOT the full ~6000-script data.win list).
// Answers, without a disassembler and without querying one name at a time,
// questions like "do these two script names compile to the same native
// address" or "what is this hook's real module+RVA right now" - e.g. for
// checking a claim that some compiled caller reaches a hook target by a
// route other than this script's own dispatch entry. Writes one CSV line per
// name to bp_ipc\script_addresses.csv; a blank address means the name wasn't
// found (removed/renamed script) rather than an error worth stopping for.
static void NAddrAll()
{
    static const char* kNames[] = {
        // HookOneScript("<Name>", ...) literal call sites across ModuleMain.cpp
        // and the ForgePact:: headers (2026-09 review follow-up).
        "LoadDrops", "CreateItemNew", "CreateItemInit", "GenerateItemRandomStats",
        "GPV", "DropRelic", "DropItemAngelicChance", "draw_text_outline",
        "cpr_irandom", "ZoneStateResetSingle", "ZoneStateResetAll",
        "ZoneGenChaosTower", "TalentUse", "StatFasterCastRate", "SocketSetTarget",
        "SPV", "RunItemEquipped", "RandomChaosTower",
        "PopulateTalentStructMapNecromancer", "PathFindTakeTarget",
        "PathFindStartPath", "PathFindScanTick", "PathFindLeashCheck",
        "PathFindAggroBroadcast", "LootGroundCreateFromItem", "LootGroundCreate",
        "LocalActivateDeactivateProps", "LoadSummonStats", "LoadSatanicDropTier",
        "LoadCommonItems", "ItemEquip", "IsObtainablePlace", "IsMyPlayer",
        "IsLoggedIn", "GetRuneword", "GetItemTooltipString", "GetItemStatString",
        "GenerateItemSpecialStats", "EquipItemUnequip", "EnemyRaritySettings",
        "EnemyHitRegDamageParent", "EnemyDestroyKillProc", "EnemyDestroyDeathEffects",
        "DropUberParts", "DropRubyKey", "DropOres", "DropOreMaterials",
        "DropMonsterGold", "DropKeys", "DropItemBoss", "DropItemAngelic",
        "DropItem", "DropGold", "DropDungeonKeys", "DropDimensionalShard",
        "DropChaosKey", "DropBossRunes", "DropBossParts", "DropBossGems",
        "DropBifrostKey", "DropBattleFragments", "DropAngelicKey",
        "DropAngelicCharm", "DrawTooltip", "DrawInventoryStatsNew", "DrawHudBuffs",
        "DebugLogAddExt", "CreateItemDrop", "CreateEnemyFreePos", "CreateEnemyElite",
        "CombatText", "CA_playerBuffAdd", "CA_enemyCreate", "BuffAdd",
        "ActivateDeactivateProps",
        // ForgePact::StatsManager's multiplier table (14-entry).
        "StatMagicFind", "StatAttackSpeed", "StatExperienceGain",
        "StatMovementSpeed", "CalculateEndDamage", "StatExtraGold",
        "StatLifeReplenish", "StatManaReplenish", "StatDefense", "StatCritDamage",
        "StatCritRate", "StatSpellCritDamage", "StatSpellCritRate",
        "EnemyCalculateExperience",
    };

    std::ofstream f(IPC_DIR + "\\script_addresses.csv", std::ios::trunc);
    if (!f) { Out("naddrall: cannot open bp_ipc\\script_addresses.csv"); return; }
    f << "name,func_ptr,module,module_base,rva\n";

    int found = 0, missing = 0;
    for (const char* name : kNames) {
        std::string full = std::string("gml_Script_") + name;
        PVOID p = nullptr;
        AurieStatus st = g_Yytk->GetNamedRoutinePointer(full.c_str(), &p);
        PVOID src = nullptr;
        if (AurieSuccess(st) && p) {
            CScript* sc = reinterpret_cast<CScript*>(p);
            try { src = (PVOID)sc->m_Functions->m_ScriptFunction; } catch (...) {}
        }
        if (!src) { f << name << ",,,,\n"; missing++; continue; }
        HMODULE mod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)src, &mod);
        char modname[MAX_PATH] = { 0 };
        if (mod) GetModuleFileNameA(mod, modname, MAX_PATH);
        char line[700];
        sprintf_s(line, "%s,%p,%s,%p,0x%llX\n",
            name, src, modname, (void*)mod,
            (unsigned long long)((char*)src - (char*)mod));
        f << line;
        found++;
    }
    f.close();
    char summary[128];
    sprintf_s(summary, "naddrall: %d found, %d missing -> bp_ipc\\script_addresses.csv", found, missing);
    Out(summary);
}

// naddrorig <ScriptName> -- naddr/naddrall report the CURRENT native function
// pointer for a script, which for anything this plugin hooks unconditionally
// at startup (Headhunter's kill hook among them, since HeadhunterAutoArm()
// runs from InstallHook() every launch) is already this plugin's OWN hook
// function, not the game's original compiled code - by the time a command can
// even be sent, the swap has already happened. This reports the TRUE vanilla
// address instead, from the g_Orig_* pointer HookOneScript captured before
// installing that hook. Only knows the names below; add an entry to check
// another always-armed hook the same way.
static void NAddrOrig(const std::string& name)
{
    struct Entry { const char* name; PFUNC_YYGMLScript ptr; };
    const Entry table[] = {
        { "EnemyDestroyKillProc", g_Orig_EnemyDestroyKillProc },
        { "DropRelic",            g_Orig_DropRelic },
    };
    for (const auto& e : table) {
        if (name != e.name) continue;
        PVOID src = (PVOID)e.ptr;
        if (!src) { Out("naddrorig " + name + ": not captured yet (hook not installed)"); return; }
        HMODULE mod = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)src, &mod);
        char modname[MAX_PATH] = { 0 };
        if (mod) GetModuleFileNameA(mod, modname, MAX_PATH);
        char buf[600];
        sprintf_s(buf, "naddrorig %s: orig_func=%p modbase=%p rva=0x%llX mod=%s",
            name.c_str(), src, (void*)mod,
            (unsigned long long)((char*)src - (char*)mod), modname);
        Out(buf);
        return;
    }
    Out("naddrorig " + name + ": no known orig pointer registered for this name");
}

// Spawn an item: json_parse the file -> InitItemFromJson -> LootGroundCreateFromItem at player.
static void SpawnItem(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { Out("spawnitem: cannot open " + path); return; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();
    try {
        // InitItemFromJson likely takes the raw JSON string (parses internally)
        RValue item = g_Yytk->CallGameScript("gml_Script_InitItemFromJson", { RValue(content) });
        Out("spawnitem: InitItemFromJson(string) -> " + Describe(item));
        // player position
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue pid = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        if (pid.ToDouble() < 0) { Out("spawnitem: no player for drop"); return; }
        RValue px = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("x") });
        RValue py = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("y") });
        RValue res = g_Yytk->CallGameScript("gml_Script_LootGroundCreateFromItem", { item, px, py });
        Out("spawnitem: LootGroundCreateFromItem(item, x, y) -> " + Describe(res));
    } catch (...) { Out("spawnitem EXCEPTION"); }
}

// gjson <globalName> -- json_stringify a global (array/struct/anything) into bp_ipc\gjson.json
static void GJson(const std::string& name)
{
    try {
        RValue ex = g_Yytk->CallBuiltin("variable_global_exists", { RValue(name) });
        if (!ex.ToBoolean()) { Out("gjson '" + name + "' -> (does not exist)"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_global_get", { RValue(name) });
        RValue s = g_Yytk->CallBuiltin("json_stringify", { v });
        std::string js = s.ToString();
        std::string path = IPC_DIR + "\\gjson.json";
        std::ofstream f(path, std::ios::binary); f << js;
        Out("gjson '" + name + "' (" + Describe(v) + ") -> " + std::to_string(js.size()) + " bytes -> gjson.json");
    } catch (...) { Out("gjson EXCEPTION on " + name); }
}

#ifndef FORGEPACT_RELEASE
// ---- Blood Pact degerlerini avlamak icin arama komutlari (yalnizca dev derleme) ----
//
// Bulunan gercek: blood_pact_* isimlerinin HICBIRI degisken degil; hepsi
// ceviri anahtari (etiket metni).  Yani modifiye degerleri baska adlarla,
// buyuk ihtimalle bir struct icinde duruyor.  Asagidakiler o yuzeyi tek
// oturumda dokmek icin.

// GameMaker'da global kapsam -5 numarali sahte ornek.  variable_instance_get_names
// onu kabul ediyor, boylece TUM global degisken adlarini alabiliyoruz.
static void GlobalNames(const std::string& filter)
{
    try {
        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { RValue(-5.0) });
        RValue cnt = g_Yytk->CallBuiltin("array_length", { names });
        int n = (int)cnt.ToDouble();
        std::string flt = Lower(filter);
        std::string line; int shown = 0;
        for (int i = 0; i < n; i++) {
            RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            std::string s = nm.ToString();
            if (!flt.empty() && Lower(s).find(flt) == std::string::npos) continue;
            line += s + " ";
            if (++shown % 8 == 0) { Out("  " + line); line.clear(); }
        }
        if (!line.empty()) Out("  " + line);
        Out("gnames: " + std::to_string(shown) + " / " + std::to_string(n) + " global"
            + (flt.empty() ? "" : (", filtre '" + filter + "'")));
    } catch (...) { Out("gnames EXCEPTION"); }
}

// Oyuncunun bir ornek degiskenini json'a dokup dosyaya yazar.  Stat struct'lari
// icin: once inames ile adi bul, sonra ijson ile icini gor.
static void PlayerVarJson(const std::string& var)
{
    try {
        RValue pid = GetPlayerId();
        if (pid.ToDouble() < 0) { Out("ijson: oyuncu yok"); return; }
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { pid, RValue(var) });
        if (!ex.ToBoolean()) { Out("ijson '" + var + "' -> oyuncuda boyle bir degisken yok"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue(var) });
        RValue s = g_Yytk->CallBuiltin("json_stringify", { v });
        std::string js = s.ToString();
        std::string path = IPC_DIR + "\\ijson_" + var + ".json";
        std::ofstream f(path, std::ios::binary); f << js;
        Out("ijson '" + var + "' (" + Describe(v) + ") -> " + std::to_string(js.size())
            + " bayt -> ijson_" + var + ".json");
    } catch (...) { Out("ijson EXCEPTION on " + var); }
}

// Bir nesnenin ornek degiskenini json'a doker (oyuncu disindaki nesneler icin).
static void ObjVarJson(const std::string& objName, const std::string& var)
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) { Out("ojson: " + objName + " ornegi yok"); return; }
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue(var) });
        if (!ex.ToBoolean()) { Out("ojson '" + var + "' -> " + objName + " icinde yok"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(var) });
        RValue s = g_Yytk->CallBuiltin("json_stringify", { v });
        std::string js = s.ToString();
        std::string path = IPC_DIR + "\\ojson_" + objName + "_" + var + ".json";
        std::ofstream f(path, std::ios::binary); f << js;
        Out("ojson " + objName + "." + var + " (" + Describe(v) + ") -> "
            + std::to_string(js.size()) + " bayt -> ojson_" + objName + "_" + var + ".json");
    } catch (...) { Out("ojson EXCEPTION"); }
}
#endif

// stat <ad> <carpan> | stat list  -- oyuncu istatistigi carpanlari
// ---- Rare drop controls: Heroic / loot ceiling / Satanic tier --------------
// All three intercept values the game READS.  Nothing is written into game
// state, so putting a slider back to x1 restores vanilla exactly - there is no
// "restore" step to get wrong.
//
// Static analysis 2026-08-28, full write-up in
// native_s10/DROP_RATE_RESEARCH.md:
//
//   Every rare-drop ladder rolls  irandom( GPV(gDataProtected[175]) )  and
//   compares the result with a threshold, so each threshold is a literal
//   percent:
//       gDataProtected[175] = 99.0   roll ceiling, shared by EVERY ladder
//       gDataProtected[177] = 28.0   base Heroic chance
//       gDataProtected[178] = 37.0   boosted Heroic chance
//   Slots 177/178 are read by nothing except DropItem, so they are clean
//   levers.  The stored values are runtime handles minted at startup, not
//   constants in the exe, so we resolve them once from global.gDataProtected
//   and then override what GPV RETURNS for those keys.
//
//   Satanic has no such slot: it is decided by LoadSatanicDropTier(monsterLevel,
//   multiplier), which brackets on monster level (200/123/100/75/42/24/15) and
//   rolls cumulative cut-offs inside the bracket.  Its `multiplier` argument
//   only widens ONE threshold in the level>=200 bracket, so scaling it is
//   useless below level 200.  Instead we raise the monster level it is given,
//   capped at 200 - the game's own top bracket.  Every number it then uses is
//   vanilla; we only choose which vanilla row applies.
static PFUNC_YYGMLScript g_OrigGpvRate = nullptr;
static PFUNC_YYGMLScript g_OrigSatTier = nullptr;

static double g_HeroicMult  = 1.0;   // 1 = vanilla (28% base / 37% boosted)
static double g_CeilingMult = 1.0;   // 1 = vanilla (roll 0..99)
static double g_SatanicMult = 1.0;   // 1 = vanilla monster level

// Angelic is different from the other three.  Its gate is not a value the game
// reads - DropItem asks "does the player have buff 332?" and, when the answer is
// no, never runs the angelic code at all.  gml_Script_GetBuff is inlined at
// every use site (0 real calls in the whole exe), so there is nothing to hook:
// the only way in is to neutralise the branch itself.
//
// The chance the game then rolls against is NOT a constant - DropItem builds it
// up at runtime with floor() and additions, so opening the gate gives the game's
// own computed rate rather than a flood.  We log that value so the real number
// can be read instead of guessed.
//
// slider: x1 = off, x2 = gate open at the game's own rate,
//         x3..x5 = gate open and the internal roll ceiling divided by (mult-1).
static double g_AngelicMult     = 1.0;   // what the panel asked for
static double g_AngelicRateMult = 1.0;   // rolls per kill once the gate is open: 1 = the game's own single roll
static bool g_InAngelicExtra = false;

static volatile long g_RateCeilHits = 0;
static volatile long g_RateHeroHits = 0;
static volatile long g_SatTierHits  = 0;

static const int kSlotAngelic     = 172;   // ceiling of the roll inside DropItemAngelicChance
static const int kSlotCeiling     = 175;
static const int kSlotHeroic      = 177;
static const int kSlotHeroicBoost = 178;

static const double kHeroicBase  = 28.0;
static const double kHeroicBoost = 37.0;
static const double kSatanicTopBracket = 200.0;

static volatile long g_AngRateHits = 0;
static volatile long g_AngChanceCalls = 0;
static double g_AngLastChance = -1.0;

static bool   g_RateKeysOk = false;
static double g_KeyCeiling = 0.0, g_KeyHeroic = 0.0, g_KeyHeroicBoost = 0.0;
static double g_KeyAngelic = 0.0;

// Resolve the three GPV keys out of global.gDataProtected.  Called only from
// the command handler (a safe frame context), never from inside a hook.
static bool ResolveRateKeys()
{
    if (g_RateKeysOk) return true;
    try {
        int len = -1;
        RValue arr = GlobalArray("gDataProtected", len);
        if (len <= kSlotHeroicBoost) {
            Out("raredrop: gDataProtected not ready yet (len=" + std::to_string(len)
                + ") - enter a map once, then apply again");
            return false;
        }
        RValue d = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)kSlotAngelic) });
        RValue a = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)kSlotCeiling) });
        RValue b = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)kSlotHeroic) });
        RValue c = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)kSlotHeroicBoost) });
        if (a.m_Kind != VALUE_REAL || b.m_Kind != VALUE_REAL || c.m_Kind != VALUE_REAL) {
            Out("raredrop: unexpected key types - controls stay off");
            return false;
        }
        double ka = a.ToDouble(), kb = b.ToDouble(), kc = c.ToDouble();
        // Three distinct slots must give three distinct keys.  If they collide,
        // the layout moved and overriding would hit the wrong value.
        if (ka == kb || ka == kc || kb == kc) {
            Out("raredrop: keys are not distinct - build layout changed, controls stay off");
            return false;
        }
        g_KeyCeiling = ka; g_KeyHeroic = kb; g_KeyHeroicBoost = kc;
        g_KeyAngelic = (d.m_Kind == VALUE_REAL) ? d.ToDouble() : 0.0;
        g_RateKeysOk = true;
        char msg[256];
        sprintf_s(msg, "raredrop: keys resolved (ceiling=%g heroic=%g boosted=%g)", ka, kb, kc);
        Out(msg);
    } catch (...) { Out("raredrop: key resolve EXCEPTION"); }
    return g_RateKeysOk;
}

// The protected-variable store is NATIVE: ac_dll_gm.dll (the anti-cheat GM
// extension) exports GetVariable / SetVariable / InitNewVariableFast.  Every
// inlined GPV copy in game code dispatches through the global `GetVariable`,
// which holds a reference to that export.  GML-level hooks (GPV, the PC_*
// wrappers) never fire - measured: reads seen = 0.  A MinHook on the export
// itself intercepts every read regardless of inlining.
// GM extension ABI: double __cdecl GetVariable(double key); key equals the
// gDataProtected index (measured live: gaget gDataProtected[177] -> 177).
typedef double (__cdecl *AcGetVariableFn)(double);
static AcGetVariableFn g_OrigProtGet = nullptr;
static volatile long g_ProtGetCalls = 0;
// Diagnostics: count reads of the three keys we care about no matter the
// multiplier, and optionally sample distinct (key, value) pairs so the real
// key space can be READ instead of guessed (ac_dll_gm also exports ScrambleKey,
// so the keys may not be the plain indices).
static volatile long g_Seen175 = 0, g_Seen177 = 0, g_Seen178 = 0;
static bool g_ProtProbe = false;
static std::map<double, double> g_ProbeSeen;
static std::mutex g_ProbeLock;

static double __cdecl HookProtGet(double key)
{
    double v = g_OrigProtGet ? g_OrigProtGet(key) : 0.0;
#ifndef FORGEPACT_RELEASE
    InterlockedIncrement(&g_ProtGetCalls);
    if (key == 175.0) InterlockedIncrement(&g_Seen175);
    else if (key == 177.0) InterlockedIncrement(&g_Seen177);
    else if (key == 178.0) InterlockedIncrement(&g_Seen178);
    if (g_ProtProbe) {
        try {
            std::lock_guard<std::mutex> lk(g_ProbeLock);
            if (g_ProbeSeen.size() < 400 && g_ProbeSeen.find(key) == g_ProbeSeen.end())
                g_ProbeSeen[key] = v;
        } catch (...) {}
    }
#endif
    if (!std::isfinite(v) || v <= 0.0) return v;   // sentinel / not set: hands off
    if (g_HeroicMult > 1.0 && (key == (double)kSlotHeroic || key == (double)kSlotHeroicBoost)) {
        double nv = v * g_HeroicMult;
        if (nv > 100.0) nv = 100.0;                // roll is irandom(99): 100 = always
        BP_DIAG_INCREMENT(g_RateHeroHits);
        return nv;
    }
    if (g_CeilingMult > 1.0 && key == (double)kSlotCeiling) {
        double nv = std::floor(v / g_CeilingMult); // smaller ceiling = every chance scaled up
        if (nv < 1.0) nv = 1.0;
        BP_DIAG_INCREMENT(g_RateCeilHits);
        return nv;
    }
    return v;
}

static bool EnsureProtGetHook()
{
    if (g_OrigProtGet) return true;
    HMODULE ac = GetModuleHandleA("ac_dll_gm.dll");
    if (!ac) { Out("raredrop: ac_dll_gm.dll is not loaded"); return false; }
    PVOID src = (PVOID)GetProcAddress(ac, "GetVariable");
    if (!src) { Out("raredrop: ac_dll_gm.dll!GetVariable export not found"); return false; }
    PVOID tramp = nullptr;
    AurieStatus hs = MmCreateHook(g_ArSelfModule, "fp_acgetvar", src, (PVOID)HookProtGet, &tramp);
    if (!AurieSuccess(hs)) {
        Out("raredrop: hook on ac_dll_gm!GetVariable failed st=" + std::to_string((int)hs));
        return false;
    }
    g_OrigProtGet = reinterpret_cast<AcGetVariableFn>(tramp);
    Out("HOOK INSTALLED on ac_dll_gm!GetVariable (native store)");
    return true;
}

static RValue& HookGpvRate(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    RValue& r = g_OrigGpvRate ? g_OrigGpvRate(S, O, R, argc, A) : R;
    if (!g_RateKeysOk || argc < 1 || !A || !A[0]) return r;
    if (A[0]->m_Kind != VALUE_REAL) return r;
    const double k = A[0]->ToDouble();
    try {
        if (k == g_KeyCeiling) {
            if (g_CeilingMult > 1.0) {
                // P(roll < T) = T/(ceiling+1), so dividing the ceiling the game
                // just returned multiplies every chance by the same factor -
                // whatever scale those thresholds happen to use.
                double v = r.ToDouble();
                if (std::isfinite(v) && v > 1.0) {
                    double n = std::floor(v / g_CeilingMult);
                    if (n < 1.0) n = 1.0;
                    r = RValue(n);
                    BP_DIAG_INCREMENT(g_RateCeilHits);
                }
            }
        } else if (k == g_KeyHeroic) {
            if (g_HeroicMult != 1.0) {
                double v = kHeroicBase * g_HeroicMult;
                if (v > 100.0) v = 100.0;
                r = RValue(v);
                BP_DIAG_INCREMENT(g_RateHeroHits);
            }
        } else if (k == g_KeyHeroicBoost) {
            if (g_HeroicMult != 1.0) {
                double v = kHeroicBoost * g_HeroicMult;
                if (v > 100.0) v = 100.0;
                r = RValue(v);
                BP_DIAG_INCREMENT(g_RateHeroHits);
            }
        }
    } catch (...) {}
    return r;
}

static RValue& HookSatanicTier(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_SatanicMult > 1.0 && argc >= 1 && A && A[0] && A[0]->m_Kind == VALUE_REAL) {
        try {
            double level = A[0]->ToDouble();
            double lifted = level * g_SatanicMult;
            if (lifted > kSatanicTopBracket) lifted = kSatanicTopBracket;
            if (lifted > level) {
                RValue liftedRV(lifted);
                RValue* args[8];
                int n = argc < 8 ? argc : 8;
                for (int i = 0; i < n; i++) args[i] = A[i];
                args[0] = &liftedRV;
                BP_DIAG_INCREMENT(g_SatTierHits);
                return g_OrigSatTier ? g_OrigSatTier(S, O, R, n, args) : R;
            }
        } catch (...) {}
    }
    return g_OrigSatTier ? g_OrigSatTier(S, O, R, argc, A) : R;
}

// Write over code bytes.  Used only for the angelic gate; everything else in
// this file works without touching code.
static bool WriteCodeBytes(void* addr, const unsigned char* bytes, size_t n)
{
    DWORD old = 0;
    if (!VirtualProtect(addr, n, PAGE_EXECUTE_READWRITE, &old)) return false;
    memcpy(addr, bytes, n);
    DWORD tmp = 0;
    VirtualProtect(addr, n, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), addr, n);
    return true;
}

static unsigned char* g_AngelicGate = nullptr;
static unsigned char  g_AngelicGateOrig[6] = {};
static bool           g_AngelicGatePatched = false;

static unsigned char* ScriptCode(const char* fullName)
{
    PVOID pv = nullptr;
    if (!AurieSuccess(g_Yytk->GetNamedRoutinePointer(fullName, &pv)) || !pv) return nullptr;
    try { return (unsigned char*)((CScript*)pv)->m_Functions->m_ScriptFunction; }
    catch (...) { return nullptr; }
}

// Locate the branch that skips the DropItemAngelicChance call.  Found by
// meaning, not by a fixed address, so it survives game updates:
//   1. find the real `call gml_Script_DropItemAngelicChance` inside DropItem
//   2. look back for a `test al,al` + `je rel32` whose target lands just after
//      that call
//   3. accept only if exactly ONE candidate matches
static unsigned char* FindAngelicGate()
{
    if (g_AngelicGate) return g_AngelicGate;
    unsigned char* drop   = ScriptCode("gml_Script_DropItem");
    unsigned char* chance = ScriptCode("gml_Script_DropItemAngelicChance");
    if (!drop || !chance) { Out("angelic: DropItem/DropItemAngelicChance not found"); return nullptr; }

    const size_t kScan = 0x30000;          // DropItem is about 0x24A50 bytes
    unsigned char* call = nullptr;
    for (size_t i = 0; i + 5 < kScan; i++) {
        if (drop[i] != 0xE8) continue;
        int rel = *reinterpret_cast<int*>(drop + i + 1);
        if (drop + i + 5 + rel == chance) { call = drop + i; break; }
    }
    if (!call) { Out("angelic: call site not found - game build changed"); return nullptr; }

    unsigned char* from = (call - drop) > 0x300 ? call - 0x300 : drop;
    unsigned char* found = nullptr;
    int hits = 0;
    for (unsigned char* q = from + 2; q + 6 <= call; q++) {
        if (q[0] != 0x0F || q[1] != 0x84) continue;              // je rel32
        if (q[-2] != 0x84 || q[-1] != 0xC0) continue;            // preceded by test al,al
        int rel = *reinterpret_cast<int*>(q + 2);
        unsigned char* dest = q + 6 + rel;
        if (dest >= call && dest <= call + 0x40) { found = q; hits++; }
    }
    if (hits != 1) {
        Out("angelic: gate not uniquely identified (" + std::to_string(hits)
            + " candidates) - nothing patched");
        return nullptr;
    }
    g_AngelicGate = found;
    char b[128];
    sprintf_s(b, "angelic: gate found at DropItem+0x%llX", (unsigned long long)(found - drop));
    Out(b);
    return found;
}

static bool OpenAngelicGate()
{
    if (g_AngelicGatePatched) return true;
    unsigned char* gate = FindAngelicGate();
    if (!gate) return false;
    memcpy(g_AngelicGateOrig, gate, sizeof(g_AngelicGateOrig));
    const unsigned char nops[6] = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    if (!WriteCodeBytes(gate, nops, sizeof(nops))) { Out("angelic: write failed"); return false; }
    g_AngelicGatePatched = true;
    Out("angelic: gate OPEN (the game now rolls for angelic drops)");
    return true;
}

static void CloseAngelicGate()
{
    if (!g_AngelicGatePatched || !g_AngelicGate) return;
    WriteCodeBytes(g_AngelicGate, g_AngelicGateOrig, sizeof(g_AngelicGateOrig));
    g_AngelicGatePatched = false;
    Out("angelic: gate closed, original bytes restored");
}

// Measurement: the chance DropItem computed is argument2.  Logged so the real
// number can be read on the first run instead of guessed.
static PFUNC_YYGMLScript g_OrigAngChance = nullptr;
static RValue& HookAngelicChance(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
#ifndef FORGEPACT_RELEASE
    long n = InterlockedIncrement(&g_AngChanceCalls);
    // First few calls: report argc and the RValue kinds only.  These are small
    // integers, so this line can never overflow the buffer - if the process dies
    // anyway, the fault is not in the formatting.
    if (n <= 5) {
        char k[160];
        sprintf_s(k, "angelic: enter #%ld argc=%d kind0=%d kind1=%d kind2=%d kind3=%d", n, argc,
                  (argc > 0 && A && A[0]) ? (int)A[0]->m_Kind : -1,
                  (argc > 1 && A && A[1]) ? (int)A[1]->m_Kind : -1,
                  (argc > 2 && A && A[2]) ? (int)A[2]->m_Kind : -1,
                  (argc > 3 && A && A[3]) ? (int)A[3]->m_Kind : -1);
        Out(k);
    }
    // Formatting a game-supplied double is not safe with %f: a huge or non-finite
    // value expands to hundreds of digits, overruns the buffer and makes sprintf_s
    // abort the whole process (0xC0000409).  Check the type, reject non-finite,
    // and use %g, whose output is bounded.
    if (argc >= 3 && A && A[2] && A[2]->m_Kind == VALUE_REAL) {
        try {
            double chance = A[2]->ToDouble();
            if (std::isfinite(chance)) {
                g_AngLastChance = chance;
                if (n <= 15 || (n % 100) == 0) {
                    char b[256];
                    std::string who;
                    try { if (S) { RValue inst = S->ToRValue(); who = TyInstName(inst) + " rarity " + std::to_string((int)HhReadNumber(inst, "enemyRarity", -1.0)); } } catch (...) { who = "?"; }
                    sprintf_s(b, "angelic: roll #%ld  chance=%g  by %s", n, chance, who.substr(0, 60).c_str());
                    Out(b);
                }
            } else if (n <= 5) {
                Out("angelic: roll with a non-finite chance value, not logged");
            }
        } catch (...) {}
    }
#endif
    // Multiplying the chance argument in place broke the game's own check (x99 -> zero
    // drops, 2026-09-05).  Since 1.3.13 the multiplier is a number of ROLLS: every extra roll
    // is the game's own function with the game's own chance, so x2 really is two 1-in-N dice.
    RValue& r = g_OrigAngChance ? g_OrigAngChance(S, O, R, argc, A) : R;
    const int extra = (int)std::lround(g_AngelicRateMult) - 1;   // rate x1 = the game's roll only
    if (!g_InAngelicExtra && extra > 0 && g_OrigAngChance) {
        g_InAngelicExtra = true;
        for (int i = 0; i < extra && i < 9; ++i) { RValue t; try { g_OrigAngChance(S, O, t, argc, A); } catch (...) {} }
        g_InAngelicExtra = false;
        InterlockedIncrement(&g_AngRateHits);
    }
    return r;
}

// raredrop heroic|ceiling|satanic|angelic <multiplier>   |   raredrop list
#ifndef FORGEPACT_RELEASE
// ---- socketprobe: capture the two socket rolls -----------------------------
// Socket count is stat 20 and it is GENERATED, never loaded: nothing written
// into the save survives, because CreateItemNew recomputes it from the item
// seed.  Static analysis put the two socket rolls at chain slots 2 and 3 of the
// cpr_* calls inside CreateItemNew.  Both cpr_irandom and CreateItemNew have
// real call sites, so this hooks BY NAME and needs no addresses - it therefore
// survives game updates.
static HMODULE g_GameBase = GetModuleHandleW(nullptr);
static PFUNC_YYGMLScript g_OrigCreateItemNew = nullptr;
static PFUNC_YYGMLScript g_OrigCprIrandom = nullptr;
static PFUNC_YYGMLScript g_OrigLoadCommonItems = nullptr;
static bool g_SockProbe = false;
static thread_local int  g_InCreate = 0;
static thread_local int  g_InLoadCommon = 0;
static thread_local int  g_CprIndex = 0;
static thread_local int  g_LcRolls = 0;
static thread_local char g_SockLine[2048];
static thread_local int  g_SockLen = 0;
static thread_local char g_SockJson[2048];
static thread_local int  g_SockJsonLen = 0;
static thread_local char g_SockName[160];
static volatile long g_SockItems = 0;

// Copies an argument into buf as readable text.  Never throws, never indexes an
// RValue of an unexpected kind.
static void DescribeArg(RValue* a, char* buf, size_t cap)
{
    buf[0] = 0;
    if (!a) { strcpy_s(buf, cap, "null"); return; }
    try {
        if (a->m_Kind == VALUE_STRING) {
            const char* s = a->ToCString();
            sprintf_s(buf, cap, "\"%s\"", s ? s : "");
        } else if (a->m_Kind == VALUE_REAL || a->m_Kind == VALUE_INT32
                   || a->m_Kind == VALUE_INT64 || a->m_Kind == VALUE_BOOL) {
            sprintf_s(buf, cap, "%g", a->ToDouble());
        } else {
            sprintf_s(buf, cap, "kind%d", (int)a->m_Kind);
        }
    } catch (...) { strcpy_s(buf, cap, "?"); }
}

static RValue& HookCprIrandom(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    double bound = -1.0;
    if (argc >= 1 && A && A[0] && A[0]->m_Kind == VALUE_REAL) {
        try { bound = A[0]->ToDouble(); } catch (...) {}
    }
    // cpr_irandom is also reached from scripts nested inside CreateItemNew, so a
    // plain ordinal is meaningless.  Record the CALL SITE (return address as a
    // module RVA) - that identifies which of the game's call sites produced the
    // roll, and the two socket sites can then be picked out exactly.
    void* ret = _ReturnAddress();
    RValue& r = g_OrigCprIrandom ? g_OrigCprIrandom(S, O, R, argc, A) : R;
    // CreateItemNew calls LoadCommonItems first, and that routine burns a large
    // and item-dependent number of rolls.  They flooded the buffer on the first
    // run, so they are skipped here: what remains is CreateItemNew's own chain,
    // whose slots 2 and 3 are the two sides of the socket if/else.
    if (g_SockProbe && g_InCreate > 0 && g_InLoadCommon > 0) {
        // Only the count matters: it is how far LoadCommonItems advances the
        // shared RNG stream before the socket roll, so the editor's simulator
        // has to skip exactly this many draws to line up with the socket draw.
        g_LcRolls++;
    }
    if (g_SockProbe && g_InCreate > 0 && g_InLoadCommon == 0) {
        int idx = g_CprIndex++;
        double out = 0.0;
        try { if (r.m_Kind == VALUE_REAL) out = r.ToDouble(); } catch (...) {}
        unsigned long long rva = 0;
        if (g_GameBase) rva = (unsigned long long)ret - (unsigned long long)g_GameBase;
        if (idx < 64 && g_SockLen < (int)sizeof(g_SockLine) - 64) {
            int n = sprintf_s(g_SockLine + g_SockLen, sizeof(g_SockLine) - g_SockLen,
                              " [%llX b=%g r=%g]", rva, bound, out);
            if (n > 0) g_SockLen += n;
        }
        if (idx < 64 && g_SockJsonLen < (int)sizeof(g_SockJson) - 64) {
            int n = sprintf_s(g_SockJson + g_SockJsonLen, sizeof(g_SockJson) - g_SockJsonLen,
                              "%s[%llu,%g,%g]", idx ? "," : "", rva, bound, out);
            if (n > 0) g_SockJsonLen += n;
        }
    }
    return r;
}

// CreateItemInit is a one-line wrapper that forwards its argument to cpr_init,
// and CreateItemNew calls it before LoadCommonItems and before the socket roll.
// Logging its seed identifies which saved field drives the socket draw.
static PFUNC_YYGMLScript g_OrigCreateItemInit = nullptr;
static thread_local double g_InitSeed = -1.0;

static RValue& HookCreateItemInit(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_SockProbe && argc >= 1 && A && A[0]) {
        try { g_InitSeed = A[0]->ToDouble(); } catch (...) { g_InitSeed = -2.0; }
    }
    return g_OrigCreateItemInit ? g_OrigCreateItemInit(S, O, R, argc, A) : R;
}

static RValue& HookLoadCommonItems(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    g_InLoadCommon++;
    RValue& r = g_OrigLoadCommonItems ? g_OrigLoadCommonItems(S, O, R, argc, A) : R;
    g_InLoadCommon--;
    return r;
}

static RValue& HookCreateItemNew(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    const bool probing = g_SockProbe;
    if (probing && g_InCreate == 0) {
        g_CprIndex = 0; g_SockLen = 0; g_SockLine[0] = 0; g_LcRolls = 0; g_InitSeed = -1.0;
        g_SockJsonLen = 0; g_SockJson[0] = 0;
        // Record which item this is so the two items we care about can be found
        // in the log by name.
        char a0[80], a1[64];
        DescribeArg(argc > 0 && A ? A[0] : nullptr, a0, sizeof(a0));
        DescribeArg(argc > 1 && A ? A[1] : nullptr, a1, sizeof(a1));
        sprintf_s(g_SockName, "%s, %s", a0, a1);
    }
    g_InCreate++;
    RValue& r = g_OrigCreateItemNew ? g_OrigCreateItemNew(S, O, R, argc, A) : R;
    g_InCreate--;
    if (probing && g_InCreate == 0 && g_SockLen > 0) {
        long n = InterlockedIncrement(&g_SockItems);
        // The chain alone cannot say WHICH item it belongs to - CreateItemNew's
        // first argument is a struct, so it prints as a bare kind.  Serialising
        // the created item gives its base name and definition fields, which is
        // what turns a measured chain into a per-item socket entry.  Uses the
        // same json_stringify path the drop log already relies on.
        std::string itemJson;
        try {
            if (r.m_Kind == VALUE_OBJECT) {
                CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
                RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { r });
                itemJson = js.ToString();
                if (itemJson.find("itemDefinitionStruct") == std::string::npos) itemJson.clear();
            }
        } catch (...) { itemJson.clear(); }
        try {
            std::ofstream of(IPC_DIR + "\\socketchain.jsonl", std::ios::app);
            of << "{\"n\":" << n << ",\"seed\":" << (long long)g_InitSeed
               << ",\"lc\":" << g_LcRolls << ",\"rolls\":[" << g_SockJson << "]";
            if (!itemJson.empty()) of << ",\"it\":" << itemJson;
            of << "}\n";
        } catch (...) {}
        if (n <= 40) {
            char b[2600];
            sprintf_s(b, "socketprobe item#%ld (%s) seed=%.0f lc=%d rolls:%s", n, g_SockName, g_InitSeed, g_LcRolls, g_SockLine);
            Out(b);
        } else if (n % 250 == 0) {
            Out("socketprobe: " + std::to_string(n) + " items -> bp_ipc\\socketchain.jsonl");
        }
    }
    return r;
}

static void SocketProbeCmd(const std::string& rest)
{
    std::string v = Lower(rest);
    while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
    if (v == "off") { g_SockProbe = false; Out("socketprobe: OFF"); return; }
    if (!g_OrigCprIrandom
        && !HookOneScriptTable("cpr_irandom", "fp_cprir", (PVOID)HookCprIrandom, &g_OrigCprIrandom)) {
        Out("socketprobe: could not hook cpr_irandom"); return;
    }
    if (!g_OrigCreateItemNew
        && !HookOneScriptTable("CreateItemNew", "fp_citemnew", (PVOID)HookCreateItemNew, &g_OrigCreateItemNew)) {
        Out("socketprobe: could not hook CreateItemNew"); return;
    }
    if (!g_OrigLoadCommonItems
        && !HookOneScriptTable("LoadCommonItems", "fp_loadcommon", (PVOID)HookLoadCommonItems, &g_OrigLoadCommonItems)) {
        Out("socketprobe: could not hook LoadCommonItems"); return;
    }
    if (!g_OrigCreateItemInit
        && !HookOneScriptTable("CreateItemInit", "fp_citeminit", (PVOID)HookCreateItemInit, &g_OrigCreateItemInit)) {
        Out("socketprobe: could not hook CreateItemInit"); return;
    }
    g_SockItems = 0;
    g_SockProbe = true;
    Out("socketprobe: ON - the socket roll is CreateItemNew chain slot 2 or 3");
}
#endif

static void RareDropCmd(const std::string& rest)
{
    std::string a1, a2;
    a1 = FirstToken(rest, a2);
    while (!a1.empty() && std::isspace((unsigned char)a1.back())) a1.pop_back();
    while (!a2.empty() && std::isspace((unsigned char)a2.back())) a2.pop_back();
    std::string what = Lower(a1);
    Out("raredrop: enter '" + what + "' arg='" + a2 + "'");

    if (what.empty() || what == "list") {
        char b[224];
        sprintf_s(b, "  heroic  x%.2f  -> base %.0f / boosted %.0f percent  (vanilla 28/37)  hits=%ld",
                  g_HeroicMult,
                  (kHeroicBase * g_HeroicMult > 100.0 ? 100.0 : kHeroicBase * g_HeroicMult),
                  (kHeroicBoost * g_HeroicMult > 100.0 ? 100.0 : kHeroicBoost * g_HeroicMult),
                  g_RateHeroHits);
        Out(b);
        sprintf_s(b, "  ceiling x%.2f  -> every rare ladder rolls on a %.2fx smaller range  hits=%ld",
                  g_CeilingMult, g_CeilingMult, g_RateCeilHits);
        Out(b);
        sprintf_s(b, "  satanic x%.2f  -> monster level lifted, capped at %.0f  hits=%ld",
                  g_SatanicMult, kSatanicTopBracket, g_SatTierHits);
        Out(b);
        sprintf_s(b, "  angelic x%.2f  -> gate %s, rate x%.2f, rolls=%ld, scaled=%ld, last chance=%g",
                  g_AngelicMult, g_AngelicGatePatched ? "OPEN" : "closed",
                  g_AngelicRateMult, g_AngChanceCalls, g_AngRateHits,
                  std::isfinite(g_AngLastChance) ? g_AngLastChance : -1.0);
        Out(b);
        sprintf_s(b, "  store hook: %s, reads seen=%ld  key175=%ld key177=%ld key178=%ld",
                  g_OrigProtGet ? "on" : "off", g_ProtGetCalls, g_Seen175, g_Seen177, g_Seen178);
        Out(b);
        Out(std::string("  hooks: GPV=") + (g_OrigGpvRate ? "on" : "off")
            + " LoadSatanicDropTier=" + (g_OrigSatTier ? "on" : "off")
            + " keys=" + (g_RateKeysOk ? "resolved" : "not resolved"));
        return;
    }

    double mult = 1.0;
    try { mult = std::stod(a2); } catch (...) { Out("raredrop: multiplier must be a number"); return; }
    if (mult < 1.0) mult = 1.0;
    if (mult > 100.0) mult = 100.0;

    if (what == "heroic" || what == "ceiling") {
        if (mult > 1.0 && !EnsureProtGetHook()) {
            Out("raredrop " + what + ": could not hook the variable store - staying vanilla");
            return;
        }
        if (what == "heroic") {
            // Measured live (55M store reads over two sessions): keys 177/178 are
            // NEVER read during normal monster kills, so this multiplier cannot
            // change kill loot.  It stays wired for the special contexts that do
            // read them (vault/chest paths), but is reported honestly.
            g_HeroicMult = mult;
            Out("raredrop heroic: set, but NOTE - normal monster kills never read the "
                "heroic chance (measured); this only matters for special chest paths");
        } else {
            g_CeilingMult = mult;
            char b[192];
            sprintf_s(b, "raredrop ceiling: x%.2f -> every rare chance multiplied (affects EVERY ladder)",
                      mult);
            Out(b);
        }
        return;
    }

    if (what == "probe") {
        if (!EnsureProtGetHook()) return;
        if (!g_ProtProbe) {
            { std::lock_guard<std::mutex> lk(g_ProbeLock); g_ProbeSeen.clear(); }
            g_ProtProbe = true;
            Out("raredrop probe: ON - sampling distinct store keys; run it again to dump");
        } else {
            g_ProtProbe = false;
            std::map<double, double> snap;
            { std::lock_guard<std::mutex> lk(g_ProbeLock); snap = g_ProbeSeen; }
            char b[160];
            sprintf_s(b, "raredrop probe: OFF - %zu distinct keys:", snap.size());
            Out(b);
            std::string line;
            int shown = 0;
            for (const auto& kv : snap) {
                char e[64];
                sprintf_s(e, " %g=%g", kv.first, kv.second);
                line += e;
                if (++shown % 6 == 0) { Out("  " + line); line.clear(); }
            }
            if (!line.empty()) Out("  " + line);
        }
        return;
    }

    if (what == "angelic") {
        if (mult <= 1.0) {
            CloseAngelicGate();
            g_AngelicMult = 1.0;
            g_AngelicRateMult = 1.0;
            Out("raredrop angelic: off (vanilla)");
            return;
        }
        if (!OpenAngelicGate()) { Out("raredrop angelic: nothing changed"); return; }
        if (!g_OrigAngChance)
            HookOneScript("DropItemAngelicChance", "fp_angch",
                          (PVOID)HookAngelicChance, &g_OrigAngChance);
        g_AngelicMult = mult;
        g_AngelicRateMult = mult - 1.0;      // x2 = the game's own rate
        // Measured live: x9 scaling works (26% per roll), x99 produces ZERO drops -
        // an oversized chance value breaks the game's own check.  Hard cap at x9.
        if (g_AngelicRateMult > 9.0) g_AngelicRateMult = 9.0;
        char b[176];
        sprintf_s(b, "raredrop angelic: x%.2f -> gate open, %d roll(s) per kill at the game's own chance (x2 = one roll)",
                  mult, (int)std::lround(g_AngelicRateMult));
        Out(b);
        return;
    }

    if (what == "satanic") {
        if (mult > 1.0 && !g_OrigSatTier
            && !HookOneScript("LoadSatanicDropTier", "fp_sattier",
                              (PVOID)HookSatanicTier, &g_OrigSatTier)) {
            Out("raredrop: could not hook LoadSatanicDropTier");
            return;
        }
        g_SatanicMult = mult;
        char b[192];
        sprintf_s(b, "raredrop satanic: x%.2f -> monster level lifted for the tier roll (cap %.0f)",
                  mult, kSatanicTopBracket);
        Out(b);
        return;
    }

    Out("raredrop: unknown '" + a1 + "'  (heroic | ceiling | satanic | angelic | list)");
}

// ---- Satanic Zone mod pool: satmods <buff|debuff> <csv of disabled ids> ----
// Lets the game roll a Satanic Zone's buffs/debuffs exactly as it always
// has, then - only for the ids the user disabled - swaps in a random
// still-enabled id from the same polarity's full range instead. Nothing is
// forced: an id the user left on can still fail to appear, and turning
// everything back on (empty csv for both polarities) is bit-for-bit vanilla.
//
// Hook target is UNCONFIRMED against a live game (see
// docs/satanic-zone-mods-research.md, Phase 0) - LoadSatanicZone is the
// primary candidate; GetSatanicZoneOffline / LoadRandomSatanicStat are the
// documented fallbacks if probing shows the roll happens there instead.
static PFUNC_YYGMLScript g_OrigLoadSatanicZone = nullptr;
static std::set<int> g_SatDisabledBuffs;    // empty = every buff (1-25) enabled
static std::set<int> g_SatDisabledDebuffs;  // empty = every debuff (1-26) enabled
static volatile long g_SatModsHits = 0;
static volatile long g_SatLoadCalls = 0;   // every call, regardless of whether anything needed filtering
static const int kSatanicBuffCount = 25;
static const int kSatanicDebuffCount = 26;
// Research-only: log the first N LoadSatanicZone calls (args + resulting
// arrays) so Phase 0 can see call cadence directly in out.txt, not just infer
// it from g_SatModsHits (which only moves when a disabled id was present).
static volatile long g_SatLoadTraceLeft = 40;

// Resolves the single Controller_obj instance (same pattern as ObjVarJson).
static bool ResolveControllerObj(RValue& out)
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("Controller_obj") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) return false;
        out = id;
        return true;
    } catch (...) { return false; }
}

// Rewrites one polarity's array in place: any id in `disabled` is replaced by a
// random id NOT already present in the array and NOT disabled; if every id in
// [1, idCount] is disabled, that slot is left out of the array instead (the
// zone simply carries fewer/zero mods of that polarity - not a bug).
static void FilterSatanicArray(const RValue& controller, const char* varName,
                                const std::set<int>& disabled, int idCount)
{
    if (disabled.empty()) return;
    try {
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { controller, RValue(varName) });
        if (!ex.ToBoolean()) return;
        RValue arr = g_Yytk->CallBuiltin("variable_instance_get", { controller, RValue(varName) });
        if (arr.m_Kind != VALUE_ARRAY) return;
        int n = (int)g_Yytk->CallBuiltin("array_length", { arr }).ToDouble();
        if (n <= 0) return;

        std::set<int> present;
        std::vector<int> current(n, 0);
        for (int i = 0; i < n; ++i) {
            int id = (int)g_Yytk->CallBuiltin("array_get", { arr, RValue((double)i) }).ToDouble();
            current[i] = id;
            present.insert(id);
        }

        std::vector<int> replacement;   // same length as current, -1 = drop this slot
        bool changed = false;
        for (int i = 0; i < n; ++i) {
            int id = current[i];
            if (disabled.find(id) == disabled.end()) { replacement.push_back(id); continue; }
            changed = true;
            int pick = -1;
            for (int attempt = 0; attempt < 200 && pick < 0; ++attempt) {
                int cand = 1 + (std::rand() % idCount);
                if (disabled.find(cand) != disabled.end()) continue;
                if (present.find(cand) != present.end()) continue;
                pick = cand;
            }
            if (pick > 0) { present.insert(pick); replacement.push_back(pick); }
            // else: enabled pool exhausted for this polarity - drop the slot.
        }
        if (!changed) return;

        RValue newArr = g_Yytk->CallBuiltin("array_create", { RValue((double)replacement.size()) });
        for (size_t i = 0; i < replacement.size(); ++i)
            g_Yytk->CallBuiltin("array_set", { newArr, RValue((double)i), RValue((double)replacement[i]) });
        g_Yytk->CallBuiltin("variable_instance_set", { controller, RValue(varName), newArr });
        BP_DIAG_INCREMENT(g_SatModsHits);
    } catch (...) {}
}

static RValue& HookLoadSatanicZone(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    BP_DIAG_INCREMENT(g_SatLoadCalls);
    RValue& r = g_OrigLoadSatanicZone ? g_OrigLoadSatanicZone(S, O, R, argc, A) : R;
    bool traced = false;
    if (g_SatLoadTraceLeft > 0) {
        --g_SatLoadTraceLeft;
        traced = true;
        RValue controller;
        std::string buffsBefore = "?", debuffsBefore = "?";
        if (ResolveControllerObj(controller)) {
            try {
                RValue bv = g_Yytk->CallBuiltin("variable_instance_get", { controller, RValue("satanicZoneBuff") });
                RValue dv = g_Yytk->CallBuiltin("variable_instance_get", { controller, RValue("satanicZoneDebuff") });
                buffsBefore = Describe(bv);
                debuffsBefore = Describe(dv);
            } catch (...) {}
        }
        Out("satmods TRACE: LoadSatanicZone call#" + std::to_string(g_SatLoadCalls) + " argc=" + std::to_string(argc)
            + (argc >= 1 && A && A[0] ? " arg0=" + Describe(*A[0]) : "")
            + " -> " + Describe(r)
            + " | before filter: buffs=" + buffsBefore + " debuffs=" + debuffsBefore);
    }
    if (!g_SatDisabledBuffs.empty() || !g_SatDisabledDebuffs.empty()) {
        RValue controller;
        if (ResolveControllerObj(controller)) {
            FilterSatanicArray(controller, "satanicZoneBuff", g_SatDisabledBuffs, kSatanicBuffCount);
            FilterSatanicArray(controller, "satanicZoneDebuff", g_SatDisabledDebuffs, kSatanicDebuffCount);
        }
    }
    if (traced) {
        try {
            RValue controller;
            if (ResolveControllerObj(controller)) {
                RValue bv = g_Yytk->CallBuiltin("variable_instance_get", { controller, RValue("satanicZoneBuff") });
                RValue dv = g_Yytk->CallBuiltin("variable_instance_get", { controller, RValue("satanicZoneDebuff") });
                Out("satmods TRACE:   after filter: buffs=" + Describe(bv) + " debuffs=" + Describe(dv));
            }
        } catch (...) {}
    }
    return r;
}

static bool EnsureSatanicZoneHook()
{
    if (g_OrigLoadSatanicZone) return true;
    return HookOneScript("LoadSatanicZone", "fp_loadsz", (PVOID)HookLoadSatanicZone, &g_OrigLoadSatanicZone);
}

// ---- Poll-and-correct: the actual filtering mechanism -----------------
// Live research 2026-09-10 found LoadSatanicZone never fires during normal
// play (call counter stayed 0 through zone loads, waypoint travel); rereading
// HS-Offline-Tracker's own README, LoadSatanicZone(room) is THEIR diagnostic
// code asking the game a question, not something the game calls on its own
// during a real roll. So hooking a specific "roll" routine is the wrong
// approach - the actual write site is still unidentified (and may not even
// be a single script; the array visibly changed once mid-session with the
// hook installed and calls staying at 0).
//
// Instead: sample Controller_obj.satanicZoneBuff/Debuff every
// kSatanicPollFrames frames from the existing FrameCallback, and the instant
// the content differs from what we last saw (i.e. the game changed it),
// filter it right then. This needs no knowledge of which script performs the
// roll, and is only as stale as the poll interval (well under a second).
static const uint32_t kSatanicPollFrames = 15;   // ~0.25s at 60fps
static std::vector<int> g_SatLastSeenBuffs;
static std::vector<int> g_SatLastSeenDebuffs;
static volatile long g_SatPollChanges = 0;   // how many times a poll saw new content (game-driven, not ours)
// Verbose "content changed" trace: dev builds only, so a player's out.txt
// does not fill up with every Satanic Zone roll they happen to see.
#ifndef FORGEPACT_RELEASE
static volatile long g_SatLoadTraceLeftPoll = 40;
#else
static volatile long g_SatLoadTraceLeftPoll = 0;
#endif

static std::vector<int> ReadIntArray(const RValue& controller, const char* varName)
{
    std::vector<int> out;
    try {
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { controller, RValue(varName) });
        if (!ex.ToBoolean()) return out;
        RValue arr = g_Yytk->CallBuiltin("variable_instance_get", { controller, RValue(varName) });
        if (arr.m_Kind != VALUE_ARRAY) return out;
        int n = (int)g_Yytk->CallBuiltin("array_length", { arr }).ToDouble();
        for (int i = 0; i < n; ++i)
            out.push_back((int)g_Yytk->CallBuiltin("array_get", { arr, RValue((double)i) }).ToDouble());
    } catch (...) {}
    return out;
}

// Called every kSatanicPollFrames frames from FrameCallback.
static void SatanicPollTick()
{
    // Cheap bail-out: nothing to correct and nothing to trace.
    if (g_SatDisabledBuffs.empty() && g_SatDisabledDebuffs.empty() && g_SatLoadTraceLeftPoll <= 0) return;
    RValue controller;
    if (!ResolveControllerObj(controller)) return;

    std::vector<int> buffs = ReadIntArray(controller, "satanicZoneBuff");
    std::vector<int> debuffs = ReadIntArray(controller, "satanicZoneDebuff");
    const bool changed = (buffs != g_SatLastSeenBuffs) || (debuffs != g_SatLastSeenDebuffs);

    if (changed) {
        BP_DIAG_INCREMENT(g_SatPollChanges);
        if (g_SatLoadTraceLeftPoll > 0) {
            --g_SatLoadTraceLeftPoll;
            auto fmt = [](const std::vector<int>& v) {
                std::string s = "[";
                for (size_t i = 0; i < v.size(); ++i) s += (i ? "," : "") + std::to_string(v[i]);
                return s + "]";
            };
            Out("satmods POLL: content changed (#" + std::to_string(g_SatPollChanges) + ") buffs "
                + fmt(g_SatLastSeenBuffs) + " -> " + fmt(buffs) + "  debuffs "
                + fmt(g_SatLastSeenDebuffs) + " -> " + fmt(debuffs));
        }
    }

    if (!g_SatDisabledBuffs.empty() || !g_SatDisabledDebuffs.empty()) {
        FilterSatanicArray(controller, "satanicZoneBuff", g_SatDisabledBuffs, kSatanicBuffCount);
        FilterSatanicArray(controller, "satanicZoneDebuff", g_SatDisabledDebuffs, kSatanicDebuffCount);
        // Re-read post-filter so the cache reflects what we left behind, not
        // what the game rolled - otherwise our own correction would look
        // like another "game changed it" event next tick.
        g_SatLastSeenBuffs = ReadIntArray(controller, "satanicZoneBuff");
        g_SatLastSeenDebuffs = ReadIntArray(controller, "satanicZoneDebuff");
    } else {
        g_SatLastSeenBuffs = buffs;
        g_SatLastSeenDebuffs = debuffs;
    }
}

static std::set<int> ParseIdCsv(const std::string& csv)
{
    std::set<int> out;
    size_t p = 0;
    while (p <= csv.size()) {
        size_t c = csv.find(',', p);
        if (c == std::string::npos) c = csv.size();
        std::string tok = TrimCopy(csv.substr(p, c - p));
        if (!tok.empty()) { try { out.insert(std::stoi(tok)); } catch (...) {} }
        p = c + 1;
    }
    return out;
}

// satmods <buff|debuff> <csv of disabled ids>   -- empty csv clears that polarity
// satmods status                                 -- report current state
static void SatModsCmd(const std::string& rest)
{
    std::string a1, a2;
    a1 = Lower(TrimCopy(FirstToken(rest, a2)));
    a2 = TrimCopy(a2);

    if (a1.empty() || a1 == "status") {
        Out("satmods: poll(active" + std::string((!g_SatDisabledBuffs.empty() || !g_SatDisabledDebuffs.empty()) ? "" : ", nothing to filter yet")
            + ")=every " + std::to_string(kSatanicPollFrames) + " frames"
            + " disabled buffs=" + std::to_string(g_SatDisabledBuffs.size()) + "/" + std::to_string(kSatanicBuffCount)
            + " disabled debuffs=" + std::to_string(g_SatDisabledDebuffs.size()) + "/" + std::to_string(kSatanicDebuffCount)
            + " pollChanges=" + std::to_string(g_SatPollChanges)
            + " hits=" + std::to_string(g_SatModsHits)
            + " | diag: LoadSatanicZone hook=" + std::string(g_OrigLoadSatanicZone ? "on" : "off")
            + " calls=" + std::to_string(g_SatLoadCalls));
        return;
    }
    if (a1 != "buff" && a1 != "debuff") {
        Out("satmods: unknown '" + a1 + "' (buff | debuff | status)");
        return;
    }

    std::set<int>& target = (a1 == "buff") ? g_SatDisabledBuffs : g_SatDisabledDebuffs;
    target = ParseIdCsv(a2);
    // Best-effort diagnostic hook - poll-and-correct (SatanicPollTick, driven
    // from FrameCallback) is the real filtering mechanism and needs no hook,
    // so a failure here is not fatal to the feature. See the comment above
    // SatanicPollTick for why LoadSatanicZone stopped being trusted as the hook target.
    if (!target.empty()) EnsureSatanicZoneHook();
    Out("satmods " + a1 + ": " + std::to_string(target.size()) + " id(s) disabled");
}

// StatCmd/StatAddCmd moved to ForgePact::StatsManager (see the module includes anchor above).
// itemjson <path> -- json_parse file -> InitItemFromJson -> json_stringify result to bp_ipc\iteminfo.json
static void ItemJson(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { Out("itemjson: cannot open " + path); return; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();
    RValue item; bool ok = false;
    try {
        RValue parsed = g_Yytk->CallBuiltin("json_parse", { RValue(content) });
        item = g_Yytk->CallGameScript("gml_Script_InitItemFromJson", { parsed });
        ok = true;
    } catch (...) { Out("itemjson: struct-arg EXCEPTION, trying raw string"); }
    if (!ok) {
        try { item = g_Yytk->CallGameScript("gml_Script_InitItemFromJson", { RValue(content) }); ok = true; }
        catch (...) { Out("itemjson: string-arg EXCEPTION too"); return; }
    }
    try {
        RValue s = g_Yytk->CallBuiltin("json_stringify", { item });
        std::string js = s.ToString();
        std::string opath = IPC_DIR + "\\iteminfo.json";
        std::ofstream of(opath, std::ios::binary); of << js;
        Out("itemjson -> item " + Describe(item) + ", " + std::to_string(js.size()) + " bytes -> iteminfo.json");
    } catch (...) { Out("itemjson: stringify EXCEPTION (item=" + Describe(item) + ")"); }
}

// jstat <path> -- InitItemFromJson WITH self-context (CallGameScriptEx self=global), then
// json_stringify the computed item + GetItemStatString/Tooltip. Lets us decode any jewel
// n-array headlessly (which affix/stat it produces).
static CInstance* GetPlayerInstance()
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) return nullptr;
        CInstance* inst = nullptr;
        if (AurieSuccess(g_Yytk->GetInstanceObject((int32_t)id.ToDouble(), inst))) return inst;
    } catch (...) {}
    return nullptr;
}

static void JStat(const std::string& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) { Out("jstat: cannot open " + path); return; }
    std::stringstream ss; ss << f.rdbuf();
    std::string content = ss.str();

    CInstance* self = GetPlayerInstance();
    if (!self) { g_Yytk->GetGlobalInstance(&self); Out("jstat: no player, self=global"); }
    else Out("jstat: self=Player_obj");

    // parsed struct (b,c,j,n,a). We try GetItemStatString on (A) InitItemFromJson result,
    // and (B) the raw parsed struct directly -- whichever the game accepts.
    RValue parsed;
    try { g_Yytk->CallBuiltinEx(parsed, "json_parse", self, self, { RValue(content) }); }
    catch (...) { Out("jstat: json_parse EXCEPTION"); return; }

    RValue item; bool haveItem = false;
    try {
        AurieStatus st = g_Yytk->CallGameScriptEx(item, "gml_Script_InitItemFromJson", self, self, { parsed });
        haveItem = AurieSuccess(st) && item.m_Kind != VALUE_UNDEFINED;
        Out("jstat: InitItemFromJson -> " + Describe(item) + " st=" + std::to_string((int)st));
    } catch (...) { Out("jstat: InitItemFromJson EXCEPTION (will use raw struct)"); }

    // candidate item RValues to try the readers on
    std::vector<std::pair<std::string, RValue>> cands;
    if (haveItem) cands.push_back({ "init", item });
    cands.push_back({ "raw", parsed });

    for (auto& c : cands) {
        Out("jstat: --- trying readers on '" + c.first + "' (" + Describe(c.second) + ") ---");
        try { RValue r; g_Yytk->CallGameScriptEx(r, "gml_Script_GetItemStatString", self, self, { c.second });
              Out("jstat[" + c.first + "] STATSTR: " + r.ToString()); } catch (...) { Out("jstat[" + c.first + "] StatString EXC"); }
        try { RValue r; g_Yytk->CallGameScriptEx(r, "gml_Script_GetItemTooltipString", self, self, { c.second });
              Out("jstat[" + c.first + "] TOOLTIP: " + r.ToString()); } catch (...) { Out("jstat[" + c.first + "] Tooltip EXC"); }
        try { RValue r; g_Yytk->CallGameScriptEx(r, "gml_Script_GetItemStats", self, self, { c.second });
              std::string d = Describe(r);
              if (r.m_Kind == VALUE_OBJECT || r.m_Kind == VALUE_ARRAY) { RValue s; g_Yytk->CallBuiltinEx(s, "json_stringify", self, self, { r }); d += " json=" + s.ToString(); }
              Out("jstat[" + c.first + "] STATS: " + d); } catch (...) { Out("jstat[" + c.first + "] GetItemStats EXC"); }
    }
}

// callnum <Script> [n1] [n2] ... -- call game script with NUMERIC args; json_stringify struct/array results
static void CallNum(const std::string& rest)
{
    std::stringstream ss(rest);
    std::string name; ss >> name;
    if (name.empty()) { Out("callnum: need script name"); return; }
    std::vector<RValue> args; double d;
    while (ss >> d) args.push_back(RValue(d));
    try {
        RValue r = g_Yytk->CallGameScript("gml_Script_" + name, args);
        std::string desc = Describe(r);
        if (r.m_Kind == VALUE_OBJECT || r.m_Kind == VALUE_ARRAY) {
            try {
                RValue s = g_Yytk->CallBuiltin("json_stringify", { r });
                desc += " json=" + s.ToString();
            } catch (...) { desc += " (stringify failed)"; }
        }
        Out("callnum " + name + " (" + std::to_string(args.size()) + " args) -> " + desc);
    } catch (...) { Out("callnum EXCEPTION calling " + name); }
}

// Player array tools: read/find/set elements of an array instance-var on Player_obj (e.g. pSt).
static RValue GetPlayerArr(const std::string& var, int& len)
{
    len = -1;
    RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
    RValue id = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
    if (id.ToDouble() < 0) return RValue();
    RValue arr = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(var) });
    if (arr.m_Kind != VALUE_ARRAY) return RValue();
    RValue n = g_Yytk->CallBuiltin("array_length", { arr });
    len = (int)n.ToDouble();
    return arr;
}
static void PFind(const std::string& var, double value)
{
    try {
        int len = -1; RValue arr = GetPlayerArr(var, len);
        if (len < 0) { Out("pfind: " + var + " not an array on player"); return; }
        std::string hits; int c = 0;
        for (int i = 0; i < len; i++) {
            RValue e = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)i) });
            double d = 0; try { d = e.ToDouble(); } catch (...) { continue; }
            if (d == value) { hits += std::to_string(i) + " "; c++; }
        }
        Out("pfind " + var + " == " + std::to_string(value) + " -> indices: " + (hits.empty() ? "(none)" : hits) + "(len=" + std::to_string(len) + ")");
    } catch (...) { Out("pfind EXCEPTION"); }
}
static void PGet(const std::string& var, int idx)
{
    try {
        int len = -1; RValue arr = GetPlayerArr(var, len);
        if (len < 0) { Out("pget: not an array"); return; }
        if (idx < 0 || idx >= len) { Out("pget: index out of range (len=" + std::to_string(len) + ")"); return; }
        RValue e = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)idx) });
        Out("pget " + var + "[" + std::to_string(idx) + "] -> " + Describe(e));
    } catch (...) { Out("pget EXCEPTION"); }
}
static void PSet(const std::string& var, int idx, double value)
{
    try {
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        if (id.ToDouble() < 0) { Out("pset: no player"); return; }
        RValue arr = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(var) });
        if (arr.m_Kind != VALUE_ARRAY) { Out("pset: " + var + " not array"); return; }
        g_Yytk->CallBuiltin("array_set", { arr, RValue((double)idx), RValue(value) });
        RValue e = g_Yytk->CallBuiltin("array_get", { arr, RValue((double)idx) });
        Out("pset " + var + "[" + std::to_string(idx) + "] = " + std::to_string(value) + " -> now " + Describe(e));
    } catch (...) { Out("pset EXCEPTION"); }
}

// inames <ObjName> [filter] -- dump all instance variable names of the first instance, optionally filtered.
static void InstanceNames(const std::string& objName, const std::string& filter)
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) { Out("inames: no instance of " + objName); return; }
        RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { id });
        RValue cnt = g_Yytk->CallBuiltin("array_length", { names });
        int n = (int)cnt.ToDouble();
        std::string flt = Lower(filter);
        std::string line; int shown = 0;
        for (int i = 0; i < n; i++) {
            RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
            std::string s = nm.ToString();
            if (!flt.empty() && Lower(s).find(flt) == std::string::npos) continue;
            line += s + " ";
            if (++shown % 8 == 0) { Out("  " + line); line.clear(); }
        }
        if (!line.empty()) Out("  " + line);
        Out("inames " + objName + ": " + std::to_string(n) + " total" + (flt.empty() ? "" : (", filter '" + filter + "'")));
    } catch (...) { Out("inames EXCEPTION"); }
}

// cb <builtinName> [args...]  -- numeric args -> real, otherwise string. Calls any GM builtin.
static void CallBuiltinCmd(const std::string& rest)
{
    std::stringstream ss(rest);
    std::string name; ss >> name;
    if (name.empty()) { Out("cb: need a builtin name"); return; }
    std::vector<RValue> args;
    std::string tok;
    while (ss >> tok) {
        try {
            size_t pos; double d = std::stod(tok, &pos);
            if (pos == tok.size()) { args.push_back(RValue(d)); continue; }
        } catch (...) {}
        args.push_back(RValue(tok));
    }
    try {
        RValue r = g_Yytk->CallBuiltin(name.c_str(), args);
        Out("cb " + name + " (" + std::to_string(args.size()) + " args) -> " + Describe(r));
    } catch (...) { Out("cb EXCEPTION calling " + name); }
}

// Read/write an instance variable on the first instance of a named object.
static void ObjVarGet(const std::string& objName, const std::string& var)
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) { Out("oget: no instance of " + objName); return; }
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue(var) });
        if (!ex.ToBoolean()) { Out("oget " + objName + "." + var + " -> (no such var)"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(var) });
        Out("oget " + objName + "." + var + " -> " + Describe(v));
    } catch (...) { Out("oget EXCEPTION"); }
}
static void ObjVarSet(const std::string& objName, const std::string& var, double val)
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) { Out("oset: no instance of " + objName); return; }
        g_Yytk->CallBuiltin("variable_instance_set", { id, RValue(var), RValue(val) });
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(var) });
        Out("oset " + objName + "." + var + " = " + std::to_string(val) + " -> now " + Describe(v));
    } catch (...) { Out("oset EXCEPTION"); }
}

// Spawn an object at the player's position (sidesteps zone-gen gating).
static void SpawnAtPlayer(int objIdx)
{
    try {
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue pid = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        double pidv = pid.ToDouble();
        if (pidv < 0) { Out("SpawnAtPlayer: no Player_obj instance (id=" + std::to_string(pidv) + ")"); return; }
        RValue px = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("x") });
        RValue py = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("y") });
        g_Yytk->CallBuiltin("instance_create_depth", { px, py, RValue(0.0), RValue((double)objIdx) });
        char b[160];
        sprintf_s(b, "spawned obj %d at player (%.0f, %.0f)", objIdx, px.ToDouble(), py.ToDouble());
        Out(b);
    } catch (...) { Out("SpawnAtPlayer EXCEPTION"); }
}

// --- Chaos Tower ------------------------------------------------------------
// OLCULDU (2026-08-28): kule bolgede yalnizca oyunun sectigi 10 bolgede
// olusuyor ve o secimi yapan ZoneGenChaosTower disaridan cagrilinca hata
// veriyor (bolge uretiminin ortasinda calismak uzere yazilmis).  Marker'i
// (Spawn_Chaos_Tower_obj) sonradan yaratmak da ise yaramiyor: bes ornek
// canli kaldi, hicbiri tepki vermedi - o marker bolge uretilirken tuketiliyor.
//
// Calisan tek yol: Chaos_Tower_obj nesnesini DOGRUDAN yaratmak.  Tek basina
// yeterli - NPC, kontrolcu, marker ya da eSt kapisi gerekmiyor (temiz bir
// bolgede tek ornekle dogrulandi, oyuncu kuleye girebildi).
//
// Bu, projedeki diger ozelliklerin aksine oyunun kendi yerlestirmesini
// kullanmiyor; kuleyi biz koyuyoruz.  Dogal yol ZoneGenChaosTower'i oyunun
// kendisine cagirtmaktan geciyor ama onu tetikleyen sart henuz bulunamadi.
static bool g_CtOto = false;          // her bolgede bir kule
static double g_CtSonOda = -1.0;      // bolge degisimini yakalamak icin
static int  g_CtGecikme = 0;          // oyuncu yerlesene kadar bekle (kare)

static void ChaosTowerKur(bool sessiz)
{
    try {
        RValue idx = g_Yytk->CallBuiltin("asset_get_index", { RValue("Chaos_Tower_obj") });
        int oi = (int)idx.ToDouble();
        if (oi < 0) { if (!sessiz) Out("chaostower: Chaos_Tower_obj bulunamadi"); return; }
        RValue n = g_Yytk->CallBuiltin("instance_number", { RValue((double)oi) });
        if ((int)n.ToDouble() > 0) {          // bu bolgede zaten var - ikinciyi koyma
            if (!sessiz) Out("chaostower: bu bolgede zaten bir kule var");
            return;
        }
        SpawnAtPlayer(oi);
        if (!sessiz) Out("chaostower: kule kuruldu");
    } catch (...) { if (!sessiz) Out("chaostower EXCEPTION"); }
}

// Her karede cagrilir; bolge degisimini yakalayip kuleyi kurar.
static void ChaosTowerTick()
{
    if (!g_CtOto || !g_Yytk) return;
    try {
        RValue oda = g_Yytk->CallBuiltin("variable_global_get", { RValue("room") });
        double o = oda.ToDouble();
        if (o != g_CtSonOda) {
            g_CtSonOda = o;
            g_CtGecikme = 90;   // ~1.5 sn: oyuncu ve zemin yerlessin
            return;
        }
        if (g_CtGecikme > 0 && --g_CtGecikme == 0) ChaosTowerKur(true);
    } catch (...) {}
}

static void SpawnByName(const std::string& name)
{
    try {
        RValue idx = g_Yytk->CallBuiltin("asset_get_index", { RValue(name) });
        int oi = (int)idx.ToDouble();
        if (oi < 0) { Out("spawnname: '" + name + "' not found (idx=-1)"); return; }
        Out("spawnname '" + name + "' -> idx " + std::to_string(oi));
        SpawnAtPlayer(oi);
    } catch (...) { Out("SpawnByName EXCEPTION"); }
}

// Force a relic to drop at the player on demand. DropRelic has NO internal Satanic-Zone
// gate (the restriction lives in its callers); it reads the drop x/y from argv[0]/argv[1]
// and spawns via LootGroundCreate. So we call the original trampoline with the player's
// coords as args and the player instance as self.
static void ForceRelicDrop(int n)
{
    if (n < 1) n = 1;
    if (n > 200) n = 200;
    if (!g_Orig_DropRelic) { Out("forcerelic: DropRelic not hooked yet"); return; }
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) { Out("forcerelic: no Player_obj instance (be in a level)"); return; }
        RValue px = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("x") });
        RValue py = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("y") });
        CInstance* self = nullptr;
        g_Yytk->GetInstanceObject((int32_t)id.ToDouble(), self);
        if (!self) { Out("forcerelic: cannot resolve player CInstance"); return; }
        for (int i = 0; i < n; i++) {
            RValue ax = px; RValue ay = py;
            RValue* argv[2] = { &ax, &ay };
            RValue tmp;
            try { g_Orig_DropRelic(self, self, tmp, 2, argv); } catch (...) {}
        }
        char b[128];
        sprintf_s(b, "forcerelic: %d relic call(s) at player (%.0f, %.0f)", n, px.ToDouble(), py.ToDouble());
        Out(b);
    } catch (...) { Out("forcerelic EXCEPTION"); }
}

// ===== Relic gate: KALDIRILDI ===============================================
// Sabit RVA 0x207F837 kullaniyordu.  O adres mevcut S10 derlemesinde
// gml_Script_DropItem'in icinde bile degil (DropItem 0x184c310..0x1870BE0),
// yani oyun her yeniden derlendiginde alakasiz bir yeri gosteriyor.  Imza
// kontrolu yamayi engelledigi icin ozellik zaten calismiyordu.
// Komut, eski ayar dosyalari gurultu uretmesin diye duruyor; artik no-op.
static void SetRelicGate(bool)
{
    Out("relicgate: bu ozellik kaldirildi (sabit adres oyun guncellemeleriyle kayiyor)");
}

// ============================================================
// ===== LOCAL MULTIPLAYER (Hamachi co-op) toolkit ============
// ============================================================
// Architecture recon + steam_net P2P transport test, all driven
// from the plugin so we never touch the EOS/license menu gate.

// Build an INT64-typed RValue from a decimal string (SteamIDs exceed 2^53,
// so they must NOT go through double). Returns true on success.
static bool Int64FromStr(const std::string& s, RValue& out)
{
    try {
        unsigned long long v = std::stoull(s);
        out = RValue((int64_t)v);
        return true;
    } catch (...) { return false; }
}

// steamid -- our own identity from the (emulated) Steam: init flag, 64-bit id, name.
static void SteamId()
{
    try {
        RValue init = g_Yytk->CallBuiltin("steam_initialised", {});
        Out("steam_initialised -> " + Describe(init));
    } catch (...) { Out("steam_initialised -> EXCEPTION (builtin missing?)"); }
    try {
        RValue id = g_Yytk->CallBuiltin("steam_get_user_steam_id", {});
        Out("steam_get_user_steam_id -> " + Describe(id) + "  (int64=" + std::to_string(id.ToInt64()) + ")");
    } catch (...) { Out("steam_get_user_steam_id -> EXCEPTION"); }
    try {
        RValue nm = g_Yytk->CallBuiltin("steam_get_persona_name", {});
        Out("steam_get_persona_name -> " + Describe(nm));
    } catch (...) { Out("steam_get_persona_name -> EXCEPTION"); }
}

// netscripts -- confirm every multiplayer-relevant script/builtin is resolvable
// and report its index. Tells us which entry points we can actually drive.
static void NetScripts()
{
    static const char* names[] = {
        // host/client room state machine
        "RunningHost", "GetRoomHostId", "MultiplayerReset",
        "CA_clientRoomJoinReady", "CA_herssiPlayerJoined", "CA_herssiPlayerDisconnected",
        "CA_setEnemyHost", "CA_setEnemyHostToRoomHost", "CA_updateLobbyLeader",
        "NetworkSendJoiningPlayer", "NetworkRoomGoto", "NetworkRoomSetupDone",
        "NetworkFinishPacket", "NetworkSendClient", "NetworkSendClientPlayerPosition",
        "CreateOnlineGame", "PlayerDisconnect", "ConvertOnlineInventory",
    };
    for (const char* n : names) {
        int idx = -1;
        std::string full = std::string("gml_Script_") + n;
        AurieStatus s = g_Yytk->GetNamedRoutineIndex(full.c_str(), &idx);
        Out(std::string("  script ") + n + " -> " + (AurieSuccess(s) && idx >= 0
            ? ("OK idx=" + std::to_string(idx)) : ("MISSING st=" + std::to_string((int)s))));
    }
    static const char* builtins[] = {
        "steam_net_set_auto_accept_p", "steam_net_accept_p", "steam_net_packet_send",
        "steam_net_packet_receive", "steam_net_packet_get_sender_id", "steam_net_packet_get_size",
        "steam_net_packet_get_data", "steam_lobby_create", "steam_lobby_get_lobby_id",
        "buffer_create", "buffer_write", "buffer_delete",
    };
    for (const char* n : builtins) {
        PVOID p = nullptr;
        AurieStatus s = g_Yytk->GetNamedRoutinePointer(n, &p);
        Out(std::string("  builtin ") + n + " -> " + (AurieSuccess(s) && p ? "OK" : ("MISSING st=" + std::to_string((int)s))));
    }
}

// netdump -- dump every global whose name matches any multiplayer keyword, int64-safe.
static void NetDump()
{
    CInstance* global = nullptr;
    AurieStatus st = g_Yytk->GetGlobalInstance(&global);
    if (!AurieSuccess(st) || !global) { Out("netdump: GetGlobalInstance failed"); return; }
    static const char* keys[] = { "host", "client", "room", "herssi", "online",
        "multiplayer", "lobby", "network", "net_", "peer", "session", "steamid", "player_id" };
    RValue globalrv = RValue(global);
    int count = 0;
    g_Yytk->EnumInstanceMembers(globalrv,
        [&](const char* name, RValue* val) -> bool {
            if (!name) return false;
            std::string ln = Lower(name);
            for (const char* k : keys) {
                if (ln.find(k) != std::string::npos) {
                    Out("  global." + std::string(name) + " = " + (val ? Describe(*val) : "<null>"));
                    count++;
                    break;
                }
            }
            return false;
        });
    Out("netdump -> " + std::to_string(count) + " network-ish globals");
}

// netstate -- the key host/client flags, read as globals.
static void NetState()
{
    static const char* vars[] = { "RunningHost", "is_client", "roomHost", "runningHost", "isClient" };
    for (const char* v : vars) {
        try {
            RValue ex = g_Yytk->CallBuiltin("variable_global_exists", { RValue(v) });
            if (ex.ToBoolean()) {
                RValue r = g_Yytk->CallBuiltin("variable_global_get", { RValue(v) });
                Out(std::string("  global.") + v + " = " + Describe(r));
            } else {
                Out(std::string("  global.") + v + " = (not a global)");
            }
        } catch (...) { Out(std::string("  global.") + v + " -> EXCEPTION"); }
    }
}

// ----- steam_net P2P transport test -----
static bool g_P2PPoll = false;       // when true, poll steam_net_packet_receive every frame
static volatile long g_P2PRecv = 0;  // total packets received
static std::string g_P2PLog;         // distinct "sender->size" entries

// p2paccept <0|1> -- enable auto-accept of incoming P2P sessions (host side).
static void P2PAccept(const std::string& arg)
{
    bool on = (arg.find('1') != std::string::npos) || Lower(arg).find("on") != std::string::npos;
    try {
        RValue r = g_Yytk->CallBuiltin("steam_net_set_auto_accept_p", { RValue(on ? 1.0 : 0.0) });
        Out("steam_net_set_auto_accept_p(" + std::string(on ? "true" : "false") + ") -> " + Describe(r));
    } catch (...) { Out("p2paccept -> EXCEPTION"); }
}

// p2psend <steamid> -- send a small probe packet to a peer SteamID over steam_net P2P.
// Proves the Goldberg transport carries data between two instances (LAN/Hamachi).
static void P2PSend(const std::string& idStr)
{
    RValue id;
    if (!Int64FromStr(idStr, id)) { Out("p2psend: bad steamid '" + idStr + "'"); return; }
    try {
        // small grow buffer, alignment 1; write a 4-byte probe payload
        RValue buf = g_Yytk->CallBuiltin("buffer_create", { RValue(16.0), RValue(1.0), RValue(1.0) });
        const unsigned char probe[4] = { 0xC0, 0x0F, 0xEE, 0x01 };
        for (unsigned char b : probe)
            g_Yytk->CallBuiltin("buffer_write", { buf, RValue(1.0) /*buffer_u8*/, RValue((double)b) });
        RValue size = g_Yytk->CallBuiltin("buffer_tell", { buf });
        RValue r = g_Yytk->CallBuiltin("steam_net_packet_send", { id, buf, size });
        Out("steam_net_packet_send(id=" + std::to_string(id.ToInt64()) + ", size=" + Describe(size) + ") -> " + Describe(r));
        g_Yytk->CallBuiltin("buffer_delete", { buf });
    } catch (...) { Out("p2psend -> EXCEPTION (check netscripts for builtin availability)"); }
}

// p2ppoll <0|1> -- toggle per-frame receive polling. When a packet arrives we
// log the sender's 64-bit SteamID and payload size = PROOF the transport works.
static void P2PPoll(const std::string& arg)
{
    g_P2PPoll = (arg.find('1') != std::string::npos) || Lower(arg).find("on") != std::string::npos;
    Out(std::string("p2ppoll -> ") + (g_P2PPoll ? "ON" : "OFF"));
}

static void P2PStats()
{
    Out("p2pstats: received=" + std::to_string(g_P2PRecv) + " polling=" + (g_P2PPoll ? "ON" : "OFF"));
    Out("  senders->size: " + (g_P2PLog.empty() ? std::string("(none yet)") : g_P2PLog));
}

// Called every frame when g_P2PPoll is on.
static void P2PReceiveTick()
{
    try {
        RValue got = g_Yytk->CallBuiltin("steam_net_packet_receive", {});
        bool any = false;
        try { any = got.ToBoolean(); } catch (...) {}
        double sz = 0; try { sz = got.ToDouble(); } catch (...) {}
        if (!any && sz == 0) return;
        InterlockedIncrement(&g_P2PRecv);
        long long sender = 0;
        try { RValue s = g_Yytk->CallBuiltin("steam_net_packet_get_sender_id", {}); sender = s.ToInt64(); } catch (...) {}
        long long psize = (long long)sz;
        try { RValue ps = g_Yytk->CallBuiltin("steam_net_packet_get_size", {}); psize = ps.ToInt64(); } catch (...) {}
        std::string entry = "(" + std::to_string(sender) + "->" + std::to_string(psize) + ")";
        if (g_P2PLog.size() < 1000 && g_P2PLog.find(entry) == std::string::npos) g_P2PLog += entry;
    } catch (...) {}
}

// Parse one CLI token into an RValue: big all-digit -> int64 (SteamIDs!), else real, else string.
static RValue ParseArgToken(const std::string& tok)
{
    bool alldigits = !tok.empty();
    size_t st = (tok[0] == '-') ? 1 : 0;
    if (st >= tok.size()) alldigits = false;
    for (size_t i = st; i < tok.size(); i++) if (!std::isdigit((unsigned char)tok[i])) { alldigits = false; break; }
    if (alldigits && (tok.size() - st) >= 10) {     // big integer -> int64 (SteamID range)
        try { return RValue((int64_t)std::stoll(tok)); } catch (...) {}
    }
    try { size_t pos; double d = std::stod(tok, &pos); if (pos == tok.size()) return RValue(d); } catch (...) {}
    return RValue(tok);
}

// callext <Name> [args...] -- call an EXTENSION/builtin function BY INDEX via the runner's
// Script_Perform dispatch (CallBuiltin can't resolve extension funcs; GetNamedRoutineIndex can).
// This is how we reach steam_get_user_steam_id / steam_net_packet_send etc.
static void CallExt(const std::string& rest)
{
    std::stringstream ss(rest);
    std::string name; ss >> name;
    if (name.empty()) { Out("callext: need a function name"); return; }
    std::vector<RValue> args; std::string tok;
    while (ss >> tok) args.push_back(ParseArgToken(tok));

    int idx = -1;
    AurieStatus s = g_Yytk->GetNamedRoutineIndex(name.c_str(), &idx);
    if (!AurieSuccess(s) || idx < 0) { Out("callext: '" + name + "' GetNamedRoutineIndex st=" + std::to_string((int)s)); return; }

    const YYRunnerInterface& runner = g_Yytk->GetRunnerInterface();
    if (!runner.Script_Perform) { Out("callext: runner has no Script_Perform"); return; }
    CInstance* self = nullptr; g_Yytk->GetGlobalInstance(&self);
    RValue result;
    bool ok = false;
    try {
        ok = runner.Script_Perform(idx, self, self, (int)args.size(),
                                   &result, args.empty() ? nullptr : args.data());
    } catch (...) { Out("callext '" + name + "' idx=" + std::to_string(idx) + " -> EXCEPTION"); return; }
    Out("callext " + name + " idx=" + std::to_string(idx) + " (" + std::to_string(args.size())
        + " args) ok=" + std::to_string((int)ok) + " -> " + Describe(result));
}

// ============================================================
// ===== CUSTOM CO-OP TRANSPORT (our own UDP socket) ==========
// ============================================================
// Bypasses the game's gated steam_net/herssi/PanicNet entirely.
// Each frame we read the local Player_obj state and send it to the
// peer over a raw UDP socket (Hamachi/LAN/internet, by IP:port).
// A background thread receives the peer's state into g_Remote.

#pragma pack(push, 1)
struct CoopPacket {
    uint32_t magic;     // 'HSC1'
    uint16_t version;
    uint16_t type;      // 1 = player state
    uint32_t seq;
    float    x, y;
    float    dir;
    int32_t  room;      // current room index (same-zone check, later)
    int32_t  hp;
    char     name[24];
};
#pragma pack(pop)
static const uint32_t COOP_MAGIC = 0x31435348u; // 'HSC1'

static std::atomic<bool> g_CoopEnabled{ false };
static std::atomic<bool> g_CoopRun{ false };
static SOCKET g_CoopSock = INVALID_SOCKET;
static sockaddr_in g_CoopPeer{};
static std::thread g_CoopRecvThread;
static std::mutex g_RemoteMtx;
static CoopPacket g_Remote{};
static std::atomic<bool> g_RemoteValid{ false };
static std::atomic<uint32_t> g_CoopSent{ 0 };
static std::atomic<uint32_t> g_CoopRecvCount{ 0 };
static std::atomic<uint32_t> g_CoopSeq{ 0 };
static std::string g_CoopStatus = "off";
static char g_CoopName[24] = { 0 };

static void CoopRecvLoop()
{
    while (g_CoopRun.load()) {
        CoopPacket pkt{};
        sockaddr_in from{}; int fromlen = sizeof(from);
        int r = recvfrom(g_CoopSock, (char*)&pkt, sizeof(pkt), 0, (sockaddr*)&from, &fromlen);
        if (r == (int)sizeof(pkt) && pkt.magic == COOP_MAGIC) {
            { std::lock_guard<std::mutex> lk(g_RemoteMtx); g_Remote = pkt; }
            g_RemoteValid.store(true);
            g_CoopRecvCount.fetch_add(1);
        } else if (r == SOCKET_ERROR) {
            int e = WSAGetLastError();
            if (e == WSAEINTR || e == WSAENOTSOCK || e == WSAEBADF) break;
            if (!g_CoopRun.load()) break;
            Sleep(2);
        }
    }
}

static void CoopStop()
{
    g_CoopRun.store(false);
    g_CoopEnabled.store(false);
    if (g_CoopSock != INVALID_SOCKET) { closesocket(g_CoopSock); g_CoopSock = INVALID_SOCKET; }
    if (g_CoopRecvThread.joinable()) g_CoopRecvThread.join();
    g_CoopStatus = "off";
    Out("coop: stopped");
}

static void CoopStart(int myPort, const std::string& peerIp, int peerPort)
{
    if (g_CoopRun.load()) { Out("coop: already running (coopstop first)"); return; }
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { Out("coop: WSAStartup failed"); return; }
    g_CoopSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_CoopSock == INVALID_SOCKET) { Out("coop: socket() failed"); return; }
    sockaddr_in local{}; local.sin_family = AF_INET; local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons((u_short)myPort);
    if (bind(g_CoopSock, (sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        Out("coop: bind(" + std::to_string(myPort) + ") failed err=" + std::to_string(WSAGetLastError()));
        closesocket(g_CoopSock); g_CoopSock = INVALID_SOCKET; return;
    }
    g_CoopPeer = sockaddr_in{};
    g_CoopPeer.sin_family = AF_INET;
    g_CoopPeer.sin_port = htons((u_short)peerPort);
    inet_pton(AF_INET, peerIp.c_str(), &g_CoopPeer.sin_addr);
    g_RemoteValid.store(false);
    g_CoopSent.store(0); g_CoopRecvCount.store(0);
    g_CoopRun.store(true);
    g_CoopEnabled.store(true);
    g_CoopRecvThread = std::thread(CoopRecvLoop);
    g_CoopStatus = "ON my:" + std::to_string(myPort) + " -> " + peerIp + ":" + std::to_string(peerPort);
    Out("coop: started " + g_CoopStatus);
}

// coop.ini: my_port= / peer_ip= / peer_port= / enabled= / render= / puppet=
static bool LoadCoopConfigAndMaybeStart()
{
    std::ifstream f(IPC_DIR + "\\coop.ini");
    if (!f) { Out("coop: no coop.ini at " + IPC_DIR); return false; }
    std::string line; int enabled = 0, myPort = 0, peerPort = 0, render = 0; std::string peerIp, puppet;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Lower(line.substr(0, eq)), v = line.substr(eq + 1);
        while (!k.empty() && std::isspace((unsigned char)k.front())) k.erase(k.begin());
        while (!k.empty() && std::isspace((unsigned char)k.back())) k.pop_back();
        while (!v.empty() && (std::isspace((unsigned char)v.back()) || v.back() == '\r')) v.pop_back();
        while (!v.empty() && std::isspace((unsigned char)v.front())) v.erase(v.begin());
        try {
            if (k == "my_port") myPort = std::stoi(v);
            else if (k == "peer_port") peerPort = std::stoi(v);
            else if (k == "peer_ip") peerIp = v;
            else if (k == "enabled") enabled = std::stoi(v);
            else if (k == "render") render = std::stoi(v);
            else if (k == "puppet") puppet = v;
        } catch (...) {}
    }
    if (enabled == 1 && myPort > 0 && peerPort > 0 && !peerIp.empty()) {
        if (!puppet.empty()) { g_PuppetObjName = puppet; g_PuppetObjIdx = -1; }
        CoopStart(myPort, peerIp, peerPort);
        if (render == 1) { g_CoopRender.store(true); Out("coop: auto-render ON"); }
        return true;
    }
    Out("coop: coop.ini present but not enabled/complete (enabled=" + std::to_string(enabled) + ")");
    return false;
}

// Per-frame on the frame thread: read local player, send to peer.
static void CoopTick()
{
    if (!g_CoopEnabled.load() || g_CoopSock == INVALID_SOCKET) return;
    try {
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue pid  = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        if (pid.ToDouble() < 0) return;
        RValue px = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("x") });
        RValue py = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("y") });
        CoopPacket pkt{};
        pkt.magic = COOP_MAGIC; pkt.version = 1; pkt.type = 1;
        pkt.seq = g_CoopSeq.fetch_add(1);
        pkt.x = (float)px.ToDouble(); pkt.y = (float)py.ToDouble();
        if (g_CoopName[0] == 0) {
            try {
                RValue pn = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("name") });
                if (pn.m_Kind == VALUE_STRING) { std::string s = pn.ToString(); strncpy_s(g_CoopName, s.c_str(), 23); }
            } catch (...) {}
        }
        memcpy(pkt.name, g_CoopName, 24);
        sendto(g_CoopSock, (const char*)&pkt, sizeof(pkt), 0, (sockaddr*)&g_CoopPeer, sizeof(g_CoopPeer));
        g_CoopSent.fetch_add(1);
    } catch (...) {}
}

static void CoopStats()
{
    bool rv = g_RemoteValid.load();
    CoopPacket r{};
    if (rv) { std::lock_guard<std::mutex> lk(g_RemoteMtx); r = g_Remote; }
    char b[320];
    sprintf_s(b, "coopstats: %s | sent=%u recv=%u remoteValid=%d",
        g_CoopStatus.c_str(), g_CoopSent.load(), g_CoopRecvCount.load(), (int)rv);
    Out(b);
    if (rv) { sprintf_s(b, "  remote: x=%.1f y=%.1f seq=%u name='%.23s'", r.x, r.y, r.seq, r.name); Out(b); }
}

// ----- Remote "puppet" rendering: spawn an avatar at the peer's coords -----
// g_CoopRender, g_PuppetObjName, g_PuppetObjIdx, g_PuppetId are all declared earlier.
static CInstance* g_PuppetInst = nullptr;   // puppet CInstance* (to block its self-destruct)
static bool g_KeepPuppetAlive = true;       // block instance_destroy on the puppet (stops respawn-blink)
static volatile long g_DestroyBlocked = 0;

// Hook instance_destroy: if the target is our puppet, block it (keep it alive, no respawn-blink).
static TRoutine g_OrigInstDestroy = nullptr;
static void HookInstanceDestroy(RValue& Result, CInstance* Self, CInstance* Other, int argc, RValue* Args)
{
    if (g_KeepPuppetAlive && g_PuppetId >= 0) {
        bool isPuppet = false;
        if (argc == 0 && Self && Self == g_PuppetInst) isPuppet = true;       // instance_destroy() -> self
        if (!isPuppet && argc >= 1) {
            double q = -1.0; try { q = Args[0].ToDouble(); } catch (...) {}
            if (q == g_PuppetId) isPuppet = true;                             // instance_destroy(id)
        }
        if (isPuppet) { InterlockedIncrement(&g_DestroyBlocked); return; }    // skip destruction
    }
    if (g_OrigInstDestroy) g_OrigInstDestroy(Result, Self, Other, argc, Args);
}

// Hook TalentUse (skill/attack execution): skip it when the caster is our puppet,
// so the puppet doesn't mirror the local player's skills. Movement is masked by position-override.
static PFUNC_YYGMLScript g_OrigTalentUse = nullptr;
static bool g_BlockPuppetSkills = true;
static CInstance* g_CompInst = nullptr;   // companion body (also skill-blocked)
static RValue& HookTalentUse(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    // Block skills for any instance marked coop_puppet=1 (the co-op puppet AND the companion).
    if (g_BlockPuppetSkills && S) {
        if (S == g_PuppetInst || S == g_CompInst) return R;   // fast path
        try {
            RValue inst = RValue(S);
            RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { inst, RValue("coop_puppet") });
            if (ex.ToBoolean()) {
                RValue v = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("coop_puppet") });
                if (v.ToDouble() == 1.0) return R;
            }
        } catch (...) {}
    }
    return g_OrigTalentUse ? g_OrigTalentUse(S, O, R, argc, A) : R;
}

// Hook PlayerMovement: skip for the puppet so it doesn't try to walk from local input
// (it stays at the network-forced position smoothly, no tug-of-war).
static PFUNC_YYGMLScript g_OrigPlayerMove = nullptr;
static RValue& HookPlayerMovement(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_BlockPuppetSkills && g_PuppetInst && S == g_PuppetInst) return R;  // puppet: no self-movement
    return g_OrigPlayerMove ? g_OrigPlayerMove(S, O, R, argc, A) : R;
}

static void CoopClearPuppet()
{
    double id = g_PuppetId;
    g_PuppetId = -1.0;          // clear first so the destroy hook won't block this one
    g_PuppetInst = nullptr;
    if (id >= 0) {
        try {
            RValue ex = g_Yytk->CallBuiltin("instance_exists", { RValue(id) });
            if (ex.ToBoolean()) g_Yytk->CallBuiltin("instance_destroy", { RValue(id) });
        } catch (...) {}
    }
}

// Frame thread: ensure puppet exists at remote pos, force its x/y each frame.
static void CoopRenderTick()
{
    if (!g_CoopRender.load() || !g_RemoteValid.load()) return;
    CoopPacket r;
    { std::lock_guard<std::mutex> lk(g_RemoteMtx); r = g_Remote; }
    // lazily install the destroy-block hook so the puppet stays alive (no respawn-blink)
    static bool s_destroyHook = false;
    if (!s_destroyHook) {
        s_destroyHook = true;
        try { HookBuiltin("instance_destroy", "bp_instdestroy", (PVOID)HookInstanceDestroy, &g_OrigInstDestroy); } catch (...) {}
        try { HookOneScript("TalentUse", "bp_talentuse", (PVOID)HookTalentUse, &g_OrigTalentUse); } catch (...) {}
        // NOTE: blocking PlayerMovement entirely crashes (it also does collision/depth/state) -> NOT hooked.
    }
    try {
        // only render while we're actually in a game (a local Player_obj exists)
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue pid  = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        if (pid.ToDouble() < 0) { CoopClearPuppet(); return; }

        bool exists = false;
        if (g_PuppetId >= 0) {
            RValue ex = g_Yytk->CallBuiltin("instance_exists", { RValue(g_PuppetId) });
            exists = ex.ToBoolean();
        }
        if (!exists) {
            if (g_PuppetObjIdx < 0) {
                RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(g_PuppetObjName) });
                g_PuppetObjIdx = (int)oi.ToDouble();
            }
            if (g_PuppetObjIdx < 0) return; // bad object name
            RValue nid = g_Yytk->CallBuiltin("instance_create_depth",
                { RValue((double)r.x), RValue((double)r.y), RValue(-100.0), RValue((double)g_PuppetObjIdx) });
            g_PuppetId = nid.ToDouble();
            g_PuppetInst = nullptr;
            try { g_Yytk->GetInstanceObject((int32_t)g_PuppetId, g_PuppetInst); } catch (...) {}
            // mark the puppet so the IsMyPlayer hook can identify it (-> not-my-player, no local input)
            try { g_Yytk->CallBuiltin("variable_instance_set", { nid, RValue("coop_puppet"), RValue(1.0) }); } catch (...) {}
        }
        if (g_PuppetId >= 0) {
            g_Yytk->CallBuiltin("variable_instance_set", { RValue(g_PuppetId), RValue("x"), RValue((double)r.x) });
            g_Yytk->CallBuiltin("variable_instance_set", { RValue(g_PuppetId), RValue("y"), RValue((double)r.y) });
        }
    } catch (...) {}
}

// ============================================================
// ===== COMPANION: follower body + loot/gold/reveal buffs ====
// ============================================================
static std::atomic<bool> g_CompActive{ false };
static double g_CompId = -1.0;
static std::string g_CompObjName = "Player_obj";
static int g_CompObjIdx = -1;

// Beneficial "buffs" granted while the companion is out (all proven-safe multiplier hooks).
static void CompSetBuffs(bool on)
{
    ForgePact::DropManager::Instance().SetDropItemMultRaw(on ? 3 : 1);
    ForgePact::DropManager::Instance().SetDropItemBossMultRaw(on ? 3 : 1);
    ForgePact::DropManager::Instance().SetDropGoldMultRaw(on ? 3 : 1);
    g_mult_DropRelic     = on ? 2 : 1;
    ForgePact::DropManager::Instance().SetDropBossGemsMultRaw(on ? 2 : 1);
    ForgePact::DropManager::Instance().SetDropDungeonKeysMultRaw(on ? 2 : 1);
    ForgePact::MapRevealManager::Instance().SetEnabled(on);
}
static void CompDespawn()
{
    double id = g_CompId; g_CompId = -1.0; g_CompInst = nullptr;
    if (id >= 0) {
        try { RValue ex = g_Yytk->CallBuiltin("instance_exists", { RValue(id) });
              if (ex.ToBoolean()) g_Yytk->CallBuiltin("instance_destroy", { RValue(id) }); } catch (...) {}
    }
}
// Combat AI: chase the nearest enemy and drain its enemy_hp; follow the player when none.
static int    g_CompDamage = 600;     // hp drained per attack tick
static double g_CompRange  = 200.0;   // attack range
static double g_CompSpeed  = 9.0;     // move px/frame
static int    g_CompAtkEvery = 12;    // attack every N frames

static void CompTick()
{
    if (!g_CompActive.load()) return;
    static int cframe = 0; cframe++;
    try {
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue pid  = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        if (pid.ToDouble() < 0) { CompDespawn(); return; }   // not in a game
        double pxv = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("x") }).ToDouble();
        double pyv = g_Yytk->CallBuiltin("variable_instance_get", { pid, RValue("y") }).ToDouble();

        // ensure body exists
        bool exists = false;
        if (g_CompId >= 0) { RValue ex = g_Yytk->CallBuiltin("instance_exists", { RValue(g_CompId) }); exists = ex.ToBoolean(); }
        if (!exists) {
            if (g_CompObjIdx < 0) { RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(g_CompObjName) }); g_CompObjIdx = (int)oi.ToDouble(); }
            if (g_CompObjIdx < 0) return;
            RValue nid = g_Yytk->CallBuiltin("instance_create_depth", { RValue(pxv - 70.0), RValue(pyv), RValue(-50.0), RValue((double)g_CompObjIdx) });
            g_CompId = nid.ToDouble(); g_CompInst = nullptr;
            try { g_Yytk->GetInstanceObject((int32_t)g_CompId, g_CompInst); } catch (...) {}
            try { g_Yytk->CallBuiltin("variable_instance_set", { nid, RValue("coop_puppet"), RValue(1.0) }); } catch (...) {}  // mark -> skills blocked if player body
            try { g_Yytk->CallBuiltin("variable_instance_set", { nid, RValue("mouseDisable"), RValue(1.0) }); } catch (...) {}
            return;
        }

        double cxv = g_Yytk->CallBuiltin("variable_instance_get", { RValue(g_CompId), RValue("x") }).ToDouble();
        double cyv = g_Yytk->CallBuiltin("variable_instance_get", { RValue(g_CompId), RValue("y") }).ToDouble();

        // find nearest enemy to the companion
        double tx = pxv - 70.0, ty = pyv;   // default: follow the player
        if (g_EnemyParentIdx >= 0) {
            RValue ne = g_Yytk->CallBuiltin("instance_nearest", { RValue(cxv), RValue(cyv), RValue((double)g_EnemyParentIdx) });
            double neId = ne.ToDouble();
            if (neId >= 0) {
                RValue exi = g_Yytk->CallBuiltin("instance_exists", { RValue(neId) });
                if (exi.ToBoolean()) {
                    double exv = g_Yytk->CallBuiltin("variable_instance_get", { RValue(neId), RValue("x") }).ToDouble();
                    double eyv = g_Yytk->CallBuiltin("variable_instance_get", { RValue(neId), RValue("y") }).ToDouble();
                    // only engage if the enemy is reasonably near the player (don't wander off-screen)
                    double pedx = exv - pxv, pedy = eyv - pyv;
                    if (sqrt(pedx*pedx + pedy*pedy) < 700.0) {
                        double edx = exv - cxv, edy = eyv - cyv;
                        double edist = sqrt(edx*edx + edy*edy);
                        if (edist <= g_CompRange) {
                            // in range -> drain the enemy's hp (its own Step kills it at <=0, dropping loot)
                            if (cframe % g_CompAtkEvery == 0) {
                                RValue hp = g_Yytk->CallBuiltin("variable_instance_get", { RValue(neId), RValue("enemy_hp") });
                                g_Yytk->CallBuiltin("variable_instance_set", { RValue(neId), RValue("enemy_hp"), RValue(hp.ToDouble() - (double)g_CompDamage) });
                            }
                            tx = cxv; ty = cyv;   // hold near the enemy
                        } else {
                            tx = exv; ty = eyv;   // chase the enemy
                        }
                    }
                }
            }
        }
        // step toward target at companion speed
        double mdx = tx - cxv, mdy = ty - cyv;
        double md = sqrt(mdx*mdx + mdy*mdy);
        double nx = cxv, ny = cyv;
        if (md > g_CompSpeed) { nx = cxv + (mdx/md) * g_CompSpeed; ny = cyv + (mdy/md) * g_CompSpeed; }
        else { nx = tx; ny = ty; }
        // if it ever falls way behind, snap near the player
        double pdx = nx - pxv, pdy = ny - pyv;
        if (sqrt(pdx*pdx + pdy*pdy) > 900.0) { nx = pxv - 70.0; ny = pyv; }
        g_Yytk->CallBuiltin("variable_instance_set", { RValue(g_CompId), RValue("x"), RValue(nx) });
        g_Yytk->CallBuiltin("variable_instance_set", { RValue(g_CompId), RValue("y"), RValue(ny) });
    } catch (...) {}
}

// ----- Nth-instance access (for inspecting/controlling the puppet = Player_obj[1]) -----
static RValue NthInstance(const std::string& objName, int n)
{
    RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(objName) });
    if (oi.ToDouble() < 0) return RValue(-4.0);
    return g_Yytk->CallBuiltin("instance_find", { oi, RValue((double)n) });
}
static void NiGet(const std::string& obj, int n, const std::string& var)
{
    try {
        RValue id = NthInstance(obj, n);
        if (id.ToDouble() < 0) { Out("niget: no " + obj + "[" + std::to_string(n) + "]"); return; }
        RValue ex = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue(var) });
        if (!ex.ToBoolean()) { Out("niget " + obj + "[" + std::to_string(n) + "]." + var + " -> (no var)"); return; }
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(var) });
        Out("niget " + obj + "[" + std::to_string(n) + "]." + var + " -> " + Describe(v));
    } catch (...) { Out("niget EXCEPTION"); }
}
static void NiSet(const std::string& obj, int n, const std::string& var, double val)
{
    try {
        RValue id = NthInstance(obj, n);
        if (id.ToDouble() < 0) { Out("niset: no " + obj + "[" + std::to_string(n) + "]"); return; }
        g_Yytk->CallBuiltin("variable_instance_set", { id, RValue(var), RValue(val) });
        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue(var) });
        Out("niset " + obj + "[" + std::to_string(n) + "]." + var + " = " + std::to_string(val) + " -> " + Describe(v));
    } catch (...) { Out("niset EXCEPTION"); }
}
// nicall <Script> <obj> <n> -- call gml_Script_<Script> passing the Nth instance id as its arg
// --- Ozel icerik oranlari --------------------------------------------------
// Dogrulanmis tarif: paylasilan eSt kapisini ac, marker'i n kez yarat.
// Marker ADLA cozulur; indeksler oyun guncellemesiyle kayar.
// gateHook: mekanigin activate fonksiyonu kalici "bir kere" bayraklari okur;
// bunlari cogaltilan her kopya icin sifirlayan kanca gerekir (asagida).
struct SpecialContent { const char* key; const char* obj; int estSlot; double estVal; bool gateHook; };
static const SpecialContent kSpecial[] = {
    { "rift",         "Spawn_Rift_obj",          -1, 0.0,   false },
    { "battlefield",  "Spawn_Battlefield_obj",   -1, 0.0,   false },
    { "cursedorb",    "Spawn_Cursed_Orb_obj",     7, 14.0,  false },
    { "summonportal", "Spawn_Summon_Portal_obj", -1, 0.0,   false },
    { "chaospillars", "Spawn_Chaos_Pillars_obj", -1, 0.0,   false },
    // Chaos Tower: sans = taban(zorluk) + eSt[6]; zar random(zrmb) < floor(sans)*100.
    // Shadow Realm: sans = 13 + eSt[9]; ayni zar.  eSt degeri sansi %100'e tamamlar.
    { "chaostower",   "Spawn_Chaos_Tower_obj",    6, 100.0, true  },
    { "shadowrealm",  "Spawn_Shadow_Realm_obj",   9, 87.0,  true  },
};

// --- Shadow Realm / Chaos Tower "bir kere" kapilari ------------------------
// Statik cozumleme (decompile, 2026-09-03):
//   anon@119@gml_Object_Spawn_Shadow_Realm_obj_Create_0 (m_activateMechanic)
//     GPV(gDataProtected[68]) >= 2            (zorluk kapisi)
//     Controller_obj.shadowRealmSpawned == 0  (Portal_Shadow_Realm_obj Create true yapar,
//                                              yalnizca kosu sifirlaninca geri doner)
//     eSt[0] <= 0 ; random(zrmb) < (13 + max(eSt[9],0)) * 100 ; sCP(Portal_Shadow_Realm_obj)
//   anon@97@gml_Object_Spawn_Chaos_Tower_obj_Create_0 (m_activateMechanic)
//     GPV(gDataProtected[68]) >= 1            (zorluk; 0 iken zar atilmiyor - canli olculdu)
//     GPV(Controller_obj.chaosTowerStarted) == 0
//     GPV(Controller_obj.chaosTowerSpawnZone) == -1   (spawn sonrasi = oda; m_ChaosTowerReset -1 yapar)
//     eSt[0] <= 0 ; codex/buff ; sans = taban(GPV 68, GPV 251) + eSt[6] ; RunningHost()
//     -> instance_create_layer(Chaos_Tower_obj) ; SPV(chaosTowerSpawnZone, room)
// Marker'i cogaltmak tek basina yetmez: ilk kopya bayragi kapatir, digerleri
// sessizce cikar.  Bu kancalar, ozellik ACIKKEN (marker carpani > 1) her
// activate cagrisindan hemen once bayraklari sifirlar; yer secimi, zar, ag
// paketi ve nesne yaratimi oyunun kendi kodunda kalir.  Ozellik kapaliyken
// kancalar dokunmadan gecer.
static PFUNC_YYGMLScript g_Orig_ShadowRealmGate = nullptr;
static PFUNC_YYGMLScript g_Orig_ChaosTowerGate  = nullptr;
static bool g_MechGateHooksInstalled = false;
static int  g_ShadowRealmMarkerIdx = -2;   // -2 = henuz cozulmedi
static int  g_ChaosTowerMarkerIdx  = -2;
static int  g_ControllerObjIdx     = -2;
static long g_SrGateCalls = 0, g_SrGateOpened = 0, g_SrDiffForced = 0;
static long g_CtGateCalls = 0, g_CtGateOpened = 0, g_CtDiffForced = 0;
// Zorluk kapilari (canli olculdu 2026-09-03, zrmb = 9999):
//   Shadow Realm : GPV(gDataProtected[68]) >= 2
//   Chaos Tower  : GPV(gDataProtected[68]) >= 1 (0 iken zar hic atilmiyor)
// Acikken deger yalnizca activate cagrisi suresince esige cekilir, cagri
// biter bitmez eski deger geri yazilir.
static bool g_SrAnyDifficulty = true;
static bool g_CtAnyDifficulty = true;

// gDataProtected[68] icin gecici asgari deger.  Arm() esigin altindaysa yazar,
// Restore() eski degeri geri koyar; ikisi de hata firlatmaz.
static bool RValueIsNumber(const RValue& v);
struct DifficultyGateForce
{
    RValue handle; double old = 0.0; bool active = false;
    void Arm(double minValue)
    {
        active = false;
        try {
            RValue arr = g_Yytk->CallBuiltin("variable_global_get", { RValue("gDataProtected") });
            handle = g_Yytk->CallBuiltin("array_get", { arr, RValue(68.0) });
            RValue cur = g_Yytk->CallGameScript("gml_Script_GPV", { handle });
            if (!RValueIsNumber(cur)) return;
            old = cur.ToDouble();
            if (old < minValue) {
                g_Yytk->CallGameScript("gml_Script_SPV", { handle, RValue(minValue) });
                active = true;
            }
        } catch (...) { active = false; }
    }
    void Restore()
    {
        if (!active) return;
        active = false;
        try { g_Yytk->CallGameScript("gml_Script_SPV", { handle, RValue(old) }); } catch (...) {}
    }
};

static int CachedObjectIndex(int& cache, const char* name)
{
    if (cache != -2) return cache;
    cache = -1;
    try {
        RValue r = g_Yytk->CallBuiltin("asset_get_index", { RValue(name) });
        cache = (int)r.ToDouble();
    } catch (...) {}
    return cache;
}

static bool SpecialMultiplierOn(int objIdx)
{
    if (objIdx < 0) return false;
    auto it = g_ObjMult.find(objIdx);
    return it != g_ObjMult.end() && it->second > 1;
}

static bool RValueIsNumber(const RValue& v)
{
    return v.m_Kind == VALUE_REAL || v.m_Kind == VALUE_INT32 ||
           v.m_Kind == VALUE_INT64 || v.m_Kind == VALUE_BOOL;
}

// Ilk Controller_obj ornegi (oyun bayraklari orada tutar). Bulunamazsa false.
static bool FindControllerInstance(RValue& out)
{
    int ci = CachedObjectIndex(g_ControllerObjIdx, "Controller_obj");
    if (ci < 0) return false;
    out = g_Yytk->CallBuiltin("instance_find", { RValue((double)ci), RValue(0.0) });
    return out.ToDouble() >= 0.0;      // noone = -4
}

static RValue& Hook_ShadowRealmGate(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    ++g_SrGateCalls;
    DifficultyGateForce diff;
    if (SpecialMultiplierOn(CachedObjectIndex(g_ShadowRealmMarkerIdx, "Spawn_Shadow_Realm_obj"))) {
        try {
            RValue ctrl;
            if (FindControllerInstance(ctrl)) {
                g_Yytk->CallBuiltin("variable_instance_set",
                    { ctrl, RValue("shadowRealmSpawned"), RValue(0.0) });
                ++g_SrGateOpened;
            }
            if (g_SrAnyDifficulty) {
                diff.Arm(2.0);
                if (diff.active) ++g_SrDiffForced;
            }
            if (g_SrGateOpened <= 3)
                Out("srgate: shadowRealmSpawned=0" + std::string(diff.active ? ", zorluk gecici 2" : ""));
        } catch (...) { Out("srgate: EXCEPTION (kapi acilamadi)"); }
    }
    RValue& r = g_Orig_ShadowRealmGate ? g_Orig_ShadowRealmGate(S, O, R, argc, A) : R;
    diff.Restore();
    return r;
}

static RValue& Hook_ChaosTowerGate(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    ++g_CtGateCalls;
    DifficultyGateForce diff;
    if (SpecialMultiplierOn(CachedObjectIndex(g_ChaosTowerMarkerIdx, "Spawn_Chaos_Tower_obj"))) {
        try {
            RValue ctrl;
            if (FindControllerInstance(ctrl)) {
                RValue hZone    = g_Yytk->CallBuiltin("variable_instance_get", { ctrl, RValue("chaosTowerSpawnZone") });
                RValue hStarted = g_Yytk->CallBuiltin("variable_instance_get", { ctrl, RValue("chaosTowerStarted") });
                if (RValueIsNumber(hZone))    g_Yytk->CallGameScript("gml_Script_SPV", { hZone,    RValue(-1.0) });
                if (RValueIsNumber(hStarted)) g_Yytk->CallGameScript("gml_Script_SPV", { hStarted, RValue(0.0) });
                ++g_CtGateOpened;
            }
            if (g_CtAnyDifficulty) {
                diff.Arm(1.0);
                if (diff.active) ++g_CtDiffForced;
            }
            if (g_CtGateOpened <= 3)
                Out("ctgate: chaosTowerSpawnZone=-1 chaosTowerStarted=0"
                    + std::string(diff.active ? ", zorluk gecici 1" : ""));
        } catch (...) { Out("ctgate: EXCEPTION (kapi acilamadi)"); }
    }
    RValue& r = g_Orig_ChaosTowerGate ? g_Orig_ChaosTowerGate(S, O, R, argc, A) : R;
    diff.Restore();
    return r;
}

static void InstallMechGateHooks()
{
    if (g_MechGateHooksInstalled) return;
    g_MechGateHooksInstalled = true;
    // anon@N = fonksiyonun Create kaynagindaki karakter ofseti; oyun guncellemesi
    // Create olayini degistirirse ad kayar, HookOneScript "not found" yazar ve
    // icerik vanilya davranisina (tek spawn) duser.
    HookOneScript("anon@119@gml_Object_Spawn_Shadow_Realm_obj_Create_0",
                  "fp_sr_gate", (PVOID)Hook_ShadowRealmGate, &g_Orig_ShadowRealmGate);
    HookOneScript("anon@97@gml_Object_Spawn_Chaos_Tower_obj_Create_0",
                  "fp_ct_gate", (PVOID)Hook_ChaosTowerGate, &g_Orig_ChaosTowerGate);
}

static void MechGateStats()
{
    Out("gatestats: kanca=" + std::string(g_MechGateHooksInstalled ? "kurulu" : "yok")
        + " sr(hook=" + std::string(g_Orig_ShadowRealmGate ? "ok" : "-")
        + " cagri=" + std::to_string(g_SrGateCalls) + " acildi=" + std::to_string(g_SrGateOpened)
        + " zorlukZorlandi=" + std::to_string(g_SrDiffForced) + ")"
        + " ct(hook=" + std::string(g_Orig_ChaosTowerGate ? "ok" : "-")
        + " cagri=" + std::to_string(g_CtGateCalls) + " acildi=" + std::to_string(g_CtGateOpened)
        + " zorlukZorlandi=" + std::to_string(g_CtDiffForced) + ")"
        + " srAnyDifficulty=" + std::string(g_SrAnyDifficulty ? "on" : "off")
        + " ctAnyDifficulty=" + std::string(g_CtAnyDifficulty ? "on" : "off"));
    try {
        RValue ctrl;
        if (FindControllerInstance(ctrl)) {
            RValue sr = g_Yytk->CallBuiltin("variable_instance_get", { ctrl, RValue("shadowRealmSpawned") });
            RValue hZone    = g_Yytk->CallBuiltin("variable_instance_get", { ctrl, RValue("chaosTowerSpawnZone") });
            RValue hStarted = g_Yytk->CallBuiltin("variable_instance_get", { ctrl, RValue("chaosTowerStarted") });
            RValue zone    = g_Yytk->CallGameScript("gml_Script_GPV", { hZone });
            RValue started = g_Yytk->CallGameScript("gml_Script_GPV", { hStarted });
            RValue arr = g_Yytk->CallBuiltin("variable_global_get", { RValue("gDataProtected") });
            RValue h68 = g_Yytk->CallBuiltin("array_get", { arr, RValue(68.0) });
            RValue diff = g_Yytk->CallGameScript("gml_Script_GPV", { h68 });
            Out("  Controller: shadowRealmSpawned=" + Describe(sr)
                + " chaosTowerSpawnZone=" + Describe(zone) + " chaosTowerStarted=" + Describe(started)
                + " | GPV68=" + Describe(diff));
        } else Out("  Controller_obj yok");
    } catch (...) { Out("  gatestats: okuma EXCEPTION"); }
}

static void SpecialRate(const std::string& key, int n)
{
    const SpecialContent* sc = nullptr;
    for (const auto& e : kSpecial) if (key == e.key) { sc = &e; break; }
    if (!sc) { Out("specialrate: bilinmeyen icerik '" + key + "'"); return; }
    if (n < 1) n = 1;
    if (n > 1) {
        InstallCreateHooks();
        if (sc->gateHook) InstallMechGateHooks();
    }
    try {
        RValue idx = g_Yytk->CallBuiltin("asset_get_index", { RValue(sc->obj) });
        int oi = (int)idx.ToDouble();
        if (oi < 0) { Out(std::string("specialrate: ") + sc->obj + " bulunamadi"); return; }
        // Vanilla entries do not belong in the instance-create hot-path map.
        // Keeping six x1 entries made every object creation perform a tree
        // lookup even when Special Content was completely disabled.
        if (n > 1) SetObjectMultiplier(oi, n);
        else {
            SetObjectMultiplier(oi, 1);
            // Off means Off immediately: discard copies that this marker had
            // already scheduled instead of draining them after the UI changed.
            KuyruktanNesneyiSil(oi);
        }
        if (n > 1) {
            g_EstForce[0] = 0.0;                                  // paylasilan kapi
            if (sc->estSlot >= 0) g_EstForce[sc->estSlot] = sc->estVal;
        } else {
            if (sc->estSlot >= 0) g_EstForce.erase(sc->estSlot);
            bool anySpecialEnabled = false;
            for (const auto& entry : g_ObjMult) {
                if (entry.second > 1) { anySpecialEnabled = true; break; }
            }
            if (!anySpecialEnabled) g_EstForce.erase(0);
        }
        Out("specialrate " + key + " -> " + std::to_string(n)
            + " (" + sc->obj + " idx " + std::to_string(oi) + ")");
    } catch (...) { Out("specialrate: EXCEPTION"); }
}


#ifndef FORGEPACT_RELEASE
// --- Cokme teshisi: periyodik ornek sayimi (gelistirme derlemesi) ----------
static bool g_CensusOn = false;
static int  g_CensusEvery = 120;          // kare (60 fps'te ~2 sn)

static void CensusTick(unsigned long long fc)
{
    if (!g_CensusOn || !g_Yytk) return;
    if (g_CensusEvery <= 0 || (fc % (unsigned long long)g_CensusEvery) != 0) return;
    try {
        // GML'de `all` = -3
        RValue all = g_Yytk->CallBuiltin("instance_number", { RValue(-3.0) });
        double monsters = -1; try { if (g_EnemyParentIdx >= 0) monsters = g_Yytk->CallBuiltin("instance_number", { RValue((double)g_EnemyParentIdx) }).ToDouble(); } catch (...) {}
        std::ofstream f(IPC_DIR + "\\census.txt", std::ios::app);
        f << "kare=" << fc << "  TOPLAM=" << (long long)all.ToDouble() << "  CANAVAR=" << (long long)monsters;
        for (const auto& e : kSpecial) {
            RValue idx = g_Yytk->CallBuiltin("asset_get_index", { RValue(e.obj) });
            int oi = (int)idx.ToDouble();
            if (oi < 0) continue;
            RValue n = g_Yytk->CallBuiltin("instance_number", { RValue((double)oi) });
            f << "  " << e.key << "=" << (int)n.ToDouble();
        }
        f << "\n";
        f.flush();
    } catch (...) {}
}
#endif



// --- Esya dusus sansi (droprate) -------------------------------------------
// GetNormalRepoStruct(kategori, 0, indeks) esyanin tanim struct'ini donduruyor.
// Icindeki `droprate.base` sayisi ne kadar BUYUKSE o esya o kadar NADIR:
//   keys_key 100 | keys_crystal_key 400 | keys_chaos_key 2700 | keys_bifrost_key 32750
// Bu alan yazilabilir; cagriyi cogaltmak yerine zarin kendisini degistiriyoruz.
static int g_DropRateKat = 12;   // 12 = Keys

// --- Blood Pact aileleri: ada gore eslesen genel gruplar -------------------
// Eski gruplar sabit indeks listesi tasiyordu (bifrost={2} gibi).  Yeni
// aileler onlarca esya iceriyor ve indeksleri oyun guncellemesinde kayiyor,
// o yuzden ADA gore esliyoruz - isimler surumler arasi sabit.
//
// tip = LoadDrops damla tipi (2026-08-27 taramasinda olculdu).
// tip -1 = o ailenin dis kapisi zaten acik, yalnizca ic zar carpilir.
struct DropGrup {
    const char* ad;
    int         kategori;
    const char* parcalar;   // virgulle ayrilmis; esya adi bunlardan birini ICERIYORSA sayilir
    int         tip;
};
static const DropGrup kDropGruplar[] = {
    // Sondaki '$' = ad bununla BITMELI.  Duz icerme yetmiyordu:
    //   "_rune" -> socketable_orb_of_runeforge'u da yakaliyordu (o bir orb)
    //   "satanic" -> 6 tane material_salvage_*_satanic_dust'i da yakaliyordu
    { "rune",       15, "_rune$",                                                    4 },
    { "orb",        15, "socketable_orb",                                           37 },
    { "bossgem",    15, "socketable_gem",                                           -1 },
    { "scrollofra", 13, "scroll_of_ra",                                             34 },
    { "primeevil",  13, "gurags_,deaths_,damiens_,anubis_,karp_kings_,satans_horn", 41 },
    { "dimshard",   13, "dimensional_shard",                                        43 },
    { "battlefrag", 13, "battle_fragment",                                          25 },
    { "colosfrag",  13, "colosseum_fragment",                                       38 },
    { "satanic",    14, "material_satanic_",                                        39 },
    // Ruby Key: LoadDrops type 18 (measured 2026-08-27); on normal monsters the
    // gate is natively open at a tiny chance, the real wall is base 1,500,000.
    { "ruby",       12, "keys_ruby_key",                                            18 },
    // Plain gems (chipped/flawed/flawless + the plain stone): type 6, gate open.
    // Perfect stones sit at base 50,000,000 and are not meant to drop.
    { "stone",      15, "socketable_chipped_,socketable_flawed_,socketable_flawless_,socketable_amethyst,socketable_diamond,socketable_emerald,socketable_ruby,socketable_sapphire,socketable_skull,socketable_topaz", -1 },
};

static const DropGrup* DropGrupBul(const std::string& ad)
{
    for (const auto& g : kDropGruplar) if (ad == g.ad) return &g;
    return nullptr;
}

// Adi parcalardan birini iceren esyalarin ic zarini carpar.
// Donen: dokunulan esya sayisi.  ornekEski/ornekYeni ilk esyanin degerleri.
static int DropGrupUygula(const DropGrup& g, double carpan, double& ornekEski, double& ornekYeni)
{
    std::vector<std::string> parcalar;
    {
        std::string p = g.parcalar, tek;
        std::stringstream ss(p);
        while (std::getline(ss, tek, ',')) if (!tek.empty()) parcalar.push_back(Lower(tek));
    }
    int sayac = 0, bos = 0;
    for (int i = 0; i < 260 && bos < 8; i++) {
        RValue st;
        if (!RepoStruct(g.kategori, i, st)) { bos++; continue; }
        bos = 0;
        std::string ad = Lower(RepoAd(st));
        bool uydu = false;
        for (const auto& p : parcalar) {
            if (!p.empty() && p.back() == '$') {          // sonek eslesmesi
                std::string s = p.substr(0, p.size() - 1);
                if (ad.size() >= s.size() && ad.compare(ad.size() - s.size(), s.size(), s) == 0) {
                    uydu = true; break;
                }
            } else if (ad.find(p) != std::string::npos) { // duz icerme
                uydu = true; break;
            }
        }
        if (!uydu) continue;
        try {
            RValue dr;
            double vanilya = VanilyaBase(g.kategori, i, st, dr);
            if (vanilya <= 0.0) continue;
            double yeni = (carpan <= 1.0) ? vanilya : (vanilya / carpan);
            if (yeni < 1.0) yeni = 1.0;
            g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(yeni) });
            if (ornekEski < 0.0) { ornekEski = vanilya; ornekYeni = yeni; }
            sayac++;
        } catch (...) {}
    }
    return sayac;
}

static void DropRateCmd(const std::string& rest)
{
    std::string alt, kalan; alt = FirstToken(rest, kalan);
    alt = Lower(alt);
    while (!alt.empty() && (alt.back()=='\r'||alt.back()=='\n'||alt.back()==' ')) alt.pop_back();

    if (alt == "cat") {
        try { g_DropRateKat = std::stoi(kalan); Out("droprate: kategori -> " + std::to_string(g_DropRateKat)); }
        catch (...) { Out("droprate: kullanim -> droprate cat 12"); }
        return;
    }

    if (alt == "list" || alt.empty()) {
        int kat = g_DropRateKat;
        if (!kalan.empty()) { try { kat = std::stoi(kalan); } catch (...) {} }
        Out("droprate list: kategori " + std::to_string(kat) + "  (base kucukse daha SIK duser)");
        int bos = 0;
        for (int i = 0; i < 200 && bos < 6; i++) {
            RValue st;
            if (!RepoStruct(kat, i, st)) { bos++; continue; }
            bos = 0;
            std::string ad = RepoAd(st);
            double b = -1.0;
            try {
                RValue dr = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("droprate") });
                if (dr.m_Kind == VALUE_OBJECT) {
                    RValue bv = g_Yytk->CallBuiltin("variable_struct_get", { dr, RValue("base") });
                    b = bv.ToDouble();
                }
            } catch (...) {}
            char ln[200];
            sprintf_s(ln, "   [%3d] %-34.34s base=%.0f", i, ad.c_str(), SafeF(b));
            Out(ln);
        }
        return;
    }

    if (alt == "group") {
        // Grup adi + deger.  Dungeon havuzu oyundan okunur (GetDungeonKeys).
        std::string grup, degers; grup = FirstToken(kalan, degers);
        grup = Lower(grup);
        double v = 0.0;
        try { v = std::stod(degers); } catch (...) { Out("droprate: kullanim -> droprate group dungeon 50"); return; }
        if (v <= 0.0) v = 1.0;   // 0/negatif -> vanilya

        int sayac = 0;
        double ornekEski = -1.0, ornekYeni = -1.0;

        std::vector<int> hedef;
        if (grup == "bifrost")      hedef = { 2 };
        else if (grup == "angelic") hedef = { 8 };
        else if (grup == "chaos")   hedef = { 33, 1 };   // Chaos + Crystal
        else if (grup == "basic")   hedef = { 0, 1 };
        else if (grup == "relic") {
            // Relic'ler ayri kategoride (16) ve oranlari 2.5-50 milyon arasi.
            // Kategoriyi grup kendisi tasir; g_DropRateKat degistirilmez.
            for (int i = 0; i < kSeason10RelicRepoCount; i++) {
                RValue st;
                if (!RepoStruct(16, i, st)) continue;
                try {
                    RValue dr;
                    double vanilya = VanilyaBase(16, i, st, dr);
                    if (vanilya <= 0.0) continue;
                    double yeni = (v <= 1.0) ? vanilya : (vanilya / v);
                    if (yeni < 1.0) yeni = 1.0;
                    g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(yeni) });
                    if (ornekEski < 0.0) { ornekEski = vanilya; ornekYeni = yeni; }
                    sayac++;
                } catch (...) {}
            }
            char rb[220];
            sprintf_s(rb, "droprate group relic: x%.0f, %d esya  (ornek: %.0f -> %.0f)",
                      SafeF(v), sayac, SafeF(ornekEski), SafeF(ornekYeni));
            Out(rb);
            return;
        }
        else if (grup == "dungeon") {
            try {
                RValue liste = g_Yytk->CallGameScript("gml_Script_GetDungeonKeys", {});
                RValue n = g_Yytk->CallBuiltin("array_length", { liste });
                int len = (int)n.ToDouble();
                for (int i = 0; i < len; i++) {
                    RValue e = g_Yytk->CallBuiltin("array_get", { liste, RValue((double)i) });
                    hedef.push_back((int)e.ToDouble());
                }
            } catch (...) { Out("droprate group dungeon: havuz okunamadi"); return; }
        }
        else if (const DropGrup* g = DropGrupBul(grup)) {
            // Yeni Blood Pact aileleri: ada gore eslesir, kendi kategorisini tasir.
            int n = DropGrupUygula(*g, v, ornekEski, ornekYeni);
            char gb[240];
            if (n == 0) {
                sprintf_s(gb, "droprate group %s: HIC ESYA BULUNAMADI (kategori %d, ad parcasi '%s')",
                          g->ad, g->kategori, g->parcalar);
            } else {
                sprintf_s(gb, "droprate group %s: x%.0f, %d esya  (ornek: %.0f -> %.0f)%s",
                          g->ad, SafeF(v), n, SafeF(ornekEski), SafeF(ornekYeni),
                          g->tip >= 0 ? "  [dis kapi icin: dungeonkey add " : "  [dis kapi zaten acik]");
                if (g->tip >= 0) {
                    char ek[16]; sprintf_s(ek, "%d]", g->tip);
                    strcat_s(gb, ek);
                }
            }
            Out(gb);
            return;
        }
        else {
            std::string liste = "bifrost | angelic | dungeon | basic | chaos | relic";
            for (const auto& g : kDropGruplar) liste += std::string(" | ") + g.ad;
            Out("droprate group: " + liste);
            return;
        }

        // v artik CARPAN: 1 = vanilya, 5 = 5 kat daha sik.
        for (int i : hedef) {
            RValue st;
            if (!RepoStruct(g_DropRateKat, i, st)) continue;
            try {
                RValue dr;
                double vanilya = VanilyaBase(g_DropRateKat, i, st, dr);
                if (vanilya <= 0.0) continue;
                double yeni = (v <= 1.0) ? vanilya : (vanilya / v);
                if (yeni < 1.0) yeni = 1.0;
                g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(yeni) });
                if (ornekEski < 0.0) { ornekEski = vanilya; ornekYeni = yeni; }
                sayac++;
            } catch (...) {}
        }
        char b[220];
        sprintf_s(b, "droprate group %s: x%.0f, %d esya  (ornek: %.0f -> %.0f)",
                  grup.c_str(), SafeF(v), sayac, SafeF(ornekEski), SafeF(ornekYeni));
        Out(b);
        return;
    }

    if (alt == "set") {
        std::string idxs, vals; idxs = FirstToken(kalan, vals);
        try {
            int i = std::stoi(idxs);
            double v = std::stod(vals);
            RValue st;
            if (!RepoStruct(g_DropRateKat, i, st)) { Out("droprate: [" + idxs + "] yok"); return; }
            std::string ad = RepoAd(st);
            RValue dr = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("droprate") });
            if (dr.m_Kind != VALUE_OBJECT) { Out("droprate: droprate alani yok"); return; }
            RValue eski = g_Yytk->CallBuiltin("variable_struct_get", { dr, RValue("base") });
            g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(v) });
            char ln[220];
            sprintf_s(ln, "droprate set [%d] %s : %.0f -> %.0f", i, ad.c_str(), SafeF(eski.ToDouble()), SafeF(v));
            Out(ln);
        } catch (...) { Out("droprate: kullanim -> droprate set 2 100"); }
        return;
    }

    Out("droprate: list | set <i> <mutlak> | group <ad> <carpan> | cat <kategori>");
}


// --- Zindan anahtarlari: dogal kapi ---------------------------------------
// LoadDrops case 12'nin kapisi, calisan case 11/31/40 ile bayt bayt ayni ve
// ayni zari kullaniyor.  Sorun kodda degil: normal anahtar dusuren canavarlar
// zindan anahtari tablosunu tasimiyor.  Cozum kapiyi ATLAMAK degil, ayni
// kapidan bir kez daha gecmek - zar yine oyunun zari.
static PFUNC_YYGMLScript g_OrigLoadDrops = nullptr;
static bool g_DkOn = false;
static double g_DkChance = -1.0;      // -1 = auto (canavarin kendi chances[11]'i)
static double g_DkChanceMult = 1.0;   // auto modunda dis kapi carpani
static std::set<int> g_DkTipler = { 12 };   // ek zar atilacak damla tipleri (12 = zindan anahtari)
// Tip 41 kapisi Relic ile Prime Evil parcalarini (DropBossParts / DropUberParts) paylasir.
// Relic kaydiraci acildiginda sirf relic dussun: bizim ek zarimiz surerken parca betikleri
// atlanir, oyunun kendi tip-41 zari (boss odalari) dokunulmadan kalir.
static PFUNC_YYGMLScript g_Orig_DropBossParts = nullptr, g_Orig_DropUberParts = nullptr;
static bool g_DkPartsGuard = false, g_DkPartsHooksTried = false;
static volatile long g_DkPartsSkipped = 0;
static RValue& Hook_DropBossParts(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_DkPartsGuard) { InterlockedIncrement(&g_DkPartsSkipped); return R; }
    return g_Orig_DropBossParts ? g_Orig_DropBossParts(S, O, R, argc, A) : R;
}
static RValue& Hook_DropUberParts(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    if (g_DkPartsGuard) { InterlockedIncrement(&g_DkPartsSkipped); return R; }
    return g_Orig_DropUberParts ? g_Orig_DropUberParts(S, O, R, argc, A) : R;
}
static void EnsurePartsGuardHooks()
{
    if (g_DkPartsHooksTried) return;
    g_DkPartsHooksTried = true;
    HookOneScript("DropBossParts", "fp_dk_bossparts", (PVOID)Hook_DropBossParts, &g_Orig_DropBossParts);
    HookOneScript("DropUberParts", "fp_dk_uberparts", (PVOID)Hook_DropUberParts, &g_Orig_DropUberParts);
}
// Tip basina dis-kapi carpani.  Yoksa g_DkChanceMult'e duser.
static std::map<int, double> g_DkTipCarpan;

// Tip basina OLCEK = o olumde bu tip icin zar atma OLASILIGI.
//
// OLCULDU (2026-08-28): oyunun kapisi  irandom(payda) < chances[tip]
// seklinde ve irandom TAM SAYI donduruyor.  Bu yuzden chances degerini
// 1'in altina cekmek hicbir sey yapmiyor - 0.5 de 0.01 de ayni kapiyi
// veriyor, ikisi de yalnizca irandom sifir cektiginde geciyor.
// Olculen sonuc: olcek 0.05 -> dusen esyalarin %30'u relic,
//                olcek 0.001 -> %69.  Yani hic azalmadi.
//
// Dogru cozum orani kucultmek degil, O OLUMDE ZARI HIC ATMAMAK.
// Asagidaki deger, kaydirac birimi basina zar atma olasiligi:
//     olasilik = olcek * kaydirac   (1.0'da kirpilir)
// OLCULEREK ayarlandi (dusen esyalarin yuzde kaci relic):
//     eski davranis          %30,2   (567 esyada 171 relic)
//     dogrusal 0.01,  x2      %6,4   - cok fazla
//     dogrusal 0.0005, x2     %0,7   - iyi
//     dogrusal 0.0005, x100   %4,3   - cok az
//
// Dogrusal esleme ikisini birden veremiyor: 50 katlik kaydirac araligi
// yalnizca 6 kat fark uretiyordu.  x2'yi dogru yapan deger x100'u zayif
// birakiyor, tersi de seli geri getiriyor.  Bu yuzden egri KARELI:
//     olasilik = olcek * kaydirac^2   (1.0'da kirpilir)
// 0.00025 ile:  x2 -> 0.001 (olculen iyi deger korunur)
//               x10 -> 0.025 | x20 -> 0.10 | x50 -> 0.63 | x100 -> 1.0
static std::map<int, double> g_DkTipOlcek = {
    { 41, 0.00025 },   // relic
};

// Kendi zarimiz.  Oyunun rastgele dizisine dokunmuyoruz ki diger
// damlalarin sirasi degismesin.
static unsigned long long g_ZarTohum = 88172645463325252ULL;
static double KendiZar()
{
    g_ZarTohum ^= g_ZarTohum << 13;
    g_ZarTohum ^= g_ZarTohum >> 7;
    g_ZarTohum ^= g_ZarTohum << 17;
    return (double)(g_ZarTohum >> 11) / 9007199254740992.0;   // [0,1)
}

static double TipOlcek(int tip)
{
    auto it = g_DkTipOlcek.find(tip);
    return (it == g_DkTipOlcek.end()) ? 1.0 : it->second;
}
static volatile long g_DkRolls = 0;   // ek zar atilan olum sayisi
static volatile long g_DkNative = 0;  // tip 12 zaten yerli -> dokunulmadi
static int g_DkProbe = 0;             // teshis: ilk N cagriyi kaydet
static int g_DkFullProbe = 0;         // teshis: chances dizisinin tamamini dok

#ifndef FORGEPACT_RELEASE
// typemap: bir sonraki olumde tum damla tiplerini tara (asagida anlatildi)
static bool   g_TypeMapIste = false;
static bool   g_TypeMapCalisiyor = false;
static double g_TypeMapSans = 100000.0;

// Tarama sirasinda esyalarin IC zarini da acmak icin.
//
// Neden gerekli: chances[t]=100000 yalnizca DIS kapiyi aciyor.  Kapi gecse bile
// esyanin kendi droprate.base'i geciyor - rune 350..334800, orb 11000+.  Tek
// denemede tutma sansi binde bir, o yuzden gecerli tipler bile "hicbir sey
// uretmedi" gorunuyordu.  base=1 yapinca gecerli her tip ilk denemede urun verir.
static bool g_TypeMapTumOranlar = false;

static void TumOranlariAc(std::vector<std::pair<int, int>>& dokunulan)
{
    for (int kat = 0; kat <= 19; kat++) {
        int bos = 0;
        for (int i = 0; i < 250 && bos < 6; i++) {
            RValue st;
            if (!RepoStruct(kat, i, st)) { bos++; continue; }
            bos = 0;
            try {
                RValue dr;
                double vanilya = VanilyaBase(kat, i, st, dr);   // vanilyayi saklar
                if (vanilya <= 0.0) continue;
                g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(1.0) });
                dokunulan.push_back({ kat, i });
            } catch (...) {}
        }
    }
}

static void OranlariGeriAl(const std::vector<std::pair<int, int>>& dokunulan)
{
    for (const auto& p : dokunulan) {
        RValue st;
        if (!RepoStruct(p.first, p.second, st)) continue;
        try {
            RValue dr;
            double vanilya = VanilyaBase(p.first, p.second, st, dr);
            if (vanilya > 0.0)
                g_Yytk->CallBuiltin("variable_struct_set", { dr, RValue("base"), RValue(vanilya) });
        } catch (...) {}
    }
}

#endif

static RValue& Hook_LoadDrops(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    // 1) Once VANILYA davranis, hicbir sey degistirmeden.
    RValue& res = g_OrigLoadDrops ? g_OrigLoadDrops(S, O, R, argc, A) : R;
#ifdef FORGEPACT_RELEASE
    // If a key feature was enabled and later switched Off, the hook remains
    // installed for this process.  Do not parse arguments or touch arrays.
    if (!g_DkOn) return res;
#endif
    if (!A || argc < 9 || !A[2] || !A[8] || !g_Yytk) return res;

    try {
        int tip = (int)A[2]->ToDouble();

#ifndef FORGEPACT_RELEASE
        // chances dizisinin TAMAMINI dok.  Blood Pact'teki 10 damla oranini
        // (rune, orb, gem, ore, satanic, heroic...) bulmanin en kisa yolu:
        // bu dizide sifirdan buyuk her indeks o canavarin attigi bir zar.
        if (g_DkFullProbe > 0) {
            RValue boyRV = g_Yytk->CallBuiltin("array_length", { *A[8] });
            int boy = (int)boyRV.ToDouble();
            std::ofstream f(IPC_DIR + "\\chances.txt", std::ios::app);
            f << "== cagri tip=" << tip << " argc=" << argc << " boy=" << boy << "\n";
            for (int i = 0; i < boy; i++) {
                RValue c = g_Yytk->CallBuiltin("array_get", { *A[8], RValue((double)i) });
                double d = c.ToDouble();
                if (d != 0.0) f << "   [" << i << "] = " << d << "\n";
            }
            // Diger argumanlar da ise yarayabilir - tipini/degerini yaz.
            for (int i = 0; i < argc; i++)
                f << "   arg" << i << " = " << (A[i] ? Describe(*A[i]) : std::string("(null)")) << "\n";
            f.flush();
            g_DkFullProbe--;
        }

        // --- typemap: tek olumde TUM damla tiplerini aileye esle ---------------
        // Sorun: chances dizisinde 70 slot var ama normal bir canavarda yalnizca
        // 9'u sifirdan buyuk.  Rune/orb/gem/ore hangi indekste, bilmiyoruz ve
        // 61 tipi tek tek oyunda denemek gunler surer.
        //
        // Cozum: her tip icin sirayla kapiyi ac, LoadDrops'u cagir, birikme
        // listesinin (arg6 dizisi / arg7 ds_list) BUYUYUP buyumedigine bak,
        // sonra izi sil.  Buyuduyse o tip bir eşya uretti - json'i yaz.
        if (g_TypeMapIste && !g_TypeMapCalisiyor) {
            g_TypeMapIste = false;
            g_TypeMapCalisiyor = true;   // yeniden girisi engelle
            std::ofstream f(IPC_DIR + "\\typemap.txt", std::ios::app);
            int boy = 0;
            try { boy = (int)g_Yytk->CallBuiltin("array_length", { *A[8] }).ToDouble(); } catch (...) {}
            std::vector<std::pair<int, int>> dokunulan;
            if (g_TypeMapTumOranlar) {
                try { TumOranlariAc(dokunulan); } catch (...) {}
            }
            f << "===== typemap taramasi  tip_sayisi=" << boy
              << "  sans=" << g_TypeMapSans
              << "  ic_zar=" << (g_TypeMapTumOranlar
                                 ? ("acik(" + std::to_string(dokunulan.size()) + " esya)")
                                 : std::string("vanilya"))
              << "  argc=" << argc << " =====\n";
            f.flush();

            std::vector<RValue*> A2(A, A + argc);
            for (int t = 0; t < boy; t++) {
                // HER TIP kendi korumasinda.  Bazi tipler bu canavar icin
                // gecersiz ve oyunun kendi kodu istisna atiyor; tek bir ortak
                // try kullanilinca ilk gecersiz tip tum taramayi dusuruyordu.
                double eskiD = 0.0;
                bool geriAlindi = false;
                g_TypeMapAdlar.clear();
                g_TypeMapKancaSayaci = 0;
                try {
                    eskiD = g_Yytk->CallBuiltin("array_get", { *A[8], RValue((double)t) }).ToDouble();

                    g_Yytk->CallBuiltin("array_set", { *A[8], RValue((double)t), RValue(g_TypeMapSans) });
                    RValue tipRV((double)t);
                    A2[2] = &tipRV;
                    RValue r2;
                    g_TypeMapAktifTip = t;               // yaratim kancalari bu tiple etiketlesin
                    if (g_OrigLoadDrops) g_OrigLoadDrops(S, O, r2, argc, A2.data());
                    g_TypeMapAktifTip = -1;

                    g_Yytk->CallBuiltin("array_set", { *A[8], RValue((double)t), RValue(eskiD) });
                    geriAlindi = true;

                    f << "[" << t << "] vanilya=" << eskiD
                      << "  uretilen=" << g_TypeMapAdlar.size() << "\n";
                    for (const auto& ad : g_TypeMapAdlar)
                        f << "      " << ad << "\n";
                } catch (...) {
                    g_TypeMapAktifTip = -1;
                    f << "[" << t << "] ISTISNA (bu canavar icin gecersiz tip olabilir)"
                      << "  uretilen=" << g_TypeMapAdlar.size() << "\n";
                    for (const auto& ad : g_TypeMapAdlar)
                        f << "      " << ad << "\n";
                }
                // Izi HER durumda sil - yoksa 100000 sans dizide kalir.
                if (!geriAlindi) {
                    try { g_Yytk->CallBuiltin("array_set", { *A[8], RValue((double)t), RValue(eskiD) }); }
                    catch (...) {}
                }
                f.flush();
            }
            if (g_TypeMapTumOranlar) {
                try { OranlariGeriAl(dokunulan); } catch (...) {}
                f << "-- " << dokunulan.size() << " esyanin ic zari vanilyaya geri alindi\n";
            }
            f << "===== tarama bitti =====\n";
            f.flush();
            g_TypeMapCalisiyor = false;
        }

        if (g_DkProbe > 0) {
            RValue c11 = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(11.0) });
            RValue c12 = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(12.0) });
            RValue c17 = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(17.0) });
            RValue c18 = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(18.0) });
            std::ofstream f(IPC_DIR + "\\loaddrops.txt", std::ios::app);
            f << "tip=" << tip
              << "  chances[11]=" << c11.ToDouble()
              << "  [12]=" << c12.ToDouble()
              << "  [17]=" << c17.ToDouble()
              << "  [18]=" << c18.ToDouble() << "\n";
            f.flush();
            g_DkProbe--;
        }
#endif

        if (!g_DkOn || tip != 11) return res;   // yalnizca normal-anahtar zarindan sonra

        // Oran: o canavarin KENDI normal-anahtar orani (auto) ya da sabit.
        // Auto sayesinde paydayi (gDataProtected.<0xAF>) bilmemize gerek yok.
        // Kapi orani icin taban: o canavarin KENDI normal-anahtar sansi.
        RValue tabanRV = g_Yytk->CallBuiltin("array_get", { *A[8], RValue(11.0) });
        double taban = tabanRV.ToDouble();

        std::vector<RValue*> A2(A, A + argc);
        for (int tipEk : g_DkTipler) {
            // Bu canavarda o tip zaten yerli mi?  Oyleyse DOKUNMA - yoksa ayni
            // olumde iki bagimsiz zar atilir ve boss'un kendi orani ezilir.
            RValue mevcut = g_Yytk->CallBuiltin("array_get", { *A[8], RValue((double)tipEk) });
            if (mevcut.ToDouble() > 0.0) { InterlockedIncrement(&g_DkNative); continue; }

            // HER TIPIN KENDI CARPANI.  Onceki surum tek ortak carpan
            // kullaniyordu ve panel ona TUM kaydiraclarin EN YUKSEGINI
            // gonderiyordu: Dungeon Keys'i 20 yapan biri Relic'i 2'de biraksa
            // bile relic kapisi 20 ile aciliyordu.  Kaydirac yalan soyluyordu.
            double kendiCarpan = g_DkChanceMult;          // eski davranis (geriye donuk)
            auto itc = g_DkTipCarpan.find(tipEk);
            if (itc != g_DkTipCarpan.end()) kendiCarpan = itc->second;

            // ON-ZAR: bu olumde bu tipi denemeye deger mi?
            // Oyunun kapisina 1'in altinda bir sayi vermek ise yaramiyor
            // (irandom tam sayi), o yuzden seyreltmeyi BURADA yapiyoruz.
            double olcek = TipOlcek(tipEk);
            if (olcek < 1.0) {
                // Kareli egri - altta yumusak, ustte sert.  Aciklama
                // g_DkTipOlcek tanimindaki olcum tablosunda.
                double olasilik = olcek * kendiCarpan * kendiCarpan;
                if (olasilik > 1.0) olasilik = 1.0;
                if (KendiZar() >= olasilik) continue;   // bu olumde hic denenmiyor
            }

            double oran = (g_DkChance >= 0.0) ? g_DkChance : (taban * kendiCarpan);
            if (oran <= 0.0) continue;

            g_Yytk->CallBuiltin("array_set", { *A[8], RValue((double)tipEk), RValue(oran) });

            // AYNI arguman dizisi, yalnizca damla tipi degisiyor.  Kapi ve zar vanilya.
            RValue tipRV((double)tipEk);
            A2[2] = &tipRV;
            RValue r2;
            if (tipEk == 41) { EnsurePartsGuardHooks(); g_DkPartsGuard = true; }
            if (g_OrigLoadDrops) g_OrigLoadDrops(S, O, r2, argc, A2.data());
            g_DkPartsGuard = false;
            InterlockedIncrement(&g_DkRolls);

            // Izi sil - ayni cerceve sonraki damla tipleri icin kullaniliyor.
            g_Yytk->CallBuiltin("array_set", { *A[8], RValue((double)tipEk), RValue(0.0) });
        }
    } catch (...) {}
    return res;
}

static void DungeonKeyCmd(const std::string& rest)
{
    std::string a1, a2; a1 = FirstToken(rest, a2);
    std::string v = Lower(a1);
    while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();

    if (v == "off") { g_DkOn = false; Out("dungeonkey: KAPALI"); return; }

#ifndef FORGEPACT_RELEASE
    if (v == "typemap") {
        std::string sansStr, bayrak; sansStr = FirstToken(a2, bayrak);
        try { g_TypeMapSans = std::stod(sansStr); } catch (...) { g_TypeMapSans = 100000.0; }
        if (!std::isfinite(g_TypeMapSans)) g_TypeMapSans = 100000.0;
        // "all" -> tarama suresince TUM esyalarin ic zari da 1'e cekilir
        g_TypeMapTumOranlar = (Lower(bayrak).find("all") != std::string::npos);
        if (!g_OrigLoadDrops)
            HookOneScriptTable("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
        g_TypeMapIste = (g_OrigLoadDrops != nullptr);
        if (!g_TypeMapIste) { Out("dungeonkey typemap: kanca kurulamadi"); return; }
        char b[200];
        sprintf_s(b, "dungeonkey typemap: SONRAKI olumde tum tipler taranacak (sans=%.0f, ic_zar=%s) -> bp_ipc\\typemap.txt",
                  g_TypeMapSans, g_TypeMapTumOranlar ? "ACIK" : "vanilya");
        Out(b);
        return;
    }

    if (v == "fullprobe") {
        int n = 3;
        try { n = std::stoi(a2); } catch (...) {}
        g_DkFullProbe = n;
        // Kancayi BURADA da kur.  Yoksa sonda kurulur ama LoadDrops hic
        // yakalanmaz ve dosya bos kalir - bir kez yasandi.
        if (!g_OrigLoadDrops)
            HookOneScriptTable("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
        Out("dungeonkey: sonraki " + std::to_string(n) + " LoadDrops cagrisinin chances dizisi -> bp_ipc\\chances.txt"
            + (g_OrigLoadDrops ? "" : "  (UYARI: kanca kurulamadi)"));
        return;
    }
#endif

    if (v == "scale") {
        // Aileye gore kapi olcegi - yeniden derlemeden ayarlanabilsin diye.
        std::string tipStr, degStr;
        tipStr = FirstToken(a2, degStr);
        try {
            int n = std::stoi(tipStr);
            double d = std::stod(degStr);
            if (!std::isfinite(d) || d <= 0.0) d = 1.0;   // reject <=0, NaN and "inf" text
            g_DkTipOlcek[n] = d;
            char sb[140];
            sprintf_s(sb, "dungeonkey scale: tip %d -> olcek %.4f", n, d);
            Out(sb);
        } catch (...) { Out("dungeonkey: kullanim -> dungeonkey scale 41 0.05"); }
        return;
    }

    if (v == "stat") {
        std::string o = (g_DkChance < 0.0)
            ? ("auto x" + std::to_string((int)g_DkChanceMult))
            : std::to_string((int)g_DkChance);
        std::string liste;
        for (int x : g_DkTipler) {
            if (!liste.empty()) liste += ",";
            liste += std::to_string(x);
            auto ic = g_DkTipCarpan.find(x);
            double kc = (ic == g_DkTipCarpan.end()) ? g_DkChanceMult : ic->second;
            char cb[56]; sprintf_s(cb, "(x%.0f olcek %.5f)", SafeF(kc), SafeF(TipOlcek(x)));
            liste += cb;
        }
        Out(std::string("dungeonkey: ") + (g_DkOn ? "ACIK" : "kapali") + " | prime evil parcasi atlandi=" + std::to_string(g_DkPartsSkipped)
            + " | tipler=" + (liste.empty() ? std::string("(bos)") : liste)
            + " | oran=" + o
            + " | ek zar=" + std::to_string(g_DkRolls)
            + " | yerli(dokunulmadi)=" + std::to_string(g_DkNative));
        return;
    }

    if (v == "add" || v == "del") {
        try {
            std::string tipStr, carpStr;
            tipStr = FirstToken(a2, carpStr);
            int n = std::stoi(tipStr);
            if (v == "add") {
                g_DkTipler.insert(n);
                // Ikinci arguman verilirse o tipin KENDI kapi carpani olur.
                if (!carpStr.empty()) {
                    try {
                        double c = std::stod(carpStr);
                        if (std::isfinite(c) && c > 0.0) g_DkTipCarpan[n] = c;
                    } catch (...) {}
                }
            } else {
                g_DkTipler.erase(n);
                g_DkTipCarpan.erase(n);
            }
            std::string liste;
            for (int x : g_DkTipler) { if (!liste.empty()) liste += ","; liste += std::to_string(x); }
            Out("dungeonkey tipler: " + (liste.empty() ? std::string("(bos)") : liste));
        } catch (...) { Out("dungeonkey: kullanim -> dungeonkey add 7"); }
        return;
    }

    if (v == "list") {
        std::string liste;
        for (int x : g_DkTipler) { if (!liste.empty()) liste += ","; liste += std::to_string(x); }
        Out("dungeonkey tipler: " + (liste.empty() ? std::string("(bos)") : liste));
        return;
    }

    if (v == "chance") {
        // a2 "autox 50" gibi iki parca gelebilir - once ayir, sonra karsilastir.
        std::string carpanStr;
        std::string c = Lower(FirstToken(a2, carpanStr));
        while (!c.empty() && (c.back()=='\r'||c.back()=='\n'||c.back()==' ')) c.pop_back();
        if (c == "auto" || c.empty()) { g_DkChance = -1.0; g_DkChanceMult = 1.0; Out("dungeonkey: oran = auto (canavarin kendi anahtar orani)"); }
        else if (c == "autox") {
            double m = 1.0;
            try { m = std::stod(carpanStr); } catch (...) {}
            if (!std::isfinite(m) || m < 1.0) m = 1.0;
            g_DkChance = -1.0; g_DkChanceMult = m;
            char b[120]; sprintf_s(b, "dungeonkey: oran = auto x%.0f (dis kapi carpani)", m);
            Out(b);
        }
        else {
            try { g_DkChance = std::stod(c); Out("dungeonkey: oran = " + c); }
            catch (...) { Out("dungeonkey: kullanim -> dungeonkey chance auto|60"); }
        }
        return;
    }

    if (v == "probe") {
#ifdef FORGEPACT_RELEASE
        Out("dungeonkey probe: yayin derlemesinde yok");
#else
        try { g_DkProbe = std::stoi(a2); } catch (...) { g_DkProbe = 60; }
        if (!g_OrigLoadDrops) HookOneScriptTable("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
        Out("dungeonkey probe: " + std::to_string(g_DkProbe) + " cagri -> bp_ipc\\loaddrops.txt");
#endif
        return;
    }

    if (!g_OrigLoadDrops)
        HookOneScript("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
    g_DkOn = (g_OrigLoadDrops != nullptr);
    Out(std::string("dungeonkey: ") + (g_DkOn ? "ACIK (dogal zar, zorlama yok)" : "kanca kurulamadi"));
}

static void NiCall(const std::string& script, const std::string& obj, int n)
{
    try {
        RValue id = NthInstance(obj, n);
        if (id.ToDouble() < 0) { Out("nicall: no " + obj + "[" + std::to_string(n) + "]"); return; }
        RValue res = g_Yytk->CallGameScript("gml_Script_" + script, { id });
        Out("nicall " + script + "(" + obj + "[" + std::to_string(n) + "]) -> " + Describe(res));
    } catch (...) { Out("nicall EXCEPTION"); }
}

// dsdump <mapId> [keyFilter] -- iterate a ds_map, write each key + json(value) to bp_ipc/dsdump.txt
static void DsDump(double mapId, const std::string& filter)
{
    try {
        std::ofstream f(IPC_DIR + "\\dsdump.txt", std::ios::trunc);
        RValue key = g_Yytk->CallBuiltin("ds_map_find_first", { RValue(mapId) });
        int count = 0, guard = 0;
        std::string flt = Lower(filter);
        while (key.m_Kind != VALUE_UNDEFINED && key.m_Kind != VALUE_UNSET && guard++ < 5000) {
            std::string ks = Describe(key);
            bool show = flt.empty() || Lower(ks).find(flt) != std::string::npos;
            if (show) {
                RValue val = g_Yytk->CallBuiltin("ds_map_find_value", { RValue(mapId), key });
                std::string vs;
                if (val.m_Kind == VALUE_OBJECT || val.m_Kind == VALUE_ARRAY) {
                    try { vs = g_Yytk->CallBuiltin("json_stringify", { val }).ToString(); } catch (...) { vs = Describe(val); }
                } else vs = Describe(val);
                f << ks << " = " << vs << "\n";
                count++;
            }
            key = g_Yytk->CallBuiltin("ds_map_find_next", { RValue(mapId), key });
        }
        Out("dsdump map " + std::to_string((long long)mapId) + " -> " + std::to_string(count) + " entries (iter=" + std::to_string(guard) + ") -> dsdump.txt");
    } catch (...) { Out("dsdump EXCEPTION"); }
}

// Apply a buff to the player via the game's own BuffAdd (self = player).
// Signature learned by observation: BuffAdd(1, buffId, [v0,v1], duration, false, true, 1, false)
static void ApplyBuff(int64_t buffId, double v0, double v1, double dur)
{
    try {
        RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        RValue pid  = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
        if (pid.ToDouble() < 0) { Out("buffme: no player"); return; }
        CInstance* player = nullptr;
        g_Yytk->GetInstanceObject((int32_t)pid.ToDouble(), player);
        std::vector<RValue> vals = { RValue(v0), RValue(v1) };
        // 10-arg "register a NEW buff" form: last arg true = create/register (HUD + active).
        std::vector<RValue> args = {
            RValue(1.0), RValue(buffId), RValue(vals), RValue(dur),
            RValue(false), RValue(false), RValue(1.0), RValue(false), RValue(false), RValue(true)
        };
        RValue result;
        AurieStatus st = g_Yytk->CallGameScriptEx(result, "gml_Script_BuffAdd", player, player, args);
        Out("buffme id=" + std::to_string(buffId) + " [" + std::to_string(v0) + "," + std::to_string(v1) + "] st=" + std::to_string((int)st));
    } catch (...) { Out("buffme EXCEPTION"); }
}

// Continuous buff: re-apply a buff every frame so it stays active (the game does this for auras).
static std::atomic<bool> g_MBuff{ false };
static int64_t g_MBuffId = 56;
static double g_MBuffV0 = 400, g_MBuffV1 = 500;
static void MBuffTick()
{
    if (!g_MBuff.load()) return;
    ApplyBuff(g_MBuffId, g_MBuffV0, g_MBuffV1, 90.0);
}

// Headhunter + player-context diagnostics live in their own function so the
// main RunCommand else-if chain stays below the compiler nesting limit (C1061).
static bool HandleHeadhunterCommand(const std::string& lc, const std::string& rest)
{
    if (lc == "headhunter") {
        std::string on = Lower(TrimCopy(rest));
        if (on == "on" || on == "1" || on == "force") { g_HhForced.store(on == "force"); EnableHeadhunter(); }
        else if (on == "off" || on == "0") { g_HhEnabled.store(false); g_HhForced.store(false); }
        HeadhunterStatus();
    } else if (lc == "hhdur") {
        try { double s = std::stod(rest); if (s >= 1.0 && s <= 600.0) g_HhDurationSec = s; } catch (...) {}
        Out("hhdur -> " + std::to_string(g_HhDurationSec) + " s");
    } else if (lc == "hhmap") {
        // hhmap <affixKey> <buffId> [v0] [v1]  |  hhmap <affixKey> off  |  hhmap clear
        std::string key, r2; key = Lower(FirstToken(rest, r2));
        if (key == "clear") { g_HhMap.clear(); Out("hhmap cleared"); }
        else {
            std::stringstream ss(r2); std::string idTok; ss >> idTok;
            if (Lower(idTok) == "off") { g_HhMap.erase(key); Out("hhmap " + key + " removed"); }
            else {
                try {
                    HhBuff b{ (int64_t)std::stoll(idTok), 100.0, 100.0 };
                    if (!(ss >> b.v0)) b.v0 = 100.0;
                    if (!(ss >> b.v1)) b.v1 = b.v0;
                    g_HhMap[key] = b;
                    Out("hhmap " + key + " -> buff " + std::to_string((long long)b.id) + " [" + std::to_string(b.v0) + "," + std::to_string(b.v1) + "]");
                } catch (...) { Out("hhmap: usage hhmap <affixKey> <buffId> [v0] [v1] | hhmap <affixKey> off | hhmap clear"); }
            }
        }
    } else if (lc == "hhdefault") {
        std::stringstream ss(rest); std::string idTok; ss >> idTok;
        if (Lower(idTok) == "off") { g_HhDefaultOn = false; Out("hhdefault off"); }
        else {
            try {
                g_HhDefault.id = (int64_t)std::stoll(idTok);
                if (!(ss >> g_HhDefault.v0)) g_HhDefault.v0 = 100.0;
                if (!(ss >> g_HhDefault.v1)) g_HhDefault.v1 = g_HhDefault.v0;
                g_HhDefaultOn = true;
                Out("hhdefault -> buff " + std::to_string((long long)g_HhDefault.id));
            } catch (...) { Out("hhdefault: usage hhdefault <buffId> [v0] [v1] | off"); }
        }
#ifndef FORGEPACT_RELEASE
    } else if (lc == "spawnforce") {
        // spawnforce <alarm> <count>: perform Alarm <alarm> on the <count> nearest awake
        // Enemy_Creator_obj spawners (real `with`-style self via InvokeWithObject) and report
        // the enemy count delta.
        int alarmIdx = 1, count = 3; { std::stringstream ss(rest); ss >> alarmIdx >> count; }
        try {
            RValue cobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Creator_obj") });
            RValue eobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
            RValue player; if (!HhResolveLocalPlayer(player)) { Out("spawnforce: no player"); return true; }
            double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
            double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
            std::vector<std::pair<double, CInstance*>> order;
            AurieStatus ws = g_Yytk->InvokeWithObject(cobj, [&](CInstance* self, CInstance* other) {
                try {
                    RValue inst = self->ToRValue();
                    double cx = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
                    double cy = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
                    order.push_back({ std::sqrt((cx - px) * (cx - px) + (cy - py) * (cy - py)), self });
                } catch (...) {}
            });
            std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            int before = (int)g_Yytk->CallBuiltin("instance_number", { eobj }).ToDouble();
            int done = 0;
            for (size_t i = 0; i < order.size() && done < count; ++i) {
                CInstance* ci = order[i].second;
                RValue res; AurieStatus es = g_Yytk->CallBuiltinEx(res, "event_perform", ci, ci, { RValue(2.0), RValue((double)alarmIdx) });   // ev_alarm = 2
                ++done;
                Out("spawnforce: creator " + TyInstName(ci->ToRValue()) + " dist=" + std::to_string((long long)order[i].first) + " alarm " + std::to_string(alarmIdx) + " performed st=" + std::to_string((int)es));
            }
            int after = (int)g_Yytk->CallBuiltin("instance_number", { eobj }).ToDouble();
            Out("spawnforce: " + std::to_string(done) + " creators (of " + std::to_string(order.size()) + " visited, with st=" + std::to_string((int)ws) + "), enemies " + std::to_string(before) + " -> " + std::to_string(after));
        } catch (...) { Out("spawnforce: EXC"); }
    } else if (lc == "huntstats") {
        // Snapshot of every awake Enemy_Parent_obj: per rarity, how many chase (mySocketTarget set),
        // how many idle, and how far the idle ones are from the player (proves or disproves far aggro).
        try {
            RValue eobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
            RValue player; if (!HhResolveLocalPlayer(player)) { Out("huntstats: no player"); return true; }
            double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
            double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
            int n = (int)g_Yytk->CallBuiltin("instance_number", { eobj }).ToDouble();
            std::map<int, std::array<long, 6>> byRar;   // rarity -> {chasing, idle, idleFar(>1500), idleFarBigRange, chasingFar, canAggroIdle}
            std::string farIdle;
            for (int i = 0; i < n; ++i) {
                RValue inst = g_Yytk->CallBuiltin("instance_find", { eobj, RValue((double)i) });
                auto num = [&](const char* nm) { try { return g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue(nm) }).ToDouble(); } catch (...) { return -99999.0; } };
                int rar = (int)num("enemyRarity");
                double ex = num("x"), ey = num("y"); double d = std::sqrt((ex - px) * (ex - px) + (ey - py) * (ey - py));
                double tgt = num("mySocketTarget"), range = num("distance"), can = num("canAggro");
                bool chasing = tgt >= 0;
                auto& a = byRar[rar];
                if (chasing) { ++a[0]; if (d > 1500) ++a[4]; }
                else { ++a[1]; if (d > 1500) { ++a[2]; if (range > 100000) ++a[3]; } if (can > 0) ++a[5]; }
                if (!chasing && rar >= 2 && d > 1500 && farIdle.size() < 900) farIdle += " " + TyInstName(inst) + "(d=" + std::to_string((long long)d) + ",range=" + std::to_string((long long)range) + ",canAggro=" + std::to_string((int)can) + ",tgt=" + std::to_string((long long)tgt)
                    + ",vis=" + std::to_string((int)num("visible")) + ",dis=" + std::to_string((int)num("isDisabled")) + ",aggroT=" + std::to_string((int)num("aggroTimer")) + ",upd=" + std::to_string((int)num("enemyUpdateOnline")) + ",spawnAnim=" + std::to_string((int)num("spawnAnimationDone")) + ",delta=" + std::to_string((int)num("deltaTimer")) + ")";
            }
            Out("huntstats: awake enemies=" + std::to_string(n) + " policy=" + std::to_string(HuntPolicy()) + " scans(sampled 1/10) near<1500=" + std::to_string(g_BeScanNear) + " mid=" + std::to_string(g_BeScanMid) + " far>3000=" + std::to_string(g_BeScanFar));
            for (auto& kv : byRar) Out("  rarity " + std::to_string(kv.first) + ": chasing=" + std::to_string(kv.second[0]) + " (far>1500: " + std::to_string(kv.second[4]) + ")  idle=" + std::to_string(kv.second[1]) + " (far>1500: " + std::to_string(kv.second[2]) + ", of which bigRange: " + std::to_string(kv.second[3]) + ", canAggro>0: " + std::to_string(kv.second[5]) + ")");
            if (!farIdle.empty()) Out("  far idle rares:" + farIdle);
        } catch (...) { Out("huntstats: EXC"); }
    } else if (lc == "spawntrace") {
        int n = 40; try { n = std::stoi(TrimCopy(rest)); } catch (...) {}
        g_SpawnTraceLeft = n; Out("spawntrace -> next " + std::to_string(n) + " logged checks (so far calls " + std::to_string(g_SpawnCheckCalls) + ", spawned " + std::to_string(g_SpawnCheckSpawned)
            + ", spawn dist min/max " + std::to_string((long long)g_SpawnDistMin) + "/" + std::to_string((long long)g_SpawnDistMax) + ", closest silent check " + std::to_string((long long)g_SpawnNoMin) + ")");
    } else if (lc == "aggrotrace") {
        int n = 40; try { n = std::stoi(TrimCopy(rest)); } catch (...) {}
        g_AggroTraceLeft = n; g_AggroScanSeen.clear(); Out("aggrotrace -> next " + std::to_string(n) + " AI target events");
    } else if (lc == "raritytrace") {
        int n = 20; try { n = std::stoi(TrimCopy(rest)); } catch (...) {}
        InstallTyrantHook();
        g_RarTraceLeft = n; Out("raritytrace -> next " + std::to_string(n) + " EnemyRaritySettings calls (total so far " + std::to_string(g_TySeen) + ")");
    } else if (lc == "rarityforce" || lc == "raritypre") {
        double v = 3; int k = 5; { std::stringstream ss(rest); ss >> v >> k; }
        if (lc == "rarityforce") { g_RarForceVal = v; g_RarForceLeft = k; } else { g_RarPreVal = v; g_RarPreLeft = k; }
        Out(lc + " -> " + std::to_string((int)v) + " for the next " + std::to_string(k) + " enemies");
    } else if (lc == "tiptrace") {
        int n = 40; try { n = std::stoi(TrimCopy(rest)); } catch (...) {}
        g_TipTraceLeft = n; g_TipDrawTraceLeft = 16; Out("tiptrace -> next " + std::to_string(n) + " tooltip calls");
#endif
    } else if (lc == "tyrant") {
        std::string v = Lower(TrimCopy(rest));
        if (v == "status" || v.empty()) { TyrantStatus(); return true; }
        if (v == "off") { g_TyEnabled = false; g_TyForced = false; Out("tyrant: off"); return true; }
        if (v == "force") g_TyForced = true;
        InstallTyrantHook(); InstallBeaconHook(); g_TyEnabled.store(g_TyHookInstalled);
        TyrantStatus();
    } else if (lc == "beacon") {
        std::string v = Lower(TrimCopy(rest));
        if (v == "status" || v.empty()) { BeaconStatus(); return true; }
        if (v == "off") { g_BeEnabled = false; g_BeForced = false; Out("beacon: off"); return true; }
        if (v == "force") g_BeForced = true;
        InstallBeaconHook(); g_BeEnabled.store(g_BeHookInstalled);
        BeaconStatus();
    } else if (lc == "beaconrange") {
        try { double p = std::stod(TrimCopy(rest)); if (p >= 100.0 && p <= 10000000.0) g_BeRange = p; } catch (...) {}
        Out("beaconrange -> " + std::to_string((long long)g_BeRange) + " px");
    } else if (lc == "beaconwake" && Lower(TrimCopy(rest)).rfind("every", 0) == 0) {
        try { int n = std::stoi(TrimCopy(rest.substr(rest.find("every") + 5))); if (n >= 1 && n <= 60) g_BeWakeEvery = n; } catch (...) {}
        Out("beaconwake every -> " + std::to_string(g_BeWakeEvery) + " frames (1 = vanilla freezing every frame)");
    } else if (lc == "beaconwake" && Lower(TrimCopy(rest)).rfind("creators", 0) == 0) {
        std::string v = Lower(TrimCopy(rest.substr(rest.find("creators") + 8)));
        if (v == "off" || v == "0") g_BeWakeCreators = false; else g_BeWakeCreators = true;
        Out(std::string("beaconwake creators -> ") + (g_BeWakeCreators ? "on (spawners inside the radius wake up too)" : "off"));
#ifndef FORGEPACT_RELEASE
    } else if (lc == "creatorprobe") {
        // Research: how many spawners exist vs are awake, and what a spawner carries.
        for (const char* nm : kBeCreatorObjects) {
            try {
                RValue cobj = g_Yytk->CallBuiltin("asset_get_index", { RValue(nm) });
                if (cobj.ToDouble() < 0) continue;
                int before = (int)g_Yytk->CallBuiltin("instance_number", { cobj }).ToDouble();
                g_Yytk->CallBuiltin("instance_activate_object", { cobj });
                int after = (int)g_Yytk->CallBuiltin("instance_number", { cobj }).ToDouble();
                Out(std::string("creatorprobe ") + nm + ": awake " + std::to_string(before) + " / total " + std::to_string(after));
                if (after > 0 && std::string(nm) == "Enemy_Creator_obj") {
                    RValue inst = g_Yytk->CallBuiltin("instance_find", { cobj, RValue(0.0) });
                    RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { inst });
                    int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble(); std::string line;
                    for (int i = 0; i < n; ++i) {
                        RValue vn = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
                        std::string s = vn.ToString(), ls = Lower(s);
                        if (ls.find("range") != std::string::npos || ls.find("dist") != std::string::npos || ls.find("spawn") != std::string::npos || ls.find("activ") != std::string::npos
                            || ls.find("trigger") != std::string::npos || ls.find("done") != std::string::npos || ls.find("creat") != std::string::npos || ls.find("wait") != std::string::npos
                            || ls.find("radius") != std::string::npos || ls.find("amount") != std::string::npos || ls.find("count") != std::string::npos || ls.find("alarm") != std::string::npos) {
                            RValue v = g_Yytk->CallBuiltin("variable_instance_get", { inst, vn });
                            std::string d = Describe(v); if (d.size() > 50) d = d.substr(0, 50) + "...";
                            line += " " + s + "=" + d;
                        }
                    }
                    Out("   vars:" + line);
                    try { RValue al = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("alarm") }); Out("   alarm array: " + HhDescribeList(al)); } catch (...) {}
                }
            } catch (...) { Out(std::string("creatorprobe ") + nm + ": EXC"); }
        }
    } else if (lc == "forgeddump") {
        // Research: json of every remembered forged item -> bp_ipcorged_dump.json
        std::string body = "["; int n = 0;
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        for (const RValue& it : g_ForgedItems) {
            try { RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { it }); body += (n ? "," : "") + js.ToString(); ++n; } catch (...) {}
        }
        body += "]";
        { std::ofstream f(IPC_DIR + "\\forged_dump.json", std::ios::binary | std::ios::trunc); f << body; }
        Out("forgeddump: " + std::to_string(n) + " items -> forged_dump.json");
    } else if (lc == "hashprobe") {
        // Research: refresh itemDataHash on every remembered forged item and report the path used.
        int n = 0;
        for (const RValue& it : g_ForgedItems) {
            std::string how; const std::string b = ReadItemHash(it); const bool ok = RefreshItemHash(it, &how); const std::string a = ReadItemHash(it);
            Out("hashprobe #" + std::to_string(n++) + ": " + b.substr(0, 8) + " -> " + (a.empty() ? std::string("(none)") : a.substr(0, 8)) + " via " + how + (ok ? "" : " FAILED"));
        }
        if (!n) Out("hashprobe: no forged items remembered yet");
    } else if (lc == "enemyvars") {
        // Research: nearest monsters with rarity, affixes and the variables matching the filters.
        try {
            std::vector<std::string> filters;
            { std::string f = Lower(TrimCopy(rest)); if (f.empty()) f = "resist,magic,spell,immun,arcane";
              size_t p = 0; while (p <= f.size()) { size_t c = f.find(',', p); if (c == std::string::npos) c = f.size(); std::string t = TrimCopy(f.substr(p, c - p)); if (!t.empty()) filters.push_back(t); p = c + 1; } }
            RValue player; double px = 0, py = 0;
            if (HhResolveLocalPlayer(player)) { px = HhReadNumber(player, "x", 0.0); py = HhReadNumber(player, "y", 0.0); }
            struct E { double d; RValue id; };
            std::vector<E> list;
            RValue eobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
            int total = (int)g_Yytk->CallBuiltin("instance_number", { eobj }).ToDouble();
            for (int n = 0; n < total && n < 600; ++n) {
                RValue id = g_Yytk->CallBuiltin("instance_find", { eobj, RValue((double)n) });
                double ex = HhReadNumber(id, "x", 0.0), ey = HhReadNumber(id, "y", 0.0);
                list.push_back({ std::sqrt((ex - px) * (ex - px) + (ey - py) * (ey - py)), id });
            }
            std::sort(list.begin(), list.end(), [](const E& a, const E& b) { return a.d < b.d; });
            Out("enemyvars: " + std::to_string(total) + " monsters, showing nearest " + std::to_string(std::min<size_t>(list.size(), 12)));
            for (size_t k = 0; k < list.size() && k < 12; ++k) {
                const RValue& id = list[k].id;
                std::string line = "  #" + std::to_string(k) + " " + TyInstName(id) + " dist=" + std::to_string((int)list[k].d) + " rarity=" + std::to_string((int)HhReadNumber(id, "enemyRarity", -1.0));
                try {
                    RValue flags = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("enemyAffix") });
                    if (flags.m_Kind == VALUE_ARRAY) {
                        int len = (int)g_Yytk->CallBuiltin("array_length", { flags }).ToDouble(); std::string af;
                        for (int i = 0; i < len && i < 128; ++i) { RValue f = g_Yytk->CallBuiltin("array_get", { flags, RValue((double)i) }); if (f.ToDouble() != 0.0) af += (af.empty() ? "" : ",") + std::to_string(i) + ":" + HhAffixName(i); }
                        line += " affixes=[" + af + "]";
                    }
                } catch (...) {}
                try {
                    RValue names = g_Yytk->CallBuiltin("variable_instance_get_names", { id });
                    int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble(); std::string vars;
                    for (int i = 0; i < n; ++i) {
                        RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)i) });
                        std::string s = nm.ToString(), ls = Lower(s); bool hit = false;
                        for (const auto& f : filters) if (ls.find(f) != std::string::npos) { hit = true; break; }
                        if (!hit) continue;
                        RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, nm });
                        std::string d = Describe(v); if (d.size() > 80) d = d.substr(0, 80) + "...";
                        vars += " " + s + "=" + d;
                    }
                    line += " |" + (vars.empty() ? std::string(" (no matching vars)") : vars);
                } catch (...) { line += " | (vars exc)"; }
                Out(line);
            }
        } catch (...) { Out("enemyvars: EXC"); }
#endif
    } else if (lc == "beaconwake") {
        std::string v = Lower(TrimCopy(rest));
        if (v == "all") g_BeWakeRadius = -1.0; else if (v == "off") g_BeWakeRadius = 0.0;
        else { try { double p = std::stod(v); if (p >= 500.0 && p <= 100000.0) g_BeWakeRadius = p; } catch (...) {} }
        Out("beaconwake -> " + (g_BeWakeRadius < 0 ? std::string("whole map") : g_BeWakeRadius == 0 ? std::string("off (vanilla freezing)") : std::to_string((long long)g_BeWakeRadius) + " px"));
    } else if (lc == "beaconspawn") {
        std::string v = Lower(TrimCopy(rest));
        if (v == "off" || v == "0") g_BeSpawnNear = false; else g_BeSpawnNear = true;
        Out(std::string("beaconspawn -> ") + (g_BeSpawnNear ? "on (awake spawners give birth as if you stood next to them)" : "off"));
    } else if (lc == "beaconfarstep") {
        std::string v = Lower(TrimCopy(rest));
        if (v == "off" || v == "0") g_BeFarStep = false; else if (v == "on" || v == "1" || v.empty()) g_BeFarStep = true;
        else { try { double d = std::stod(v); if (d >= 300 && d <= 20000) g_BeFarFrom = d; } catch (...) {} }
        Out(std::string("beaconfarstep -> ") + (g_BeFarStep ? "on" : "off") + " from " + std::to_string((long long)g_BeFarFrom) + " px");
    } else if (lc == "beaconmode") {
        std::string v = Lower(TrimCopy(rest));
        if (v == "rare") g_BeRareOnly = true; else if (v == "all") g_BeRareOnly = false;
        Out(std::string("beaconmode -> ") + (g_BeRareOnly ? "rare (rares and champions only)" : "all monsters"));
    } else if (lc == "rarity") {
        // rarity <rarePct> <ancientPct>  |  rarity off
        std::string rareStr, ancStr; rareStr = FirstToken(rest, ancStr);
        std::string r1 = Lower(TrimCopy(rareStr));
        double rare = 0.0, anc = 0.0;
        if (r1 != "off" && !r1.empty()) {
            try { rare = std::stod(r1); } catch (...) { rare = 0.0; }
            try { anc = std::stod(TrimCopy(ancStr)); } catch (...) { anc = 0.0; }
        }
        if (anc < 0.0) anc = 0.0; if (anc > 100.0) anc = 100.0;
        if (rare < 0.0) rare = 0.0; if (rare > 100.0 - anc) rare = 100.0 - anc;
        g_RarRarePct = rare; g_RarAncientPct = anc;
        // The hook is shared with Tyrant's Crown; a vanilla setting installs nothing.
        if (RarityFloorActive()) InstallTyrantHook();
        Out(std::string("rarity -> ") + (RarityFloorActive()
            ? ("rare " + std::to_string((int)rare) + " pct, ancient " + std::to_string((int)anc) + " pct" + (g_TyHookInstalled ? "" : " (hook failed)"))
            : std::string("off"))
            + " | raised so far: rare=" + std::to_string(g_RarRaisedRare) + " ancient=" + std::to_string(g_RarRaisedAncient)
            + " | enemy-born left alone: " + std::to_string(g_RarSkippedEnemyBorn) + " (seen " + std::to_string(g_EnemyBornSeen) + ")");
    } else if (lc == "tyrantchance" || lc == "tyrantaffix") {
        try { double p = std::stod(TrimCopy(rest)); if (p >= 0.0 && p <= 100.0) { if (lc == "tyrantchance") g_TyRarePct = p; else g_TyAffixPct = p; } } catch (...) {}
        Out(lc + " -> " + std::to_string((int)(lc == "tyrantchance" ? g_TyRarePct : g_TyAffixPct)) + " percent");
    } else if (lc == "hhlabel") {
        std::string v = Lower(TrimCopy(rest));
        if (v == "off" || v == "0") g_HhLabelOn = false; else if (!v.empty()) g_HhLabelOn = true;
        Out(std::string("hhlabel -> ") + (g_HhLabelOn.load() ? "ON" : "off") + " (" + std::to_string(g_HhStolen.size()) + " active, callback " + (g_HhObjectCallbackInstalled ? "ok" : "missing") + ")"
            + " hudCalls=" + std::to_string(g_HhHudCalls) + " draws=" + std::to_string(g_HhLabelDraws) + " playerId=" + std::to_string(g_HhLabelPlayerId) + " offset=" + std::to_string((int)g_HhLabelOffsetPx) + " lastErr=" + g_HhLabelLastErr);
    } else if (lc == "hhlabelprobe") {
        auto num = [&](const char* fn, std::vector<RValue> a) -> std::string {
            try { RValue v = g_Yytk->CallBuiltin(fn, a); return Describe(v); } catch (...) { return "EXC"; }
        };
        std::string s = "hhlabelprobe: playerId=" + std::to_string(g_HhLabelPlayerId);
        if (g_HhLabelPlayerId >= 0) {
            RValue id((double)g_HhLabelPlayerId);
            s += " exists=" + num("instance_exists", { id }) + " x=" + num("variable_instance_get", { id, RValue("x") }) + " y=" + num("variable_instance_get", { id, RValue("y") }) + " bbox_top=" + num("variable_instance_get", { id, RValue("bbox_top") });
        }
        Out(s);
        { RValue lp; std::string how; bool ok = HhResolveLocalPlayer(lp, &how);
          Out("  resolver: " + how + (ok ? " -> " + Describe(lp) + " x=" + num("variable_instance_get", { lp, RValue("x") }) + " y=" + num("variable_instance_get", { lp, RValue("y") }) + " bbox_top=" + num("variable_instance_get", { lp, RValue("bbox_top") }) + " id=" + num("variable_instance_get", { lp, RValue("id") }) : std::string(" (not found)"))); }
        RValue cam0 = g_Yytk->CallBuiltin("view_get_camera", { RValue(0.0) });
        Out("  view_get_camera(0)=" + Describe(cam0) + " camera_get_active=" + num("camera_get_active", {}) + " camera_get_default=" + num("camera_get_default", {}) + " view_enabled=" + num("variable_global_get", { RValue("view_enabled") }));
        for (int i = 0; i < 2; ++i) {
            RValue c = i == 0 ? cam0 : g_Yytk->CallBuiltin("camera_get_default", {});
            Out(std::string("  cam") + (i == 0 ? "0" : "Default") + ": view_x=" + num("camera_get_view_x", { c }) + " view_y=" + num("camera_get_view_y", { c }) + " view_w=" + num("camera_get_view_width", { c }) + " view_h=" + num("camera_get_view_height", { c }));
            try {
                RValue m = g_Yytk->CallBuiltin("camera_get_view_mat", { c });
                RValue p = g_Yytk->CallBuiltin("camera_get_proj_mat", { c });
                std::string ms, ps;
                for (int k = 0; k < 16; ++k) { ms += " " + std::to_string(g_Yytk->CallBuiltin("array_get", { m, RValue((double)k) }).ToDouble()); ps += " " + std::to_string(g_Yytk->CallBuiltin("array_get", { p, RValue((double)k) }).ToDouble()); }
                Out("    viewmat:" + ms); Out("    projmat:" + ps);
            } catch (...) { Out("    matrices: EXC"); }
        }
        Out("  gui=" + num("display_get_gui_width", {}) + "x" + num("display_get_gui_height", {}) + " window=" + num("window_get_width", {}) + "x" + num("window_get_height", {}) + " room=" + num("variable_global_get", { RValue("room_width") }) + "x" + num("variable_global_get", { RValue("room_height") }) + " view_wport0=" + num("view_get_wport", { RValue(0.0) }) + " view_hport0=" + num("view_get_hport", { RValue(0.0) }) + " view_visible0=" + num("view_get_visible", { RValue(0.0) }));
        Out("  active labels=" + std::to_string(g_HhStolen.size()) + " lastErr=" + g_HhLabelLastErr);
#ifndef FORGEPACT_RELEASE
    } else if (lc == "perf") {
        if (Lower(TrimCopy(rest)) == "reset") { PerfReset(); Out("perf: counters reset"); } else PerfReport();
#endif
#ifndef FORGEPACT_RELEASE
    } else if (lc == "worn") {
        RefreshWornMechanics(true);
        std::string m; for (const auto& x : g_WornMechanics) m += x + " ";
        Out("worn: " + g_WornShape + " | items: " + g_WornDetail + "| mechanics worn: " + (m.empty() ? std::string("(none)") : m)
            + "| tyrant active=" + (TyrantActive() ? "yes" : "no") + " beacon active=" + (BeaconActive() ? "yes" : "no") + " headhunter equipped=" + (HhEquipped(nullptr) ? "yes" : "no"));
#endif
    } else if (lc == "angeliclist") {
        g_AngelicPoolBuilt = false; BuildAngelicPool(true);
    } else if (lc == "angelicdrop") {
        std::string v = Lower(TrimCopy(rest));
        if (v.empty() || v == "status") AngelicDropStatus();
        else if (v == "off" || v == "0") { g_AngelicDropOneIn = 0.0; AngelicDropStatus(); }
        else {
            try { double n = std::stod(v); if (n < 1.0) n = 1.0; if (n > 10000000.0) n = 10000000.0; BuildAngelicPool(false); g_AngelicDropOneIn = g_AngelicPool.empty() ? 0.0 : n; InstallHeadhunterHook(); AngelicDropStatus(); }
            catch (...) { Out("angelicdrop: usage -> angelicdrop <one in N kills> | off | status"); }
        }
    } else if (lc == "sigdrop") {
        std::string v = Lower(TrimCopy(rest));
        if (v.empty() || v == "status") SigDropStatus();
        else if (v == "off" || v == "0") { g_SigDropPct = 0.0; g_SigDropAncientPct = 0.0; g_SigDropPity = 0; SigDropStatus(); }
        else if (v == "vanilla" || v == "default") { g_SigDropPct = kSigDropAngelicPct; g_SigDropAncientPct = kSigDropAngelicPct; g_SigDropPity = 0; InstallHeadhunterHook(); SigDropStatus(); }
        else {
            // sigdrop <rare pct> [ancient pct] [pity kills]
            try {
                std::string a, restb; a = FirstToken(v, restb); std::string b2, restc; b2 = FirstToken(restb, restc);
                double p = std::stod(a); if (p < 0.0) p = 0.0; if (p > 100.0) p = 100.0; g_SigDropPct = p;
                if (!b2.empty()) { double q = std::stod(b2); if (q < 0.0) q = 0.0; if (q > 100.0) q = 100.0; g_SigDropAncientPct = q; }
                std::string c2 = TrimCopy(restc); if (!c2.empty()) { long n = std::stol(c2); if (n < 0) n = 0; g_SigDropPity = n; }
                InstallHeadhunterHook(); SigDropStatus();
            } catch (...) { Out("sigdrop: usage -> sigdrop <rare pct> [ancient pct] [pity kills] | vanilla | off | status"); }
        }
    } else if (lc == "hhlabelmax") {
        try { long v = std::stol(TrimCopy(rest)); if (v >= 1 && v <= 40) g_HhLabelMax = (size_t)v; } catch (...) {}
        while (g_HhStolen.size() > g_HhLabelMax) g_HhStolen.erase(g_HhStolen.begin());
        Out("hhlabelmax -> " + std::to_string(g_HhLabelMax) + " labels");
    } else if (lc == "hhlabeloffset") {
        try { g_HhLabelOffsetPx = std::stod(TrimCopy(rest)); } catch (...) {}
        Out("hhlabeloffset -> " + std::to_string((int)g_HhLabelOffsetPx) + " px above the head");
    } else if (lc == "hhlabelfont") {
        std::string v = TrimCopy(rest);
        if (Lower(v) == "off" || v.empty()) g_HhLabelFont.clear(); else g_HhLabelFont = v;
        Out("hhlabelfont -> " + (g_HhLabelFont.empty() ? std::string("(current font)") : g_HhLabelFont));
#ifndef FORGEPACT_RELEASE
    } else if (lc == "fonts") {
        std::string out;
        for (int i = 0; i < 300; ++i) {
            try {
                RValue ex = g_Yytk->CallBuiltin("font_exists", { RValue((double)i) });
                if (!ex.ToBoolean()) continue;
                RValue nm = g_Yytk->CallBuiltin("font_get_name", { RValue((double)i) });
                out += " " + std::to_string(i) + ":" + nm.ToString();
            } catch (...) {}
        }
        Out("fonts:" + out);
        try { RValue cur = g_Yytk->CallBuiltin("draw_get_font", {}); Out("current draw font index=" + std::to_string((int)cur.ToDouble())); } catch (...) {}
    } else if (lc == "hhtrace") {
        g_HhTrace = (Lower(TrimCopy(rest)) != "off"); g_HhLastShape.clear(); Out(std::string("hhtrace -> ") + (g_HhTrace ? "ON" : "off"));
        if (g_HhTrace) InstallEquipTraceHooks();
    } else if (lc == "hhtest") {
        // hhtest [affixKey ...] -- simulate a rare kill: the first Enemy_Parent_obj instance's affixList is
        // temporarily replaced by the given keys, the player is the first Player_obj.
        try {
            RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
            RValue pid = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
            RValue eobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
            RValue eid = g_Yytk->CallBuiltin("instance_find", { eobj, RValue(0.0) });
            if (pid.ToDouble() < 0 || eid.ToDouble() < 0) { Out("hhtest: need a player and an enemy instance"); }
            else {
                CInstance* player = nullptr; g_Yytk->GetInstanceObject((int32_t)pid.ToDouble(), player);
                std::vector<RValue> keys; std::stringstream ss(rest); std::string k; while (ss >> k) keys.push_back(RValue(k));
                if (keys.empty()) keys.push_back(RValue(std::string("affixBerserker")));
                RValue backup = g_Yytk->CallBuiltin("variable_instance_get", { eid, RValue("affixList") });
                g_Yytk->CallBuiltin("variable_instance_set", { eid, RValue("affixList"), RValue(keys) });
                bool wasTrace = g_HhTrace; g_HhTrace = true;
                HhOnKill(player, eid);
                g_HhTrace = wasTrace;
                g_Yytk->CallBuiltin("variable_instance_set", { eid, RValue("affixList"), backup });
                HeadhunterStatus();
            }
        } catch (...) { Out("hhtest EXCEPTION"); }
    } else if (lc == "hhitems") {
        // hhitems -- registry of mechanic-tagged item structs: tag, itemType, definition a/b and every field whose
        // name mentions equip/owner/player, so the equipped-state field can be verified live.
        Out("hhitems: " + std::to_string(g_ForgedItems.size()) + " tagged item struct(s)");
        for (size_t i = 0; i < g_ForgedItems.size(); ++i) {
            const RValue& item = g_ForgedItems[i];
            try {
                std::string line = "  [" + std::to_string(i) + "] ";
                RValue m = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("fp_mechanic") });
                line += "mechanic=" + m.ToString();
                RValue def = g_Yytk->CallBuiltin("variable_struct_get", { item, RValue("itemDefinitionStruct") });
                if (def.m_Kind == VALUE_OBJECT) line += " a=" + std::to_string((long long)HhReadNumber(def, "a", -1)) + " b=" + std::to_string((long long)HhReadNumber(def, "b", -1));
                line += " itemType=" + std::to_string((long long)HhReadNumber(item, "itemType", -1));
                RValue names = g_Yytk->CallBuiltin("variable_struct_get_names", { item });
                int n = (int)g_Yytk->CallBuiltin("array_length", { names }).ToDouble();
                for (int k = 0; k < n && k < 200; ++k) {
                    RValue nm = g_Yytk->CallBuiltin("array_get", { names, RValue((double)k) });
                    std::string s = nm.ToString(); std::string ls = Lower(s);
                    if (ls.find("equip") != std::string::npos || ls.find("owner") != std::string::npos || ls.find("player") != std::string::npos || ls.find("slot") != std::string::npos) {
                        RValue v = g_Yytk->CallBuiltin("variable_struct_get", { item, nm });
                        line += " " + s + "=" + Describe(v);
                    }
                }
                Out(line);
            } catch (...) { Out("  [" + std::to_string(i) + "] (dead struct)"); }
        }
    } else if (lc == "hhscan") {
        // hhscan -- research: pair every rare Enemy_Parent_obj (enemyRarity >= 2, or affix-flagged)
        // with the Enemy_Health_Bar_Parent_obj drawn over it (nearest by position); the bar carries
        // healthBarName and the affixName array = the displayed affix strings.  Output: index <-> name.
        try {
            struct E { double x, y; int rarity; std::string obj; std::vector<int> idx; };
            std::vector<E> enemies;
            RValue eobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
            int total = (int)g_Yytk->CallBuiltin("instance_number", { eobj }).ToDouble();
            for (int n = 0; n < total && n < 600; ++n) {
                RValue id = g_Yytk->CallBuiltin("instance_find", { eobj, RValue((double)n) });
                E e; e.rarity = (int)HhReadNumber(id, "enemyRarity", -1.0);
                e.x = HhReadNumber(id, "x", 0.0); e.y = HhReadNumber(id, "y", 0.0);
                try {
                    RValue flags = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("enemyAffix") });
                    if (flags.m_Kind == VALUE_ARRAY) {
                        int len = (int)g_Yytk->CallBuiltin("array_length", { flags }).ToDouble();
                        for (int i = 0; i < len && i < 128; ++i) {
                            RValue f = g_Yytk->CallBuiltin("array_get", { flags, RValue((double)i) });
                            double v = (f.m_Kind == VALUE_BOOL) ? (f.ToBoolean() ? 1.0 : 0.0) : ((f.m_Kind == VALUE_REAL || f.m_Kind == VALUE_INT32 || f.m_Kind == VALUE_INT64) ? f.ToDouble() : 0.0);
                            if (v != 0.0) e.idx.push_back(i);
                        }
                    }
                } catch (...) {}
                if (e.rarity < 2 && e.idx.empty()) continue;
                try { RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("object_index") }); e.obj = g_Yytk->CallBuiltin("object_get_name", { oi }).ToString(); } catch (...) { e.obj = "?"; }
                enemies.push_back(e);
            }
            RValue bobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Health_Bar_Parent_obj") });
            int bars = (int)g_Yytk->CallBuiltin("instance_number", { bobj }).ToDouble();
            int paired = 0;
            for (int n = 0; n < bars && n < 600; ++n) {
                RValue id = g_Yytk->CallBuiltin("instance_find", { bobj, RValue((double)n) });
                std::string names;
                try {
                    RValue v = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("affixName") });
                    if (v.m_Kind == VALUE_ARRAY) {
                        int len = (int)g_Yytk->CallBuiltin("array_length", { v }).ToDouble();
                        for (int i = 0; i < len && i < 8; ++i) { RValue s = g_Yytk->CallBuiltin("array_get", { v, RValue((double)i) }); names += (i ? "|" : "") + s.ToString(); }
                    } else if (v.m_Kind == VALUE_STRING) names = v.ToString();
                } catch (...) {}
                if (names.empty()) continue;
                double bx = HhReadNumber(id, "x", 0.0), by = HhReadNumber(id, "y", 0.0);
                std::string title; try { title = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("healthBarName") }).ToString(); } catch (...) {}
                const E* best = nullptr; double bestD = 1e18;
                for (const E& e : enemies) { double d = (e.x - bx) * (e.x - bx) + (e.y - by) * (e.y - by); if (d < bestD) { bestD = d; best = &e; } }
                std::string line = "hhpair bar[" + std::to_string(n) + "] \"" + title + "\" affixes=[" + names + "]";
                if (best) {
                    line += " -> " + best->obj + " rarity=" + std::to_string(best->rarity) + " idx=[";
                    for (size_t k = 0; k < best->idx.size(); ++k) line += (k ? "," : "") + std::to_string(best->idx[k]);
                    line += "] dist=" + std::to_string((int)std::sqrt(bestD));
                    ++paired;
                }
                Out(line);
            }
            Out("hhscan: " + std::to_string(total) + " enemies (" + std::to_string(enemies.size()) + " rare/flagged), " + std::to_string(bars) + " bars, " + std::to_string(paired) + " named pairs");
        } catch (...) { Out("hhscan EXCEPTION"); }
    } else if (lc == "icall") {
        // icall <Script> <obj> <n> [args...] -- run gml_Script_<Script> with self = the n-th
        // instance of <obj>, through YYTK's InvokeWithObject (a real `with` scope).
        // GetInstanceObject cannot resolve menu-room instances, InvokeWithObject can.
        std::string scr, r2; scr = FirstToken(rest, r2);
        std::string obj, r3; obj = FirstToken(r2, r3);
        std::string nStr, r4; nStr = FirstToken(r3, r4);
        try {
            int want = std::stoi(nStr);
            RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue(obj) });
            if (oi.ToDouble() < 0) { Out("icall: unknown object " + obj); }
            else {
                std::vector<RValue> args; std::stringstream ss(r4); std::string tok;
                while (ss >> tok) {
                    if (tok == "true") { args.push_back(RValue(true)); continue; }
                    if (tok == "false") { args.push_back(RValue(false)); continue; }
                    bool numeric = false;
                    try { size_t pos; double d = std::stod(tok, &pos); if (pos == tok.size()) { args.push_back(RValue(d)); numeric = true; } } catch (...) {}
                    if (!numeric) args.push_back(RValue(tok));
                }
                int seen = 0; bool done = false; std::string report;
                AurieStatus st = g_Yytk->InvokeWithObject(oi, [&](CInstance* self, CInstance* other) {
                    if (done || seen++ != want) return;
                    done = true;
                    try {
                        RValue res; AurieStatus cs = g_Yytk->CallGameScriptEx(res, "gml_Script_" + scr, self, other ? other : self, args);
                        report = "icall " + scr + "(" + obj + "[" + nStr + "], " + std::to_string(args.size()) + " args) st=" + std::to_string((int)cs) + " -> " + Describe(res);
                    } catch (...) { report = "icall EXCEPTION inside"; }
                });
                if (!done) Out("icall: no " + obj + "[" + nStr + "] (invoke st=" + std::to_string((int)st) + ", instances seen=" + std::to_string(seen) + ")");
                else Out(report);
            }
        } catch (...) { Out("icall EXCEPTION"); }
    } else if (lc == "pcall") {
        // pcall <Script> [args...] -- call gml_Script_<Script> with self = first Player_obj.  Numeric tokens -> real,
        // true/false -> bool, else string.  Result is Describe'd and, for structs/arrays, json_stringify'd to bp_ipc\pcall.json.
        std::string scr, r2; scr = FirstToken(rest, r2);
        try {
            RValue pobj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
            RValue pid = g_Yytk->CallBuiltin("instance_find", { pobj, RValue(0.0) });
            if (pid.ToDouble() < 0) { Out("pcall: no player"); }
            else {
                CInstance* player = nullptr; g_Yytk->GetInstanceObject((int32_t)pid.ToDouble(), player);
                std::vector<RValue> args; std::stringstream ss(r2); std::string tok;
                while (ss >> tok) {
                    if (tok == "true") { args.push_back(RValue(true)); continue; }
                    if (tok == "false") { args.push_back(RValue(false)); continue; }
                    bool numeric = false;
                    try { size_t pos; double d = std::stod(tok, &pos); if (pos == tok.size()) { args.push_back(RValue(d)); numeric = true; } } catch (...) {}
                    if (!numeric) args.push_back(RValue(tok));
                }
                RValue res; AurieStatus st = g_Yytk->CallGameScriptEx(res, "gml_Script_" + scr, player, player, args);
                Out("pcall " + scr + "(" + std::to_string(args.size()) + " args) st=" + std::to_string((int)st) + " -> " + Describe(res));
                if (res.m_Kind == VALUE_OBJECT || res.m_Kind == VALUE_ARRAY) {
                    try {
                        RValue js = g_Yytk->CallBuiltin("json_stringify", { res }); std::string s = js.ToString();
                        std::ofstream f(IPC_DIR + "\\pcall.json", std::ios::binary); f << s;
                        Out("pcall: " + std::to_string(s.size()) + " bytes -> pcall.json json=" + s.substr(0, 240));
                    } catch (...) { Out("pcall: json_stringify failed"); }
                }
            }
        } catch (...) { Out("pcall EXCEPTION"); }
#endif
    } else {
        return false;
    }
    return true;
}

static void RunCommand(const std::string& line)
{
    std::string rest;
    std::string cmd = FirstToken(line, rest);
    if (cmd.empty()) return;
    std::string lc = Lower(cmd);

#ifdef FORGEPACT_RELEASE
    // Player builds accept only commands emitted by the ForgePact panel.  The
    // research build keeps the inspection, arbitrary write, spawn and manual
    // hook commands below; none of those surfaces are available to players.
    static const std::unordered_set<std::string> kPlayerCommands = {
        "ping", "density", "reveal", "specialrate", "dropmult",
        "stat", "statadd", "raredrop", "droprate", "dungeonkey",
        "headhunter", "hhdur", "hhmap", "hhdefault", "hhlabel", "tyrant", "beacon", "beaconrange", "beaconmode", "beaconwake", "beaconspawn", "beaconfarstep", "tyrantchance", "tyrantaffix", "hhlabelfont", "hhlabeloffset", "hhlabelmax",
        "enemyspeed", "rarity", "sigdrop", "angelicdrop", "relicfilter", "orbpickup", "satmods", "petquest"
    };
    if (kPlayerCommands.find(lc) == kPlayerCommands.end()) {
        Out("command unavailable in player build: " + cmd);
        return;
    }
#endif

    if (HandleHeadhunterCommand(lc, rest)) return;
    if (lc == "relicfilter") {
        bool enable = (rest == "1" || rest == "true" || rest == "on");
        // Hooking DropRelic while character selection is still running stalls the
        // runner.  Arm it instead: the frame callback installs the hook once the
        // setup gate has passed and a real player instance exists, so the panel
        // can send this at launch (build_cmds) and it still applies in-game.
        ForgePact::RelicFilterMod::Instance().SetEnabled(enable, g_Orig_DropRelic != nullptr);
    } else if (lc == "orbpickup") {
        std::string ov = Lower(rest);
        while (!ov.empty() && std::isspace((unsigned char)ov.back())) ov.pop_back();
        if (ov == "stat") { OrbPickupStats(); return; }
        bool enable = (ov == "10" || ov == "1" || ov == "true" || ov == "on");
        g_OrbPickupRadius.store(enable);
        if (enable) {
            ResolveOrbAssets();
            char ob[180];
            sprintf_s(ob, "orbpickup -> globes pulled from %.0f px (player obj=%d, globe objs=%zu, driven per frame)",
                      kGlobeBaseRadius * kOrbPickupFactor, g_PlayerObjIdx, g_GlobeObjIdx.size());
            Out(ob);
        } else {
            Out("orbpickup -> OFF");
            OrbPickupStats();
        }
    } else if (lc == "petquest") {
        std::string pv = Lower(rest);
        while (!pv.empty() && std::isspace((unsigned char)pv.back())) pv.pop_back();
#ifndef FORGEPACT_RELEASE
        if (pv == "stat") { PetQuestCollectorStats(); return; }
#endif
        // `petquest arg <n>` - the one value in the collect call that was
        // reproduced rather than understood. The measured collect passed 1;
        // if an item with questValue > 1 ever under-credits, this is the knob
        // to test with before anyone rebuilds.
        if (pv.rfind("arg", 0) == 0) {
            std::string a = pv.substr(3);
            while (!a.empty() && std::isspace((unsigned char)a.front())) a.erase(a.begin());
            if (a.empty()) { Out("petquest arg = " + std::to_string(g_PetQuestArg.load()) + " (1 = the value measured on a real collect)"); return; }
            try { g_PetQuestArg.store(std::stod(a)); Out("petquest arg -> " + a); }
            catch (...) { Out("petquest arg: usage -> petquest arg <number>"); }
            return;
        }
        bool enable = (pv == "1" || pv == "true" || pv == "on");
        ForgePact::PetQuestCollectorMod::Instance().SetEnabled(enable);
        if (!enable) PetQuestCollectorStats();
    } else if (lc == "ping") {
        short ma=0, mi=0, pa=0; g_Yytk->QueryVersion(ma, mi, pa);
        Out("pong (YYTK " + std::to_string(ma) + "." + std::to_string(mi) + "." + std::to_string(pa) + ")");
    } else if (lc == "script") {
        DoScriptLookup(rest);
    } else if (lc == "exists") {
        DoExists(rest);
    } else if (lc == "estforce") {
        std::string idx, val; idx = FirstToken(rest, val);
        try {
            int i = std::stoi(idx); double v = std::stod(val);
            g_EstForce[i] = v;
            Out("estforce eSt[" + std::to_string(i) + "] -> her karede " + std::to_string(v));
        } catch (...) { Out("estforce: kullanim -> estforce 0 0"); }
    } else if (lc == "estfree") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "all" || v.empty()) { g_EstForce.clear(); Out("estfree: tum zorlamalar kaldirildi"); }
        else {
            try { g_EstForce.erase(std::stoi(v)); Out("estfree: " + v + " birakildi"); }
            catch (...) { Out("estfree: kullanim -> estfree 0 | estfree all"); }
        }
    } else if (lc == "eststat") {
        EstStat();
    } else if (lc == "debuglog") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "off") { g_GameLogOn = false; Out("debuglog: KAPALI (satir=" + std::to_string(g_GameLogLines) + ")"); }
        else {
            if (!g_Orig_DebugLogAddExt)
                HookOneScript("DebugLogAddExt", "bp_dbglog", (PVOID)Hook_DebugLogAddExt, &g_Orig_DebugLogAddExt);
            g_GameLogOn = (g_Orig_DebugLogAddExt != nullptr);
            Out(std::string("debuglog: ") + (g_GameLogOn ? "ACIK -> bp_ipc\\gamelog.txt" : "kanca kurulamadi"));
        }
    } else if (lc == "abysstrace") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "off") { g_AbyssTraceOn = false; Out("abysstrace: KAPALI"); }
        else {
            if (!g_Orig_AbyssMech)
                HookOneScript("anon@119@gml_Object_Spawn_Abyss_obj_Create_0",
                              "bp_abyss", (PVOID)Hook_AbyssMech, &g_Orig_AbyssMech);
            if (!g_Orig_GPV_Trace)
                HookOneScript("GPV", "bp_gpvtrace", (PVOID)Hook_GPV_Trace, &g_Orig_GPV_Trace);
            g_AbyssTraceOn = (g_Orig_AbyssMech != nullptr);
            Out(std::string("abysstrace: ") + (g_AbyssTraceOn ? "ACIK -> bp_ipc\\abyss.txt" : "mekanik kancasi kurulamadi"));
        }
    } else if (lc == "logcreate") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "off" || v.empty()) { g_LogCreatePos.clear(); Out("logcreate: kapali"); }
        else {
            try {
                g_LogCreatePos.insert(std::stoi(v));
                Out("logcreate: " + v + " -> bp_ipc\\createpos.txt");
            } catch (...) { Out("logcreate: kullanim -> logcreate 3"); }
        }
    } else if (lc == "pullnear") {
        std::string a1, a2; a1 = FirstToken(rest, a2);
        std::string v = Lower(a1);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "off" || v.empty()) { g_PullNear.clear(); Out("pullnear: kapali"); }
        else {
            try {
                g_PullNear.insert(std::stoi(v));
                if (!a2.empty()) { try { g_PullRadius = std::stod(a2); } catch (...) {} }
                Out("pullnear: " + v + " -> oyuncunun " + std::to_string((int)g_PullRadius) + " birim yakinina");
            } catch (...) { Out("pullnear: kullanim -> pullnear 3 700"); }
        }
    } else if (lc == "destroywatch") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "off" || v.empty()) { g_DestroyWatch.clear(); Out("destroywatch: kapali (yakalanan=" + std::to_string(g_DestroyHits) + ")"); }
        else {
            try {
                g_DestroyWatch.insert(std::stoi(v));
                if (!g_OrigDestroy)
                    HookBuiltin("instance_destroy", "bp_destroy", (PVOID)HookDestroy, &g_OrigDestroy);
                Out("destroywatch: " + v + " -> bp_ipc\\destroy.txt");
            } catch (...) { Out("destroywatch: kullanim -> destroywatch 3"); }
        }
    } else if (lc == "abyssforce") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "stat") {
            Out("abyssforce: mekanik=" + std::to_string(g_AbyssRuns)
                + " sorgu=" + std::to_string(g_ObtainCalls)
                + " olmaz=" + std::to_string(g_ObtainFalse)
                + " zorlanan=" + std::to_string(g_ObtainForced));
        } else if (v == "off") {
            g_ForceObtain = false; Out("abyssforce: KAPALI");
        } else {
            if (!g_Orig_AbyssMech)
                HookOneScript("anon@119@gml_Object_Spawn_Abyss_obj_Create_0",
                              "bp_abyss", (PVOID)Hook_AbyssMech, &g_Orig_AbyssMech);
            if (!g_Orig_Obtain)
                HookOneScript("IsObtainablePlace", "bp_obtain",
                              (PVOID)Hook_Obtain, &g_Orig_Obtain);
            g_ForceObtain = (g_Orig_AbyssMech != nullptr && g_Orig_Obtain != nullptr);
            Out(std::string("abyssforce: ") + (g_ForceObtain ? "ACIK" : "kanca kurulamadi"));
        }
    } else if (lc == "gaget") {
        std::string name, idx; name = FirstToken(rest, idx);
        try { GlobalArrayGet(name, std::stoi(idx)); }
        catch (...) { Out("gaget: kullanim -> gaget eSt 0"); }
    } else if (lc == "gaset") {
        std::string name, r2; name = FirstToken(rest, r2);
        std::string idx, val; idx = FirstToken(r2, val);
        try { GlobalArraySet(name, std::stoi(idx), std::stod(val)); }
        catch (...) { Out("gaset: kullanim -> gaset eSt 0 0"); }
    } else if (lc == "get") {
        DoGet(rest);
    } else if (lc == "dump") {
        DoDump(rest);
    } else if (lc == "setn") {
        std::string n, num; n = FirstToken(rest, num); DoSetNum(n, num);
    } else if (lc == "sets") {
        std::string n, val; n = FirstToken(rest, val); DoSetStr(n, val);
    } else if (lc == "call") {
        std::string n, arg; n = FirstToken(rest, arg);
        DoCall(n, arg, !arg.empty());
    } else if (lc == "callfile") {
        std::string n, path; n = FirstToken(rest, path); DoCallFile(n, path);
    } else if (lc == "calljson") {
        std::string n, path; n = FirstToken(rest, path); DoCallJson(n, path);
    } else if (lc == "structdump") {
        DoStructDump(rest);
    } else if (lc == "routineptr") {
        DoRoutinePtr(rest);
    } else if (lc == "readmem") {
        std::string a, l; a = FirstToken(rest, l); DoReadMem(a, l);
    } else if (lc == "hookon") {
        LoadConfig(); InstallHook();
    } else if (lc == "hookstats") {
        HookStats();
#ifndef FORGEPACT_RELEASE
    } else if (lc == "necrobal") {
        std::string value = Lower(rest);
        while (!value.empty() && std::isspace((unsigned char)value.back())) value.pop_back();
        if (value.empty() || value == "status" || value == "stat") NecroBalanceStatus();
        else if (value == "1" || value == "on" || value == "true") SetNecroBalance(true);
        else if (value == "0" || value == "off" || value == "false") SetNecroBalance(false);
        else Out("necrobal: usage -> necrobal 0|1  (no argument = status)");
    } else if (lc == "citrace") {
        // Pet Quest Collector Phase 0 research - see docs/pet-quest-collector-plan.md
        // and docs/pet-quest-collector-plan-b4-input-simulation.md.
        std::string sub, subRest;
        sub = FirstToken(rest, subRest);
        std::string subLc = Lower(sub);
        if (subLc == "stat" || subLc.empty()) { CiTraceStats(); return; }
        if (subLc == "help" || subLc == "?") {
            // The subcommand surface grew a lot with Plan C's Phase C0 batch;
            // this is the one place a live tester can re-read it without
            // leaving the game.
            Out("citrace subcommands:");
            Out("  0|1                      tracing off/on (installs the trace hooks)   stat  counters");
            Out("  snap1 / snap2            whole-state snapshot + diff (B4 Phase 0)");
            Out("  pokekey / pokekey2 / mouse / mousewrite   B4 Phase 0 input probes (all measured negative)");
            Out("  -- Plan C Phase C0, read-only --");
            Out("  item                     nearest quest item, all vars, method-resolved   (C0.1)");
            Out("  methods                  the six m_Quest* methods + invocation builtins  (C0.1/C0.2)");
            Out("  dumpobj <Obj> [nth]      any object instance, method-resolved            (C0.4)");
            Out("  player                   the local player, method-resolved               (C0.4)");
            Out("  globals                  the hover/target globals read as values         (C0.5)");
            Out("  sweep [extra ...]        broadened global sweep with container expansion (C0.5)");
            Out("  -- Plan C Phase C2, MUTATING (back up the save first, plan C 4) --");
            Out("  collect confirm [path] [args]   m_Questpickup on the nearest quest item - THE MEASURED CALL");
            Out("      self=item other=Loot_Manager_obj arg=1; refuses unless canPickup && lootType==0, as the game does");
            Out("  -- Plan C Phase C0, MUTATING (back up the save first, plan C 4) --");
            Out("  invoke item|global <name> confirm [path] [args...]   call a bound method value  (C0.2)");
            Out("      [path] with|withex (InvokeWithObject - the untried route) |builtin|builtinex|index|indexex");
            Out("             |methodcall|script|scriptex ; auto = in order, stop at first fault ; all = sweep anyway");
            Out("      [args] numbers, or player|item|noone|true|false  (MEASURED: cold no-arg calls fault)");
            Out("  event <type> <number> confirm              event_perform on the item     (C0.3)");
            Out("  eventobj <Obj> <type> <number> confirm     event_perform_object          (C0.3)");
            Out("  activate [Obj] confirm                     instance_activate_object      (C0.6)");
            Out("  -- Plan C Phase C1, native-analysis infrastructure --");
            Out("  symdump [start] [end]    whole script table -> bp_ipc\symbols.csv (name -> RVA)");
            Out("  dispatchdump [rva] [max] builtin table -> bp_ipc\builtins.csv (id -> name,argc,RVA)");
            Out("  dispatchtrace arm|now|show  hook the dispatcher; log EVERY builtin call, by name");
            Out("  stackwalk [n]            capture who calls keyboard_check_pressed(70); needs citrace 1 first");
            Out("  nativetrace [on|show|reset]  detour the collect chain at its REAL address (MmCreateHook),");
            Out("      not via the script table - read-only. `show` prints native vs table counts side by side.");
            return;
        }
        if (subLc == "snap1") { CiSnapTake(); return; }
        if (subLc == "snap2") { CiSnapDiff(); return; }
        if (subLc == "pokekey") {
            std::string idxTok, valTok; idxTok = FirstToken(subRest, valTok);
            try {
                int idx = std::stoi(idxTok);
                double val = valTok.empty() ? 1.0 : std::stod(valTok);
                CiPokeKey(idx, val);
            } catch (...) { Out("citrace pokekey: usage -> citrace pokekey <index> [value=1]"); }
            return;
        }
        if (subLc == "pokekey2") {
            // Pokes two inputState indices simultaneously, in the same frame -
            // see docs/pet-quest-collector-b4-research.md item 1: a real F
            // press flips indices 30 and 60 together, and neither alone (both
            // tried live) produced any visible in-game reaction.
            std::string i1Tok, r2; i1Tok = FirstToken(subRest, r2);
            std::string i2Tok, valTok; i2Tok = FirstToken(r2, valTok);
            try {
                int idx1 = std::stoi(i1Tok);
                int idx2 = std::stoi(i2Tok);
                double val = valTok.empty() ? 0.0 : std::stod(valTok);
                CiPokeKeys2(idx1, idx2, val);
            } catch (...) { Out("citrace pokekey2: usage -> citrace pokekey2 <index1> <index2> [value=0]"); }
            return;
        }
        if (subLc == "mouse") { CiMouseReport(); return; }
        if (subLc == "mousewrite") {
            std::string xTok, yTok; xTok = FirstToken(subRest, yTok);
            try { CiMouseWriteTest(std::stod(xTok), std::stod(yTok)); }
            catch (...) { Out("citrace mousewrite: usage -> citrace mousewrite <x> <y>"); }
            return;
        }
        // ---- Plan C Phase C0 (docs/pet-quest-collector-plan-c-direct-invocation.md §2) ----
        // Read-only first; the four mutating subcommands below each require a
        // literal `confirm` token (plan §4) so a stray line in cmd.txt cannot
        // fire one.
        if (subLc == "symdump") {
            // Phase C1: the whole runtime script table as index,name,rva.
            // Lives under `citrace` rather than as its own top-level command
            // for two reasons: it inherits the research-build guard, and
            // RunCommand's else-if chain is already at MSVC's block-nesting
            // limit (C1061) - one more top-level branch does not compile.
            int a = 100000, b = 110000;
            bool bad = false;
            try {
                std::string t1, r1; t1 = FirstToken(subRest, r1);
                if (!t1.empty()) a = std::stoi(t1);
                std::string t2, r2; t2 = FirstToken(r1, r2);
                if (!t2.empty()) b = std::stoi(t2);
            } catch (...) { bad = true; }
            if (bad) Out("citrace symdump: usage -> citrace symdump [startIndex=100000] [endIndex=110000]");
            else SymDump(a, b);
            return;
        }
        if (subLc == "dispatchdump") {
            // Phase C1 §3.2 step 3: the builtin ID -> name table, read live.
            // Optional args: <tablePtrRvaHex> <maxEntries>.
            unsigned long long rva = kCiDispatchTablePtrRvaDefault;
            int maxN = 20000;   // the table's real length is unknown; cap rather than guess low
            bool bad = false;
            try {
                std::string t1, r1; t1 = FirstToken(subRest, r1);
                if (!t1.empty()) rva = std::stoull(t1, nullptr, 16);
                std::string t2, r2; t2 = FirstToken(r1, r2);
                if (!t2.empty()) maxN = std::stoi(t2);
            } catch (...) { bad = true; }
            if (bad) Out("citrace dispatchdump: usage -> citrace dispatchdump [tablePtrRvaHex] [maxEntries=5000]");
            else CiDispatchDump(rva, maxN);
            return;
        }
        if (subLc == "dispatchtrace") {
            // Phase C1: hook the builtin dispatcher and record every call the
            // game makes during the collect window, resolved to names.
            //   citrace dispatchtrace arm    - install (lazily) + arm on next F press
            //   citrace dispatchtrace now    - arm immediately (captures whatever runs next)
            //   citrace dispatchtrace show   - stop and write the capture out
            std::string mode, rest2; mode = Lower(FirstToken(subRest, rest2));
            if (mode == "show") { CiDispatchWrite(kCiDispatchTablePtrRvaDefault); return; }
            if (mode.empty() || mode == "arm" || mode == "now") {
                if (!CiInstallDispatchHook(kCiDispatchFnRvaDefault)) return;
                if (mode == "now") {
                    g_CiDispN.store(0);
                    g_CiDispArmed.store(true);
                    Out("citrace dispatchtrace: capturing NOW (up to " + std::to_string(kCiDispCap) + " calls) - run `citrace dispatchtrace show` to write it out");
                } else {
                    g_CiDispArmOnFPress.store(true);
                    Out("citrace dispatchtrace: armed - capture starts on the next F-press edge (needs `citrace 1` so the key hook exists).");
                    Out("  hover a quest item, press F, then run `citrace dispatchtrace show`.");
                }
                return;
            }
            Out("citrace dispatchtrace: usage -> citrace dispatchtrace arm|now|show");
            return;
        }
        if (subLc == "nativetrace") {
            // Phase C1: the same targets the table-swap hooks already cover,
            // detoured at their real addresses instead. Read-only.
            //   citrace nativetrace          - install the detours
            //   citrace nativetrace show     - native vs table call counts
            //   citrace nativetrace reset    - zero the counters between runs
            std::string mode, rest2; mode = Lower(FirstToken(subRest, rest2));
            if (mode == "show") { CiNativeTraceReport(); return; }
            if (mode == "reset") { CiNativeTraceReset(); return; }
            if (mode.empty() || mode == "on" || mode == "install") { CiNativeTraceInstall(); return; }
            Out("citrace nativetrace: usage -> citrace nativetrace [on] | show | reset");
            return;
        }
        if (subLc == "stackwalk") {
            // Phase C1 §3.2 step 1: arm a backtrace capture on the next N real
            // F-press edges. Needs `citrace 1` first - that is what installs
            // the keyboard_check_pressed hook this fires from.
            int n = 1; try { if (!subRest.empty()) n = std::stoi(FirstToken(subRest, subRest)); } catch (...) { n = 1; }
            if (n < 1) n = 1; if (n > 10) n = 10;
            g_CiStackWalkLeft.store(n);
            Out("citrace stackwalk: armed for the next " + std::to_string(n) + " F-press edge(s).");
            if (!g_CiTraceOn.load()) Out("  WARNING: tracing is off, so the keyboard_check_pressed hook is not installed - run `citrace 1` first or this will never fire.");
            else Out("  now press F in-game (hovering a quest item makes the capture most useful).");
            return;
        }
        if (subLc == "item") { CiDumpNearestQuestItem(); return; }
        if (subLc == "methods") { CiReportMethods(); return; }
        if (subLc == "player") { CiDumpPlayer(); return; }
        if (subLc == "globals") { CiReportHoverGlobals(); return; }
        if (subLc == "sweep") { CiSweepGlobals(subRest); return; }
        if (subLc == "dumpobj") {
            std::string objName, nthTok; objName = FirstToken(subRest, nthTok);
            int nth = 0;
            if (!nthTok.empty()) { try { nth = std::stoi(nthTok); } catch (...) { Out("citrace dumpobj: usage -> citrace dumpobj <ObjectName> [nth=0]"); return; } }
            CiDumpNamedObject(objName, nth);
            return;
        }
        if (subLc == "invoke") {
            // citrace invoke item|global <name> confirm [path] [args...]
            static const char* const kInvokeUsage =
                "citrace invoke item|global <name> confirm [scriptref|native|auto|all|with|withex|builtin|builtinex|index|indexex|methodcall|script|scriptex] [args...]"
                "   (args: numbers, or player|item|noone|true|false)";
            std::string kind, r1; kind = FirstToken(subRest, r1);
            std::string nameTok, r2; nameTok = FirstToken(r1, r2);
            std::string confirmTok, r3; confirmTok = FirstToken(r2, r3);
            std::string pathTok, argTokens; pathTok = FirstToken(r3, argTokens);
            std::string kindLc = Lower(kind);
            if ((kindLc != "item" && kindLc != "global") || nameTok.empty()) { Out(std::string("citrace invoke: usage -> ") + kInvokeUsage); return; }
            if (!CiConfirmed(confirmTok, kInvokeUsage)) return;
            if (kindLc == "item") CiInvokeItemMethod(nameTok, pathTok, argTokens);
            else CiInvokeGlobalMethod(nameTok, pathTok, argTokens);
            return;
        }
        if (subLc == "collect") {
            // Phase C2 gate: the measured call shape, on the nearest quest item.
            // citrace collect confirm [path] [args...]
            static const char* const kCollectUsage =
                "citrace collect confirm [scriptref|native|auto|all|with|withex|builtin|builtinex|index|indexex|methodcall|script|scriptex] [args...]"
                "   (default path scriptref - the shipped one; `native` is the old fixed-address shape, A/B only."
                "    default arg 1, as measured on a real collect)";
            std::string confirmTok, r1; confirmTok = FirstToken(subRest, r1);
            std::string pathTok, argTokens; pathTok = FirstToken(r1, argTokens);
            if (!CiConfirmed(confirmTok, kCollectUsage)) return;
            CiCollectNearestQuestItem(pathTok.empty() ? std::string("scriptref") : pathTok, argTokens);
            return;
        }
        if (subLc == "event") {
            std::string tTok, r1; tTok = FirstToken(subRest, r1);
            std::string nTok, confirmTok; nTok = FirstToken(r1, confirmTok);
            int type = 0, number = 0;
            try { type = std::stoi(tTok); number = std::stoi(nTok); }
            catch (...) { Out("citrace event: usage -> citrace event <type> <number> confirm  (e.g. 6 10 = ev_mouse/ev_mouse_enter)"); return; }
            if (!CiConfirmed(FirstToken(confirmTok, confirmTok), "citrace event <type> <number> confirm")) return;
            CiEventPerform(type, number, "");
            return;
        }
        if (subLc == "eventobj") {
            std::string objName, r1; objName = FirstToken(subRest, r1);
            std::string tTok, r2; tTok = FirstToken(r1, r2);
            std::string nTok, confirmTok; nTok = FirstToken(r2, confirmTok);
            int type = 0, number = 0;
            try { type = std::stoi(tTok); number = std::stoi(nTok); }
            catch (...) { Out("citrace eventobj: usage -> citrace eventobj <ObjectName> <type> <number> confirm"); return; }
            if (objName.empty()) { Out("citrace eventobj: usage -> citrace eventobj <ObjectName> <type> <number> confirm"); return; }
            if (!CiConfirmed(FirstToken(confirmTok, confirmTok), "citrace eventobj <ObjectName> <type> <number> confirm")) return;
            CiEventPerform(type, number, objName);
            return;
        }
        if (subLc == "activate") {
            // `citrace activate confirm` (family default) or
            // `citrace activate <ObjectName> confirm`.
            std::string first, r1; first = FirstToken(subRest, r1);
            std::string objName = (Lower(first) == "confirm") ? std::string() : first;
            std::string confirmTok = (Lower(first) == "confirm") ? first : FirstToken(r1, r1);
            if (!CiConfirmed(confirmTok, "citrace activate [ObjectName] confirm")) return;
            CiActivateObject(objName);
            return;
        }
        bool enable = (subLc == "1" || subLc == "true" || subLc == "on");
        g_CiTraceOn.store(enable);
        if (enable) InstallCiTraceHooks();
        CiTraceStats();
#endif
    } else if (lc == "reloadcfg") {
        LoadConfig();
    } else if (lc == "enemystats") {
        EnemyStats();
    } else if (lc == "createlog") {
        CreateLog(false);
    } else if (lc == "enemylog") {
        CreateLog(true);
    } else if (lc == "enemyall") {
        try { g_EnemyMultAll = std::stoi(rest); Out("enemyall -> " + std::to_string(g_EnemyMultAll)); }
        catch (...) { Out("enemyall: bad value"); }
    } else if (lc == "density") {
        ForgePact::DensityManager::Instance().HandleCommand(rest);
    } else if (lc == "dropstats") {
        DropStats();
    } else if (lc == "dropmult") {
        std::string nm, num; nm = FirstToken(rest, num);
        try { SetDropMult(nm, std::stoi(num)); } catch (...) { Out("dropmult: bad args (e.g. dropmult relic 5)"); }
    } else if (lc == "forcerelic") {
        int n = 1; try { n = std::stoi(rest); } catch (...) {}
        ForceRelicDrop(n);
    } else if (lc == "relicgate") {
        SetRelicGate(rest == "1" || rest == "on" || rest == "true");
    // NOTE: `relicfilter` is handled at the top of RunCommand.  A second branch
    // here was unreachable and installed a different (eager) hook set, which made
    // the command read as if it did two contradictory things.
#ifndef FORGEPACT_RELEASE
    } else if (lc == "scount") {
        SCountCmd(rest);
    } else if (lc == "gpvlog") {
        std::string v = Lower(rest);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        if (v == "off") { g_GpvLog = 0; Out("gpvlog -> KAPALI"); }
        else if (v == "stat") {
            Out("gpvlog: " + std::to_string(g_GpvGorulen.size()) + " farkli kayit -> bp_ipc\\gpv.txt");
        } else {
            if (!g_OrigGPV) HookOneScriptTable("GPV", "fp_gpv", (PVOID)HookGPV, &g_OrigGPV);
            if (!g_OrigSPV) HookOneScriptTable("SPV", "fp_spv", (PVOID)HookSPV, &g_OrigSPV);
            g_GpvLog = 1;
            Out(std::string("gpvlog -> ACIK  (GPV kanca=") + (g_OrigGPV ? "var" : "YOK")
                + ", SPV kanca=" + (g_OrigSPV ? "var" : "YOK") + ")  -> bp_ipc\\gpv.txt");
        }
#endif
#ifndef FORGEPACT_RELEASE
    } else if (lc == "zonegenlog") {
        std::string v = Lower(rest);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        if (v == "off") { g_ZgLog = 0; Out("zonegenlog: KAPALI"); }
        else { ZoneGenLogKur(); g_ZgLog = 1; g_ZgSira = 0; Out("zonegenlog: ACIK"); }
#endif
    } else if (lc == "gatestats") {
        MechGateStats();
    } else if (lc == "srdiff") {
        std::string v = Lower(rest);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        g_SrAnyDifficulty = !(v == "off" || v == "0" || v == "false");
        Out(std::string("srdiff: Shadow Realm zorluk kapisi ") + (g_SrAnyDifficulty ? "ACILIYOR (her zorluk)" : "VANILYA (GPV68 >= 2)"));
    } else if (lc == "ctdiff") {
        std::string v = Lower(rest);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        g_CtAnyDifficulty = !(v == "off" || v == "0" || v == "false");
        Out(std::string("ctdiff: Chaos Tower zorluk kapisi ") + (g_CtAnyDifficulty ? "ACILIYOR (her zorluk)" : "VANILYA (GPV68 >= 1)"));
    } else if (lc == "ctstats") {
        ChaosTowerStats();
    } else if (lc == "ctforce") {
        std::string v = Lower(rest);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        if (v == "off") { g_ctForce = NAN; Out("ctforce -> OFF"); }
        else { try { g_ctForce = std::stod(v); Out("ctforce -> " + std::to_string(g_ctForce)); } catch (...) { Out("ctforce: bad value"); } }
    } else if (lc == "ctsize") {
        try { g_ctArrayN = std::stoi(rest); Out("ctsize (chaos tower array N) -> " + std::to_string(g_ctArrayN)); }
        catch (...) { Out("ctsize: bad value"); }
    } else if (lc == "ctarray") {
        std::string v = Lower(rest);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        if (v == "off") { g_ctCustom.clear(); Out("ctarray -> OFF"); }
        else {
            g_ctCustom.clear();
            std::stringstream ss(rest); std::string tok;
            while (std::getline(ss, tok, ',')) {
                try { g_ctCustom.push_back(std::stod(tok)); } catch (...) {}
            }
            Out("ctarray -> " + std::to_string(g_ctCustom.size()) + " elements");
        }
    } else if (lc == "proof") {
        char b[280];
        sprintf_s(b, "PROOF: density=x%g | extra spawners created=%ld | revisit expansions blocked=%ld | tracked placements=%llu | extra enemies created=%ld",
            ForgePact::DensityManager::Instance().Mult, g_ExtraCreators, g_DensityRevisitSkips,
            (unsigned long long)DensityPlacementCount(), g_ExtraEnemies);
        Out(b);
    } else if (lc == "clearlog") {
        g_CreateCounts.clear(); Out("create log cleared");
    } else if (lc == "spawnat") {
        try { SpawnAtPlayer(std::stoi(rest)); } catch (...) { Out("spawnat: bad index"); }
    } else if (lc == "cb") {
        CallBuiltinCmd(rest);
#ifndef FORGEPACT_RELEASE
    } else if (lc == "gnames") {
        std::string f = rest;
        while (!f.empty() && std::isspace((unsigned char)f.back())) f.pop_back();
        GlobalNames(f);
    } else if (lc == "ijson") {
        std::string v = rest;
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        PlayerVarJson(v);
    } else if (lc == "ojson") {
        std::string obj, var; obj = FirstToken(rest, var);
        while (!var.empty() && std::isspace((unsigned char)var.back())) var.pop_back();
        ObjVarJson(obj, var);
#endif
    } else if (lc == "gjson") {
        std::string n = rest; while (!n.empty() && (n.back()=='\r'||n.back()=='\n'||n.back()==' ')) n.pop_back();
        GJson(n);
    } else if (lc == "itemjson") {
        std::string p = rest; while (!p.empty() && (p.back()=='\r'||p.back()=='\n'||p.back()==' ')) p.pop_back();
        ItemJson(p);
    } else if (lc == "jstat") {
        std::string p = rest; while (!p.empty() && (p.back()=='\r'||p.back()=='\n'||p.back()==' ')) p.pop_back();
        JStat(p);
    } else if (lc == "naddr") {
        std::string n = rest; while (!n.empty() && (n.back()=='\r'||n.back()=='\n'||n.back()==' ')) n.pop_back();
        NAddr(n);
    } else if (lc == "naddrall") {
        NAddrAll();
    } else if (lc == "naddrorig") {
        std::string n = rest; while (!n.empty() && (n.back()=='\r'||n.back()=='\n'||n.back()==' ')) n.pop_back();
        NAddrOrig(n);
    } else if (lc == "sweep") {
        std::stringstream s(rest); int lo=0, hi=0, slot=0; s >> lo >> hi; if (!(s >> slot)) slot = 0;
        g_SweepLo = lo; g_SweepHi = hi; g_SweepSlot = slot; g_SweepArmed = true;
        Out("sweep armed: n[" + std::to_string(slot) + "]=" + std::to_string(lo) + ".." + std::to_string(hi) + " -> now hover ANY jewel in-game");
    } else if (lc == "callnum") {
        CallNum(rest);
    } else if (lc == "spawnitem") {
        std::string p = rest; while (!p.empty() && (p.back()=='\r'||p.back()=='\n'||p.back()==' ')) p.pop_back();
        SpawnItem(p);
    } else if (lc == "pfind") {
        std::string v, val; v = FirstToken(rest, val);
        try { PFind(v, std::stod(val)); } catch (...) { Out("pfind: e.g. pfind pSt 35"); }
    } else if (lc == "pget") {
        std::string v, idx; v = FirstToken(rest, idx);
        try { PGet(v, std::stoi(idx)); } catch (...) { Out("pget: e.g. pget pSt 42"); }
    } else if (lc == "pset") {
        std::string v, r2; v = FirstToken(rest, r2);
        std::string idx, val; idx = FirstToken(r2, val);
        try { PSet(v, std::stoi(idx), std::stod(val)); } catch (...) { Out("pset: e.g. pset pSt 42 1000"); }
    } else if (lc == "inames") {
        std::string obj, flt; obj = FirstToken(rest, flt);
        while (!flt.empty() && (flt.back()=='\r'||flt.back()=='\n'||flt.back()==' ')) flt.pop_back();
        InstanceNames(obj, flt);
    } else if (lc == "oget") {
        std::string obj, var; obj = FirstToken(rest, var);
        while (!var.empty() && (var.back()=='\r'||var.back()=='\n'||var.back()==' ')) var.pop_back();
        ObjVarGet(obj, var);
    } else if (lc == "oset") {
        std::string obj, r2; obj = FirstToken(rest, r2);
        std::string var, num; var = FirstToken(r2, num);
        try { ObjVarSet(obj, var, std::stod(num)); } catch (...) { Out("oset: bad args (e.g. oset objMinimap minimapRevealed 1)"); }
    } else if (lc == "iget") {
        std::string v = rest; while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        PlayerVarGet(v);
    } else if (lc == "iset") {
        std::string v, num; v = FirstToken(rest, num);
        try { PlayerVarSet(v, std::stod(num)); } catch (...) { Out("iset: bad args (e.g. iset uber_sung_lee_killed 0)"); }
    } else if (lc == "spawnname") {
        std::string n = rest;
        while (!n.empty() && (n.back()=='\r'||n.back()=='\n'||n.back()==' ')) n.pop_back();
        SpawnByName(n);
    } else if (lc == "watchobj") {
        try { g_WatchObj = std::stoi(rest); g_WatchCallers.clear(); Out("watchobj -> " + std::to_string(g_WatchObj)); }
        catch (...) { Out("watchobj: bad value"); }
    } else if (lc == "watchcallers") {
        Out("watchobj=" + std::to_string(g_WatchObj) + " callers(rva): " + (g_WatchCallers.empty() ? std::string("(none)") : g_WatchCallers));
    } else if (lc == "specialrate") {
        std::string key, num; key = FirstToken(rest, num);
        while (!num.empty() && (num.back()=='\r'||num.back()=='\n'||num.back()==' ')) num.pop_back();
        try { SpecialRate(Lower(key), std::stoi(num)); }
        catch (...) { Out("specialrate: kullanim -> specialrate rift 3"); }
    } else if (lc == "enemyspeed") {
        EnemySpeedCmd(rest);
    } else if (lc == "reveal") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        auto& mr = ForgePact::MapRevealManager::Instance();
        if (v.rfind("packs", 0) == 0) {
            // `reveal packs 0|1` - the monster half on its own.  The panel
            // has a nested checkbox for it under "Reveal full map", and
            // build_cmds only emits this line to turn it OFF (the plugin
            // defaults it on).
            std::string p = Lower(TrimCopy(v.substr(5)));
            mr.SetPacks(!(p == "0" || p == "off" || p == "false"));
            if (mr.PacksEnabled()) InstallDistanceLieHook();
            return;
        }
#ifndef FORGEPACT_RELEASE
        if (v == "stat" || v == "status") {
            Out(std::string("reveal: ") + (mr.IsEnabled() ? "ON" : "off")
                + " | packs=" + (mr.PacksEnabled() ? "on" : "off")
                + " zonesPopulated=" + std::to_string(mr.PacksZones())
                + " spawnWindowLeft=" + std::to_string(mr.SpawnWindowLeft()) + " frames"
                + " pending=" + (mr.PacksPending() ? ("yes(" + std::to_string(mr.PendingTicks()) + " ticks)") : "no")
                + " creatorLies=" + std::to_string(g_RevealSpawnLies));
            // What the pack pass is actually working on, so a zero above can be
            // told apart from "no spawners here" and from "already populated".
            try {
                long creators = 0;
                for (const char* nm : kBeCreatorObjects) {
                    RValue co = g_Yytk->CallBuiltin("asset_get_index", { RValue(nm) });
                    if (co.ToDouble() >= 0) creators += (long)g_Yytk->CallBuiltin("instance_number", { co }).ToDouble();
                }
                RValue eo = g_Yytk->CallBuiltin("asset_get_index", { RValue("Enemy_Parent_obj") });
                long enemies = eo.ToDouble() >= 0 ? (long)g_Yytk->CallBuiltin("instance_number", { eo }).ToDouble() : -1;
                Out("  this zone: creators awake=" + std::to_string(creators) + " enemies=" + std::to_string(enemies));
            } catch (...) { Out("  this zone: (enumeration threw)"); }
            return;
        }
#endif
        const bool on = !(v == "0" || v == "off" || v == "false");
        mr.SetEnabled(on);
        if (on && mr.PacksEnabled()) InstallDistanceLieHook();
    } else if (lc == "census") {
#ifdef FORGEPACT_RELEASE
        Out("census: yayin derlemesinde yok");
#else
        std::string a1, a2; a1 = FirstToken(rest, a2);
        std::string v = Lower(a1);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (v == "off") { g_CensusOn = false; Out("census: KAPALI"); }
        else {
            if (!a2.empty()) { try { g_CensusEvery = std::stoi(a2); } catch (...) {} }
            if (g_CensusEvery < 10) g_CensusEvery = 10;
            g_CensusOn = true;
            Out("census: ACIK, her " + std::to_string(g_CensusEvery) + " karede -> bp_ipc\\census.txt");
        }
#endif
    } else if (lc == "spread") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (!v.empty()) { try { g_KareBasina = std::stoi(v); } catch (...) {} }
        Out("spread: karede " + std::to_string(g_KareBasina)
            + " yaratim (0=kapali) | bekleyen=" + std::to_string(g_Kuyruk.size())
            + " toplam=" + std::to_string(g_KuyrukToplam));
    } else if (lc == "budget") {
        std::string v = Lower(rest);
        while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        if (!v.empty()) { try { g_OrnekButce = std::stoi(v); } catch (...) {} }
        Out("budget: " + std::to_string(g_OrnekButce) + " ornek (0=sinirsiz)"
            + " | su an=" + std::to_string(ToplamOrnek())
            + " | butce yuzunden atilan=" + std::to_string(g_ButceIptal));
    } else if (lc == "stat") {
        ForgePact::StatsManager::Instance().HandleStatCommand(rest);
    } else if (lc == "statadd") {
        ForgePact::StatsManager::Instance().HandleStatAddCommand(rest);
    } else if (lc == "angelicwatch") {
        // Research: observe the game's own angelic rolls without touching the gate.
        std::string v = Lower(TrimCopy(rest));
        if (v == "on" || v.empty()) {
            if (!g_OrigAngChance) HookOneScript("DropItemAngelicChance", "fp_angch", (PVOID)HookAngelicChance, &g_OrigAngChance);
            g_AngelicRateMult = 1.0;
            InstallHeadhunterHook();
            g_AngChanceCalls = 0; g_KillsSeen = 0;
            Out("angelicwatch: on - counting kills and the game's own angelic rolls (gate untouched)");
        } else if (v == "off") { Out("angelicwatch: hook stays, counting stops being reset"); }
        else Out("angelicwatch: kills=" + std::to_string(g_KillsSeen) + " gameRolls=" + std::to_string(g_AngChanceCalls) + " lastChance=" + std::to_string((long long)g_AngLastChance));
    } else if (lc == "raredrop") {
        RareDropCmd(rest);
    } else if (lc == "satmods") {
        SatModsCmd(rest);
#ifndef FORGEPACT_RELEASE
    } else if (lc == "socketprobe") {
        SocketProbeCmd(rest);
#endif
    } else if (lc == "droprate") {
        DropRateCmd(rest);
    } else if (lc == "dungeonkey") {
        DungeonKeyCmd(rest);
    } else if (lc == "multname") {
        std::string nm, num; nm = FirstToken(rest, num);
        while (!num.empty() && (num.back()=='\r'||num.back()=='\n'||num.back()==' ')) num.pop_back();
        try {
            int n = std::stoi(num);
            RValue idx = g_Yytk->CallBuiltin("asset_get_index", { RValue(nm) });
            int oi = (int)idx.ToDouble();
            if (oi < 0) { Out("multname: '" + nm + "' bulunamadi"); }
            else { SetObjectMultiplier(oi, n); Out("multname " + nm + " (idx " + std::to_string(oi) + ") -> " + num); }
        } catch (...) { Out("multname: kullanim -> multname Spawn_Rift_obj 3"); }
    } else if (lc == "multobj") {
        std::string idx, num; idx = FirstToken(rest, num);
        try { int oi = std::stoi(idx); int n = std::stoi(num); SetObjectMultiplier(oi, n); Out("multobj " + idx + " -> " + num); }
        catch (...) { Out("multobj: bad args"); }
    } else if (lc == "mult") {
        std::string which, num; which = FirstToken(rest, num); which = Lower(which);
        int n = 1; try { n = std::stoi(num); } catch (...) {}
        if (which == "off") { g_MultFreePos = g_MultCreate = g_MultElite = 1; Out("mult -> all OFF"); }
        else if (which == "freepos") { g_MultFreePos = n; Out("mult FreePos -> " + std::to_string(n)); }
        else if (which == "create") { g_MultCreate = n; Out("mult Create -> " + std::to_string(n)); }
        else if (which == "elite") { g_MultElite = n; Out("mult Elite -> " + std::to_string(n)); }
        else if (which == "all") { g_MultFreePos = g_MultCreate = g_MultElite = n; Out("mult ALL -> " + std::to_string(n)); }
        else Out("mult: use freepos|create|elite|all|off <n>");
    } else if (lc == "probestruct") {
        std::string v = Lower(rest);
        g_ProbeStruct = (v.find("off") == std::string::npos);
        Out(std::string("probestruct -> ") + (g_ProbeStruct ? "ON" : "OFF"));
    } else if (lc == "forceslot") {
        std::string v = rest;
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        if (Lower(v) == "off") { g_ForceSlot = NAN; Out("forceslot -> OFF"); }
        else { try { g_ForceSlot = std::stod(v); Out("forceslot -> " + std::to_string(g_ForceSlot)); } catch (...) { Out("forceslot: bad value"); } }
    } else if (lc == "steamid") {
        SteamId();
    } else if (lc == "netscripts") {
        NetScripts();
    } else if (lc == "netdump") {
        NetDump();
    } else if (lc == "netstate") {
        NetState();
    } else if (lc == "p2paccept") {
        P2PAccept(rest);
    } else if (lc == "p2psend") {
        std::string v = rest; while (!v.empty() && (v.back()=='\r'||v.back()=='\n'||v.back()==' ')) v.pop_back();
        P2PSend(v);
    } else if (lc == "p2ppoll") {
        P2PPoll(rest);
    } else if (lc == "p2pstats") {
        P2PStats();
    } else if (lc == "callext") {
        CallExt(rest);
    } else if (lc == "forcelogin") {
        std::string v = Lower(rest);
        g_ForceLogin = (v.find("on") != std::string::npos || v.find("1") != std::string::npos);
        Out(std::string("forcelogin -> ") + (g_ForceLogin ? "ON" : "OFF") + " (IsLoggedIn calls so far=" + std::to_string(g_LoginCalls) + ")");
    } else if (lc == "coopstart") {
        // coopstart            -> load coop.ini
        // coopstart <myport> <peerip> <peerport>
        std::string a, r2; a = FirstToken(rest, r2);
        if (a.empty()) { LoadCoopConfigAndMaybeStart(); }
        else {
            std::string ip, ps; ip = FirstToken(r2, ps);
            while (!ps.empty() && (ps.back()=='\r'||ps.back()=='\n'||ps.back()==' ')) ps.pop_back();
            try { CoopStart(std::stoi(a), ip, std::stoi(ps)); }
            catch (...) { Out("coopstart: usage: coopstart <myport> <peerip> <peerport>"); }
        }
    } else if (lc == "coopstop") {
        CoopStop();
    } else if (lc == "buffme") {
        std::stringstream ss(rest); double id=0,v0=100,v1=100,dur=600; ss>>id; ss>>v0; ss>>v1; ss>>dur;
        ApplyBuff((int64_t)id, v0, v1, dur);
    } else if (lc == "mbuff") {
        std::stringstream ss(rest); std::string on; ss>>on;
        double id=0,v0=0,v1=0; if(ss>>id){} if(ss>>v0){} if(ss>>v1){}
        if (id>0){ g_MBuffId=(int64_t)id; g_MBuffV0=v0; g_MBuffV1=v1; }
        bool en = (Lower(on).find("on")!=std::string::npos || on=="1");
        g_MBuff.store(en);
        Out(std::string("mbuff -> ")+(en?"ON":"OFF")+" id="+std::to_string(g_MBuffId)+" ["+std::to_string(g_MBuffV0)+","+std::to_string(g_MBuffV1)+"]");
    } else if (lc == "dsdump") {
        std::string mid, flt; mid = FirstToken(rest, flt);
        while (!flt.empty() && (flt.back()=='\r'||flt.back()=='\n'||flt.back()==' ')) flt.pop_back();
        try { DsDump(std::stod(mid), flt); } catch (...) { Out("dsdump: usage dsdump <mapId> [filter]"); }
    } else if (lc == "coopstats") {
        CoopStats();
    } else if (lc == "cooprender") {
        std::string v = Lower(rest);
        bool on = (v.find("on") != std::string::npos || v.find("1") != std::string::npos);
        g_CoopRender.store(on);
        if (!on) CoopClearPuppet();
        Out(std::string("cooprender -> ") + (on ? "ON" : "OFF") + " (puppet obj=" + g_PuppetObjName + ")");
    } else if (lc == "coopobj") {
        std::string n = rest; while (!n.empty() && (n.back()=='\r'||n.back()=='\n'||n.back()==' ')) n.pop_back();
        if (!n.empty()) { CoopClearPuppet(); g_PuppetObjName = n; g_PuppetObjIdx = -1; Out("coopobj -> " + n); }
        else Out("coopobj: need an object name (e.g. coopobj Player_obj)");
    } else if (lc == "coopclear") {
        CoopClearPuppet(); Out("coop: puppet cleared");
    } else if (lc == "comp") {
        std::string v = Lower(rest);
        bool on = (v.find("on") != std::string::npos || v.find("1") != std::string::npos);
        g_CompActive.store(on); CompSetBuffs(on);
        if (!on) CompDespawn();
        Out(std::string("companion -> ") + (on ? "ON (body=" + g_CompObjName + ", +loot/gold/reveal)" : "OFF"));
    } else if (lc == "compobj") {
        std::string n = rest; while (!n.empty() && (n.back()=='\r'||n.back()=='\n'||n.back()==' ')) n.pop_back();
        if (!n.empty()) { CompDespawn(); g_CompObjName = n; g_CompObjIdx = -1; Out("compobj -> " + n); }
        else Out("compobj: need an object name");
    } else if (lc == "puppetinput") {
        std::string v = Lower(rest);
        g_HookPuppetInput = (v.find("off") == std::string::npos) && (v.find("0") == std::string::npos);
        Out(std::string("puppetinput(IsMyPlayer=false for puppet) -> ") + (g_HookPuppetInput ? "ON" : "OFF"));
    } else if (lc == "niget") {
        std::string obj, r2; obj = FirstToken(rest, r2);
        std::string ns, var; ns = FirstToken(r2, var);
        while (!var.empty() && (var.back()=='\r'||var.back()=='\n'||var.back()==' ')) var.pop_back();
        try { NiGet(obj, std::stoi(ns), var); } catch (...) { Out("niget: usage niget <obj> <n> <var>"); }
    } else if (lc == "niset") {
        std::string obj, r2; obj = FirstToken(rest, r2);
        std::string ns, r3; ns = FirstToken(r2, r3);
        std::string var, val; var = FirstToken(r3, val);
        try { NiSet(obj, std::stoi(ns), var, std::stod(val)); } catch (...) { Out("niset: usage niset <obj> <n> <var> <val>"); }
    } else if (lc == "nicall") {
        std::string scr, r2; scr = FirstToken(rest, r2);
        std::string obj, ns; obj = FirstToken(r2, ns);
        while (!ns.empty() && (ns.back()=='\r'||ns.back()=='\n'||ns.back()==' ')) ns.pop_back();
        try { NiCall(scr, obj, std::stoi(ns)); } catch (...) { Out("nicall: usage nicall <Script> <obj> <n>"); }
    } else {
        Out("unknown command: " + cmd);
    }
}

// PollCommands moved to ForgePact::IpcServer::PollCommands (2026-09 class
// split) - it proxies every line to RunCommand() unchanged, see the header's
// own comment for why RunCommand itself stays here.

// ===== stall watchdog =======================================================
// A multi-second freeze on the character screen was reported three times and
// could not be pinned down from the logs - the guesses (a hot builtin hook, the
// runner-interface scan) were all wrong.  So the plugin names the culprit
// itself: a background thread watches for frames to stop arriving and samples
// the frame thread's instruction pointer, writing the owning module + RVA to
// out.txt.  That distinguishes "stuck in BloodPactPlugin" from "stuck in
// YYToolkit" from "stuck in the game" without a debugger.
//
// The frame thread is suspended ONLY for the GetThreadContext call and every
// allocation/format happens after it is resumed, so a lock the stalled thread
// holds (heap, CRT, loader) can never deadlock the watchdog.
static std::atomic<uint64_t> g_LastFrameTickMs{ 0 };
static HANDLE g_FrameThread = nullptr;
static std::atomic<bool> g_WatchdogRun{ false };

// Deliberately does NOT go through Out(): that also calls into the YYTK
// interface, which is not ours to touch from a second thread.
static void OutRaw(const std::string& s)
{
    std::ofstream f(OutPath(), std::ios::app);
    f << s << "\n";
}

static std::string ModuleAndRvaOf(uintptr_t addr)
{
    HMODULE mod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)addr, &mod) || !mod) {
        char b[32]; sprintf_s(b, "0x%llX", (unsigned long long)addr);
        return std::string("<unknown> ") + b;
    }
    char path[MAX_PATH] = {};
    GetModuleFileNameA(mod, path, MAX_PATH);
    std::string p(path);
    const size_t slash = p.find_last_of("\\/");
    std::string name = (slash == std::string::npos) ? p : p.substr(slash + 1);
    char b[48];
    sprintf_s(b, "+0x%llX", (unsigned long long)(addr - (uintptr_t)mod));
    return name + b;
}

// Blocked in a kernel wait tells us WHERE but not WHY.  Disk bytes and free
// memory across the stall separate "the machine is thrashing" (whole-PC freeze,
// paging) from "the frame thread is waiting on the GPU" (game-only stall).
struct StallSnapshot { unsigned long long readBytes, writeBytes, otherBytes, availMb; };
static StallSnapshot TakeStallSnapshot()
{
    StallSnapshot s{ 0, 0, 0, 0 };
    IO_COUNTERS io{};
    if (GetProcessIoCounters(GetCurrentProcess(), &io)) {
        s.readBytes = io.ReadTransferCount;
        s.writeBytes = io.WriteTransferCount;
        s.otherBytes = io.OtherTransferCount;
    }
    MEMORYSTATUSEX ms{}; ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) s.availMb = ms.ullAvailPhys / (1024ull * 1024ull);
    return s;
}

static void StallWatchdogLoop()
{
    constexpr uint64_t kStallMs = 3000;     // a frame this late is not a slow frame
    bool stalling = false;
    int samplesThisStall = 0, reports = 0;
    uint64_t worst = 0;
    StallSnapshot before{};
    while (g_WatchdogRun.load()) {
        Sleep(500);
        const uint64_t last = g_LastFrameTickMs.load();
        if (!last || !g_FrameThread) continue;
        const uint64_t behind = GetTickCount64() - last;

        if (behind < kStallMs) {
            if (stalling) {
                stalling = false;
                const StallSnapshot after = TakeStallSnapshot();
                char b[220];
                sprintf_s(b, "STALL ended - frames were %llu ms apart | disk read %llu KB, write %llu KB, other %llu KB | free RAM %llu -> %llu MB",
                          (unsigned long long)worst,
                          (after.readBytes - before.readBytes) / 1024,
                          (after.writeBytes - before.writeBytes) / 1024,
                          (after.otherBytes - before.otherBytes) / 1024,
                          before.availMb, after.availMb);
                OutRaw(b);
            }
            continue;
        }
        if (!stalling) { stalling = true; samplesThisStall = 0; worst = 0; before = TakeStallSnapshot(); }
        if (behind > worst) worst = behind;
        // A handful of samples per stall is enough to tell a spin from a wait,
        // and the session cap keeps a permanent hang from filling the disk.
        if (samplesThisStall >= 5 || reports >= 40) continue;

        CONTEXT ctx{};
        ctx.ContextFlags = CONTEXT_CONTROL;
        bool captured = false;
        if (SuspendThread(g_FrameThread) != (DWORD)-1) {
            captured = GetThreadContext(g_FrameThread, &ctx) != 0;
            ResumeThread(g_FrameThread);
        }
        if (!captured) continue;
        ++samplesThisStall; ++reports;
        OutRaw("STALL " + std::to_string(behind) + " ms - frame thread at " + ModuleAndRvaOf((uintptr_t)ctx.Rip));
    }
}

static void StartStallWatchdog()
{
    if (g_WatchdogRun.exchange(true)) return;
    try { std::thread(StallWatchdogLoop).detach(); }
    catch (...) { g_WatchdogRun.store(false); }
}

void FrameCallback(FWFrame& FrameContext)
{
    UNREFERENCED_PARAMETER(FrameContext);
    static uint32_t fc = 0;
    g_RuntimeFrame = fc;

    // Watchdog heartbeat.  One tick read + one atomic store per frame.
    g_LastFrameTickMs.store(GetTickCount64());
    if (!g_FrameThread) {
        DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                        GetCurrentProcess(), &g_FrameThread, 0, FALSE, DUPLICATE_SAME_ACCESS);
    }
#ifndef FORGEPACT_RELEASE
    PerfFrameTick();
#endif
    PERF_SCOPE(g_PerfFrame);
    FlushItemStats(fc);
    if (fc == 1) Trace("0-framecallback-running");

    // Special Content uses the game's eSt gates.  The helper is also safe in
    // all-off mode: it returns immediately while g_EstForce is empty.
    EstForceApply();
    ++g_HhFrame;
    if (g_HhEnabled.load() && (g_HhFrame % 120) == 0) HeadhunterActivityTick();
    if ((fc % kSatanicPollFrames) == 0) SatanicPollTick();
    KuyrukIsle();
#ifndef FORGEPACT_RELEASE
    CensusTick(fc);
#endif

    // one-time setup once the runner is fully alive: load config + install hook
    // Character selection still runs menu/controller code after the runner is
    // alive. Delay ForgePact setup until that transition has settled; release
    // commands are not consumed before this point.
    if (!g_Setup && fc > 300) {
        g_Setup = true;
        Trace("1-setup-start");
        try { LoadConfig(); Trace("2-loadconfig-ok"); InstallHook(); Trace("3-installhook-ok"); }
        catch (...) { Out("setup EXCEPTION"); Trace("X-setup-cppexception"); }
#ifndef FORGEPACT_RELEASE
        try { LoadCoopConfigAndMaybeStart(); Trace("4-coop-ok"); }
        catch (...) { Out("coop auto-start EXCEPTION"); Trace("X-coop-cppexception"); }
        try { SetRelicGate(true); } catch (...) {}   // relic gate ALWAYS ON (every kill drops a relic; only generates in Satanic Zones)
#endif
        Trace("5-setup-done");
    }

    // Orb pickup: the player position the globe step hooks pull toward, read
    // once per frame here instead of once per globe per step.
    if (g_OrbPickupRadius.load()) {
        RValue player;
        std::string how;
        if (HhResolveLocalPlayer(player, &how)) {
            g_OrbPlayerHow = how;
            try {
                const double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
                const double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
                if (std::isfinite(px) && std::isfinite(py)) {
                    g_PlayerX = px; g_PlayerY = py;
                    g_PlayerPosValid.store(true);
                } else { g_PlayerPosValid.store(false); }
            } catch (...) { g_PlayerPosValid.store(false); }
        } else { g_OrbPlayerHow = how; g_PlayerPosValid.store(false); }
        OrbPickupTick();
    }

    // Pet quest collector, toggled by `petquest 1`: walks the pet to the
    // nearest eligible quest item on screen and invokes the confirmed collect
    // on arrival, one item at a time (see PetQuestCollectorMod.hpp). No hook
    // is installed - everything goes through CallBuiltin - so unlike
    // relicfilter this needs no arm/defer lifecycle.
    if (ForgePact::PetQuestCollectorMod::Instance().IsEnabled()) {
        PetQuestCollectorTick();
    }

#ifndef FORGEPACT_RELEASE
    // B4 Phase 0 research: `citrace pokekey`'s pending one-frame restore must
    // land regardless of whether `citrace <on|off>` tracing itself is armed -
    // see docs/pet-quest-collector-plan-b4-input-simulation.md §3a.
    CiPokeKeyTick();
#endif

    // Relic filter, armed by `relicfilter 1`: the DropRelic hook goes in only
    // once the runner has settled AND a real player exists.  Installing it during
    // character selection stalled the game for about a minute, which is why the
    // panel used to withhold the command entirely and the mod never applied
    // after a restart (user report 2026-09-09).  Checked once a second at most.
    if (ForgePact::RelicFilterMod::Instance().IsPending() && g_Setup && (fc % 60) == 0) {
        RValue player;
        if (HhResolveLocalPlayer(player)) {
            ForgePact::RelicFilterMod::Instance().ClearPending();
            HookOneScript("DropRelic", "bp_drelic", (PVOID)Hook_DropRelic, &g_Orig_DropRelic);
            Out(std::string("relicfilter: hook installed -> ") + (g_Orig_DropRelic ? "ON" : "FAILED (DropRelic not found)"));
        }
    }

#ifndef FORGEPACT_RELEASE
    // Gelistirici kisayollari.  Yayin derlemesinde YOK: F6 oyuncunun
    // dibine Damien boss'u cagiriyor, F10 isinlanma portali aciyor,
    // F11 relic dusuruyor, F7/F8/F9 density'yi panelden bagimsiz
    // degistirip arayuzle celisiyordu.  F5 (minimap) asagida kalir.
    // ===== Hotkeys for one-button control =====
    static bool f8p = false, f9p = false, f7p = false, f6p = false;
    bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    bool f6 = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    if (f6 && !f6p) {
        SpawnByName("Damien_obj"); // summon Damien boss at player
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Summoned Damien at player");
    }
    f6p = f6;
    if (f8 && !f8p) {
        ForgePact::DensityManager::Instance().Mult = (ForgePact::DensityManager::Instance().Mult > 1.0) ? 1.0 : 3.0;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%g", ForgePact::DensityManager::Instance().Mult);
    }
    if (f9 && !f9p) {
        if (ForgePact::DensityManager::Instance().Mult < 20.0) ForgePact::DensityManager::Instance().Mult += 0.5;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%g", ForgePact::DensityManager::Instance().Mult);
    }
    if (f7 && !f7p) {
        if (ForgePact::DensityManager::Instance().Mult > 1.0) ForgePact::DensityManager::Instance().Mult -= 0.5;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%g", ForgePact::DensityManager::Instance().Mult);
    }
    f8p = f8; f9p = f9; f7p = f7;

    // ===== F10: spawn Sheeponia teleport portal on player (instant teleport) =====
    static bool f10p = false;
    bool f10 = (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    if (f10 && !f10p) {
        SpawnByName("Portal_Sheeponia_obj"); // portal spawns on player; its own Step_0 teleports to Sheeponia
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Sheeponia portal spawned on you (F10)");
    }
    f10p = f10;

    // ===== F11: force a relic drop at the player (works anywhere, not just Satanic Zones) =====
    static bool f11p = false;
    bool f11 = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    if (f11 && !f11p) {
        ForceRelicDrop(1);
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Forced relic drop at player (F11)");
    }
    f11p = f11;

#endif
    // auto map reveal (throttled internally to every ~20 frames)
    ForgePact::MapRevealManager::Instance().OnFrame(g_RuntimeFrame);

#ifndef FORGEPACT_RELEASE
    static bool f5p = false;
    bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5 && !f5p) {
        ForgePact::MapRevealManager::Instance().Toggle();
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Auto map-reveal: %s", ForgePact::MapRevealManager::Instance().IsEnabled() ? "ON" : "OFF");
    }
    f5p = f5;
#endif

// Research-only co-op/companion/buff features are not accepted by the player
// command whitelist, so their per-frame branches do not belong in ship builds.
#ifndef FORGEPACT_RELEASE
    // P2P receive poll: every frame while enabled, drain incoming steam_net packets.
    if (g_P2PPoll) {
        try { P2PReceiveTick(); } catch (...) {}
    }

    // Custom co-op: send local player state to peer every frame.
    if (g_CoopEnabled.load()) {
        try { CoopTick(); } catch (...) {}
    }
    // Custom co-op: render the remote player's puppet at received coords.
    if (g_CoopRender.load()) {
        try { CoopRenderTick(); } catch (...) {}
    }
    // Companion: keep the follower at the player's side.
    if (g_CompActive.load()) {
        try { CompTick(); } catch (...) {}
    }
    // Continuous buff (re-apply every frame to stay active).
    if (g_MBuff.load()) {
        try { MBuffTick(); } catch (...) {}
    }
#endif

    // Keep fc advancing before setup, but do not consume queued player commands
    // until the one-shot hook installation attempt has completed.
    // Two IPC checks per second are enough for a settings panel and avoid five
    // filesystem probes per second during gameplay.
    if (((fc++) % 30) == 0 && g_Setup) {
        if ((g_RuntimeFrame % 6) == 0) { PERF_SCOPE(g_PerfPoll); try { ForgePact::IpcServer::Instance().PollCommands(); } catch (...) {} }
    }
}

// ===== Single-instance bypass: clear ERROR_ALREADY_EXISTS on mutex/event creation so a
// 2nd game copy doesn't detect the 1st and self-exit. Installed as early as possible. =====
#ifndef FORGEPACT_RELEASE
typedef HANDLE(WINAPI* PFN_CreateMutexW)(LPSECURITY_ATTRIBUTES, BOOL, LPCWSTR);
typedef HANDLE(WINAPI* PFN_CreateMutexA)(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR);
typedef HANDLE(WINAPI* PFN_CreateEventW)(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCWSTR);
typedef HANDLE(WINAPI* PFN_CreateEventA)(LPSECURITY_ATTRIBUTES, BOOL, BOOL, LPCSTR);
static PFN_CreateMutexW g_oCMW = nullptr;
static PFN_CreateMutexA g_oCMA = nullptr;
static PFN_CreateEventW g_oCEW = nullptr;
static PFN_CreateEventA g_oCEA = nullptr;
// Diagnostic: log every NAMED object that already existed (candidate single-instance lock).
static void SiLog(const char* kind, const wchar_t* wn, const char* an)
{
    std::string name;
    if (wn) { std::wstring w(wn); name.assign(w.begin(), w.end()); }
    else if (an) name = an;
    else return;
    std::ofstream f(IPC_DIR + "\\silog.txt", std::ios::app);
    f << kind << " ALREADY: " << name << "\n";
}
// Only bypass NAMED objects whose name does NOT contain "Mutex" (GM-internal mutexes keep real behavior).
static bool SiNameW(LPCWSTR n) { if (!n) return false; std::wstring s(n); return s.find(L"Mutex") == std::wstring::npos; }
static bool SiNameA(LPCSTR n) { if (!n) return false; std::string s(n); return s.find("Mutex") == std::string::npos; }
static HANDLE WINAPI hkCMW(LPSECURITY_ATTRIBUTES a, BOOL b, LPCWSTR n) { HANDLE h = g_oCMW(a, b, n); if (GetLastError() == ERROR_ALREADY_EXISTS) { SiLog("MtxW", n, nullptr); if (SiNameW(n)) SetLastError(ERROR_SUCCESS); } return h; }
static HANDLE WINAPI hkCMA(LPSECURITY_ATTRIBUTES a, BOOL b, LPCSTR n) { HANDLE h = g_oCMA(a, b, n); if (GetLastError() == ERROR_ALREADY_EXISTS) { SiLog("MtxA", nullptr, n); if (SiNameA(n)) SetLastError(ERROR_SUCCESS); } return h; }
static HANDLE WINAPI hkCEW(LPSECURITY_ATTRIBUTES a, BOOL m, BOOL s, LPCWSTR n) { HANDLE h = g_oCEW(a, m, s, n); if (GetLastError() == ERROR_ALREADY_EXISTS) { SiLog("EvtW", n, nullptr); if (SiNameW(n)) SetLastError(ERROR_SUCCESS); } return h; }
static HANDLE WINAPI hkCEA(LPSECURITY_ATTRIBUTES a, BOOL m, BOOL s, LPCSTR n) { HANDLE h = g_oCEA(a, m, s, n); if (GetLastError() == ERROR_ALREADY_EXISTS) { SiLog("EvtA", nullptr, n); if (SiNameA(n)) SetLastError(ERROR_SUCCESS); } return h; }
static void InstallSingleInstanceBypass()
{
    HMODULE k = GetModuleHandleW(L"kernelbase.dll"); if (!k) k = GetModuleHandleW(L"kernel32.dll");
    if (!k) return;
    struct { const char* name; PVOID hook; PVOID* orig; const char* id; } tbl[] = {
        { "CreateMutexW", (PVOID)hkCMW, (PVOID*)&g_oCMW, "si_cmw" },
        { "CreateMutexA", (PVOID)hkCMA, (PVOID*)&g_oCMA, "si_cma" },
        { "CreateEventW", (PVOID)hkCEW, (PVOID*)&g_oCEW, "si_cew" },
        { "CreateEventA", (PVOID)hkCEA, (PVOID*)&g_oCEA, "si_cea" },
    };
    for (auto& e : tbl) {
        void* p = (void*)GetProcAddress(k, e.name);
        if (!p) continue;
        PVOID tr = nullptr;
        if (AurieSuccess(MmCreateHook(g_ArSelfModule, e.id, p, e.hook, &tr))) *e.orig = tr;
    }
}
#endif

EXPORTED AurieStatus ModuleInitialize(
    IN AurieModule* Module,
    IN const fs::path& ModulePath)
{
    UNREFERENCED_PARAMETER(ModulePath);

    // InstallSingleInstanceBypass();  // DISABLED: the blocker was Goldberg's port, not a mutex; this broke Goldberg init

    g_Yytk = YYTK::GetInterface();
    if (!g_Yytk) {
        Aurie::DbgPrint("[BloodPact] ERROR: Failed to get YYToolkit interface!\n");
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }

    // Startup bookkeeping moved to ForgePact::ModManager::Initialize (2026-09
    // class split) - see the header's own comment for why ModuleInitialize
    // itself stays here.
    ForgePact::ModManager::Instance().Initialize();
    LoadStartup();   // oyun kodu calismadan once uygulanmasi gereken ayarlar
#ifdef FORGEPACT_RELEASE
    KonsoluGizle();
#endif

    AurieStatus st = g_Yytk->CreateCallback(Module, EVENT_FRAME, (PVOID)FrameCallback, 0);
    InstallHeadLabelHook();
    if (!AurieSuccess(st)) {
        Out("FAILED to register frame callback st=" + std::to_string((int)st));
        g_Yytk->PrintError(__FILE__, __LINE__, "[BloodPact] Failed to register frame callback (st=%d)", (int)st);
    } else {
        g_Yytk->PrintInfo("[BloodPact] BloodPact plugin successfully loaded into YYToolkit.");
        g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] ready - watching bp_ipc\\cmd.txt");
        StartStallWatchdog();
    }

    Aurie::DbgPrint("[BloodPact] BloodPact plugin initialized successfully.\n");
    return AURIE_SUCCESS;
}
