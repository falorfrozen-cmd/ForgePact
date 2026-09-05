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
static double g_CreatorMult = 1.0;          // ALL Enemy_Creator* spawners (density).  Kesirli olabilir: 1.5, 2.5 ...
static double g_CreatorFrac = 0.0;          // kesir birikimi - 1.5x'te her ikinci ureticiye bir fazla kopya
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
    if (g_CreatorMult <= 1.0 || g_RuntimeFrame > g_DensityWindowHardEnd)
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
    g_CreatorFrac = 0.0;
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
    const bool inspectCreator = g_CreatorMult > 1.0
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
    if (g_CreatorMult > 1.0 && isCreator && !specialChild
        && !densityAlreadyApplied
        && (knownCreator || DensityWindowActive())) {
        // Tam kisim herkese; kesir kismi birikime yayilir ve 1'e ulasinca
        // O ureticiye bir fazla kopya dusar.  Rastgelelik yok, deterministik.
        int tam = (int)g_CreatorMult;
        double kesir = g_CreatorMult - (double)tam;
        int m = tam;
        if (kesir > 0.0) {
            g_CreatorFrac += kesir;
            if (g_CreatorFrac >= 1.0) { g_CreatorFrac -= 1.0; m += 1; }
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
static void HookICD(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
#ifdef FORGEPACT_RELEASE
    // A hook installed earlier in this process cannot be removed safely while
    // the game is running.  With every related feature Off, take a true native
    // pass-through path: no object lookup, cache access or post-create work.
    if (g_CreatorMult <= 1.0 && g_EnemyMultAll <= 1 && g_ObjMult.empty()) {
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
    const bool densityCreate = g_CreatorMult > 1.0
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
#ifdef FORGEPACT_RELEASE
    if (g_CreatorMult <= 1.0 && g_EnemyMultAll <= 1 && g_ObjMult.empty()) {
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
    const bool densityCreate = g_CreatorMult > 1.0
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

static bool HookOneScript(const char* shortName, const char* id, PVOID dest, PFUNC_YYGMLScript* origOut)
{
    std::string full = std::string("gml_Script_") + shortName;
    PVOID p = nullptr;
    AurieStatus st = g_Yytk->GetNamedRoutinePointer(full.c_str(), &p);
    if (!AurieSuccess(st) || !p) { Out(std::string("hook ") + shortName + ": not found st=" + std::to_string((int)st)); return false; }
    CScript* sc = reinterpret_cast<CScript*>(p);
    PVOID src = nullptr;
    try { src = (PVOID)sc->m_Functions->m_ScriptFunction; } catch (...) {}
    if (!src) { Out(std::string("hook ") + shortName + ": null src"); return false; }
    PVOID tramp = nullptr;
    AurieStatus hs = MmCreateHook(g_ArSelfModule, id, src, dest, &tramp);
    if (!AurieSuccess(hs)) { Out(std::string("hook ") + shortName + ": MmCreateHook failed st=" + std::to_string((int)hs)); return false; }
    *origOut = reinterpret_cast<PFUNC_YYGMLScript>(tramp);
    Out(std::string("HOOK INSTALLED on ") + shortName);
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
#ifdef FORGEPACT_RELEASE
  #define BP_LOGDROP(a,b,c,d) ((void)0)
#else
  #define BP_LOGDROP(a,b,c,d) LogDrop(a,b,c,d)
#endif

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
        const std::string path = IPC_DIR + "\\itemstats.json", tmp = path + ".tmp";
        g_ItemStatsWriting = true;
        std::thread([path, tmp, body = std::move(body)]() {
            try {
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
static void RememberForgedItem(const RValue& item)
{
    for (auto& e : g_ForgedItems) if (e.m_Object == item.m_Object) { e = item; return; }
    if (g_ForgedItems.size() >= 64) g_ForgedItems.erase(g_ForgedItems.begin());
    g_ForgedItems.push_back(item);
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

static void LoadCustomForgeEntries()
{
    g_CustomForgeEntries.clear();
    std::ifstream f(CustomForgeRuntimePath(), std::ios::binary);
    if (!f) {
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

static bool TryApplyCustomForge(RValue* candidate)
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
            if (entry.rarity >= 0 || !entry.lore.empty() || !entry.name.empty()) {
                RValue hasInfo = g_Yytk->CallBuiltin(
                    "variable_struct_exists", { *candidate, RValue("itemInfoStruct") });
                RValue info = hasInfo.ToBoolean()
                    ? g_Yytk->CallBuiltin("variable_struct_get", { *candidate, RValue("itemInfoStruct") })
                    : RValue();
                if (info.m_Kind == VALUE_OBJECT) {
                    if (entry.rarity >= 0)
                        g_Yytk->CallBuiltin("variable_struct_set", { info, RValue("27"), RValue((double)entry.rarity) });
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

static void CustomForgePostProcess(RValue& result, int argc, RValue** args)
{
    if (TryApplyCustomForge(&result)) return;
    // Some constructors mutate an argument and return undefined. Scan only a
    // small, bounded prefix; TryApply requires all three canonical item fields.
    for (int i = 0; i < argc && i < 8; ++i)
        if (args && TryApplyCustomForge(args[i])) return;
}


// ===== Headhunter mechanic ==================================================
//
// Path-of-Exile style belt: when the player kills a rare/champion monster the
// monster's affixes are translated into timed player buffs.  Live-verified
// facts (2026-09-04): EnemyDestroyKillProc runs with self = killing Player_obj
// and argument 2 = the dying enemy instance; the enemy carries `affixList`
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

static bool HhEquipped(CInstance* player)
{
    (void)player;
    if (g_HhForced.load()) return true;
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
static bool HhResolveLocalPlayer(RValue& out, std::string* how = nullptr)
{
    try {
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        RValue p;
        AurieStatus st = g_Yytk->CallGameScriptEx(p, "gml_Script_GetMyPlayer", g, g, {});
        if (AurieSuccess(st) && (p.m_Kind == VALUE_REAL || p.m_Kind == VALUE_INT32 || p.m_Kind == VALUE_INT64 || p.m_Kind == VALUE_REF || p.m_Kind == VALUE_OBJECT)) {
            RValue ex = g_Yytk->CallBuiltin("instance_exists", { p });
            if (ex.ToBoolean()) { out = p; if (how) *how = "GetMyPlayer"; return true; }
        }
    } catch (...) {}
    try {
        RValue obj = g_Yytk->CallBuiltin("asset_get_index", { RValue("Player_obj") });
        if (obj.ToDouble() >= 0) {
            RValue inst = g_Yytk->CallBuiltin("instance_find", { obj, RValue(0.0) });
            RValue ex = g_Yytk->CallBuiltin("instance_exists", { inst });
            if (ex.ToBoolean()) { out = inst; if (how) *how = "instance_find(Player_obj)"; return true; }
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
static bool g_TyHookInstalled = false, g_TyHookAttempted = false;
static PFUNC_YYGMLScript g_Orig_EnemyRaritySettings = nullptr;
// enemyAffix indices whose meaning is live-confirmed (see kHhAffixNames); 0 = champion marker.
static const int kTyAffixPool[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 25, 30, 31, 36, 38 };
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
static bool TyrantActive()
{
    if (!g_TyEnabled.load()) return false;
    if (g_TyForced.load()) return true;
#ifndef FORGEPACT_RELEASE
    return TyrantItemLoaded();   // research build: a loaded crown counts as worn (equip detection pending)
#else
    return false;
#endif
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
    ++g_TySeen;
    RValue inst; try { if (S) inst = S->ToRValue(); } catch (...) { S = nullptr; }
#ifndef FORGEPACT_RELEASE
    bool trace = g_RarTraceLeft > 0;
    if (trace) { --g_RarTraceLeft; Out("rarity #" + std::to_string(g_TySeen) + " ENTRY self=" + TyInstName(inst) + " argc=" + std::to_string(argc) + TyArgs(argc, A) + RarState(inst)); }
    if (g_RarForceLeft > 0 && S) { --g_RarForceLeft; try { g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("forceRarity"), RValue(g_RarForceVal) }); Out("   -> forceRarity set to " + std::to_string((int)g_RarForceVal)); } catch (...) {} }
    if (g_RarPreLeft > 0 && S) { --g_RarPreLeft; try { g_Yytk->CallBuiltin("variable_instance_set", { inst, RValue("enemyRarity"), RValue(g_RarPreVal) }); Out("   -> enemyRarity pre-set to " + std::to_string((int)g_RarPreVal)); } catch (...) {} }
#endif
    if (S && TyrantActive()) {
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
static void InstallTyrantHook()
{
    if (g_TyHookAttempted) return;
    g_TyHookAttempted = true;
    g_TyHookInstalled = HookOneScript("EnemyRaritySettings", "fp_tyrant_rarity", (PVOID)Hook_EnemyRaritySettings, &g_Orig_EnemyRaritySettings);
}
static void TyrantAutoArm()
{
    bool wanted = g_TyForced.load();
#ifndef FORGEPACT_RELEASE
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (e.mechanic == "tyrant") { wanted = true; break; }
#else
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (e.mechanic == "tyrant") { Out("tyrant: item found; mechanic is a preview in this build (use 'tyrant force' to try it)"); break; }
#endif
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
        + " itemLoaded=" + (TyrantItemLoaded() ? "yes" : "no"));
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
    HookOneScript("PathFindTakeTarget",     "bp_tr_take",  (PVOID)Hook_TracePathFindTakeTarget,     &g_OrigAggro_PathFindTakeTarget);
    HookOneScript("SocketSetTarget",        "bp_tr_sst",   (PVOID)Hook_TraceSocketSetTarget,        &g_OrigAggro_SocketSetTarget);
    HookOneScript("PathFindAggroBroadcast", "bp_tr_bc",    (PVOID)Hook_TracePathFindAggroBroadcast, &g_OrigAggro_PathFindAggroBroadcast);
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
#ifndef FORGEPACT_RELEASE
    return g_BeItemTagged.load();   // research build: a loaded amulet counts as worn
#else
    return false;
#endif
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
static TRoutine g_OrigDistanceToObject = nullptr;
static void Hook_distance_to_object(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    if (g_OrigDistanceToObject) g_OrigDistanceToObject(Result, S, O, argc, Args);
    if (!g_BeSpawnNear || !S || !BeaconActive()) return;
    try {
        RValue inst = S->ToRValue();
        RValue oi = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("object_index") });
        if (!IsCreatorObject((int)oi.ToDouble())) return;
        if (g_BeWakeRadius > 0.0) {
            RValue player; if (!HhResolveLocalPlayer(player)) return;
            const double px = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("x") }).ToDouble();
            const double py = g_Yytk->CallBuiltin("variable_instance_get", { player, RValue("y") }).ToDouble();
            const double cx = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("x") }).ToDouble();
            const double cy = g_Yytk->CallBuiltin("variable_instance_get", { inst, RValue("y") }).ToDouble();
            const double dx = cx - px, dy = cy - py;
            if (dx * dx + dy * dy > g_BeWakeRadius * g_BeWakeRadius) return;
        }
        Result = RValue(0.0);
        ++g_BeSpawnLies;
    } catch (...) {}
}
static void InstallBeaconHook()
{
    if (g_BeHookAttempted) return;
    g_BeHookAttempted = true;
    HookBuiltin("distance_to_object", "fp_beacon_dist", (PVOID)Hook_distance_to_object, &g_OrigDistanceToObject);
    bool a = HookOneScript("PathFindScanTick",   "fp_beacon_scan",  (PVOID)Hook_PathFindScanTick,   &g_Orig_PathFindScanTick);
    bool b = HookOneScript("PathFindLeashCheck", "fp_beacon_leash", (PVOID)Hook_PathFindLeashCheck, &g_Orig_PathFindLeashCheck);
    HookOneScript("ActivateDeactivateProps",      "fp_beacon_wake",  (PVOID)Hook_ActivateDeactivateProps,      &g_Orig_ActivateDeactivateProps);
    HookOneScript("LocalActivateDeactivateProps", "fp_beacon_wakel", (PVOID)Hook_LocalActivateDeactivateProps, &g_Orig_LocalActivateDeactivateProps);
    g_BeHookInstalled = a && b;
}
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
#ifndef FORGEPACT_RELEASE
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (e.mechanic == "beacon") { wanted = true; break; }
#else
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (e.mechanic == "beacon") { Out("beacon: item found; mechanic is a preview in this build (use 'beacon force' to try it)"); break; }
#endif
    if (!wanted) return;
    InstallBeaconHook();
    g_BeEnabled.store(g_BeHookInstalled);
    Out(std::string("beacon: ") + (g_BeHookInstalled ? "armed" : "hook failed") + " (range " + std::to_string((long long)g_BeRange) + ", mode " + (g_BeRareOnly ? "rare" : "all") + ")");
}

static void HhApplyBuff(CInstance* player, const HhBuff& b, double frames)
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
    } catch (...) { Out("hh: BuffAdd EXCEPTION"); }
}

// Core: translate one dying enemy's affixes into buffs on `player`.
static void HhOnKill(CInstance* player, const RValue& enemy)
{
    InterlockedIncrement(&g_HhKills);
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
        if (rarity < 2.0) return;

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

        // A kill counts when the game flags it rare (rarity >= 2) or the string affix list is filled.
        if (rarity < 2.0 && keys.empty()) return;
        InterlockedIncrement(&g_HhRareKills);
        if (g_HhTrace) {
            std::string ks; for (const auto& k : keys) ks += (ks.empty() ? "" : ",") + k;
            Out("hh: rare kill rarity=" + std::to_string((int)rarity) + " affixes=[" + ks + "]");
        }
        if (!HhEquipped(player)) { InterlockedIncrement(&g_HhSkippedNotEquipped); if (g_HhTrace) Out("hh: belt not equipped, skipped"); return; }
        double spd = 60.0;
        try { spd = g_Yytk->CallBuiltin("game_get_speed", { RValue(0.0) }).ToDouble(); if (spd < 1.0) spd = 60.0; } catch (...) {}
        double frames = g_HhDurationSec * spd;
        int applied = 0;
        for (const std::string& key : keys) {
            auto it = g_HhMap.find(key);
            if (it == g_HhMap.end() && key.rfind("affix", 0) == 0) it = g_HhMap.find(key.substr(5));
            if (it != g_HhMap.end()) { HhApplyBuff(player, it->second, frames); ++applied; HhRememberStolen(player, it->first, g_HhDurationSec, it->second.id); }
            else if (g_HhTrace) Out("hh: no mapping for affix '" + key + "'");
        }
        if (applied == 0 && g_HhDefaultOn) { HhApplyBuff(player, g_HhDefault, frames); HhRememberStolen(player, "Rare Essence", g_HhDurationSec, g_HhDefault.id); }
    } catch (...) { Out("hh: EXCEPTION in HhOnKill"); }
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
    HookOneScript("RunItemEquipped", "fp_tr_rie", (PVOID)Hook_TraceRunItemEquipped, &g_OrigTrace_RunItemEquipped);
    HookOneScript("ItemEquip", "fp_tr_ie", (PVOID)Hook_TraceItemEquip, &g_OrigTrace_ItemEquip);
    HookOneScript("EquipItemUnequip", "fp_tr_eiu", (PVOID)Hook_TraceEquipItemUnequip, &g_OrigTrace_EquipItemUnequip);
}
#endif

// Live-verified 2026-09-04 (research probe, 5 real kills): EnemyDestroyKillProc runs with
// self = the DYING ENEMY and argument 2 = the killing Player_obj (the earlier reading had
// the roles swapped, which is why no kill ever showed enemy data).
static RValue& Hook_EnemyDestroyKillProc(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    RValue& res = g_Orig_EnemyDestroyKillProc ? g_Orig_EnemyDestroyKillProc(S, O, R, argc, A) : R;
    if (g_HhEnabled.load() && S && argc >= 3 && A && A[2]) {
        try {
            CInstance* player = nullptr;
            try {
                RValue pid = g_Yytk->CallBuiltin("variable_instance_get", { *A[2], RValue("id") });
                if (pid.m_Kind == VALUE_REAL || pid.m_Kind == VALUE_INT32 || pid.m_Kind == VALUE_INT64)
                    g_Yytk->GetInstanceObject((int32_t)pid.ToDouble(), player);
            } catch (...) { player = nullptr; }
            if (!player) player = O;
            RValue enemy = S->ToRValue();
            if (player) HhOnKill(player, enemy);
        } catch (...) {}
    }
    return res;
}

static void InstallHeadhunterHook()
{
    if (g_HhHookInstalled) return;
    if (HookOneScript("EnemyDestroyKillProc", "fp_headhunter_kill", (PVOID)Hook_EnemyDestroyKillProc, &g_Orig_EnemyDestroyKillProc))
        g_HhHookInstalled = true;
}

// Called after the sidecar is loaded: arm the mechanic only when some forged
// item asks for it (or a command forced it), so ordinary players pay nothing.
static void HeadhunterAutoArm()
{
    bool wanted = g_HhForced.load();
#ifndef FORGEPACT_RELEASE
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (e.mechanic == "headhunter") { wanted = true; break; }
#else
    // Player builds (1.3.7): the Headhunter groundwork ships dormant.  Forged
    // items keep their fp_mechanic tag; the kill hook is only installed by an
    // explicit "headhunter force" command until belt detection and the affix
    // -> buff table are complete.
    for (const CustomForgeEntry& e : g_CustomForgeEntries) if (e.mechanic == "headhunter") { Out("headhunter: item found; mechanic is a preview in this build (use 'headhunter force' to try it)"); break; }
#endif
    if (!wanted) return;
    InstallHeadhunterHook();
    g_HhEnabled.store(g_HhHookInstalled);
    Out(std::string("headhunter: ") + (g_HhHookInstalled ? "armed" : "hook failed") + " (" + std::to_string(g_HhDurationSec) + " s, " + std::to_string(g_HhMap.size()) + " mapped affixes)");
}

static void HeadhunterStatus()
{
    std::string m;
    for (const auto& kv : g_HhMap) m += kv.first + "->" + std::to_string((long long)kv.second.id) + " ";
    Out(std::string("headhunter: ") + (g_HhEnabled.load() ? "ON" : "off") + (g_HhForced.load() ? " (forced)" : "")
        + " hook=" + (g_HhHookInstalled ? "yes" : "no") + " dur=" + std::to_string(g_HhDurationSec) + "s"
        + " kills=" + std::to_string(g_HhKills) + " rare=" + std::to_string(g_HhRareKills)
        + " rarityFlag=" + std::to_string(g_HhRarityKills) + " withAffixData=" + std::to_string(g_HhAffixKills)
        + " buffs=" + std::to_string(g_HhBuffsApplied) + " skippedNoBelt=" + std::to_string(g_HhSkippedNotEquipped)
        + " default=" + (g_HhDefaultOn ? std::to_string((long long)g_HhDefault.id) : std::string("off"))
        + " map=[" + m + "]");
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
        CustomForgePostProcess(_res, argc, A); \
        BP_LOGDROP(#NAME, _res, argc, A); \
        return _res; \
    }

// ===== Oyuncu istatistigi carpanlari =====================================
// Blood Pact'in Magic Find / Attack Speed / Cast Rate / Experience gain /
// Movement Speed satirlari bir DEPODA durmuyor.  Oyuncunun
// uzerinde boyle degiskenler yok (olculdu: iget magic_find -> "boyle bir
// degisken yok"), pSt dizisi de deger degil tutamac tutuyor.
//
// Her stat icin bir Stat<Ad> betigi var ve oyun degeri her ihtiyac duydugunda
// oradan okuyor.  O yuzden depoyu aramak yerine DONEN DEGERI carpiyoruz -
// tek nokta, tum kullanicilari birden etkiliyor.
//
// Carpan 1.0 iken kanca hicbir sey yapmaz; vanilya davranis birebir korunur.

// Carpan her kanca icin AYRI bir double.  Onceki surum std::map'te metin
// anahtariyla ariyordu; bu her cagrida gecici bir std::string kurup bellek
// ayiriyordu.  Bu fonksiyonlar oyunun en sicak yollarinda (olculdu: tek
// oturumda StatMovementSpeed 6083, StatTotalDamage 2350 cagri), oraya
// tahsis koymak dogru degil.
// Stat* betikleri DIZI donduruyor - olculdu 2026-08-28:
//     StatMovementSpeed -> [305.108..., 0, 0, 600]
// ve ilk eleman karakter ekranindaki degerin ta kendisi (305).  Onceki surum
// dizinin ToDouble()'ini carpiyordu; bu anlamsiz bir sayi uretiyordu.  Magic
// Find'da hicbir etki gorulmemesinin ve hareket hizinda oyunun kasmasinin
// sebebi buydu.
//
// Diziyi YERINDE degistirmiyoruz: oyun ayni diziyi her cagrida yeniden
// kullaniyor olabilir, o zaman carpan her karede birikip patlardi.  Kopya
// uretip yalnizca [0]'i olcekliyoruz.
static RValue StatOlcekle(RValue& deger, double carpan)
{
    if (deger.m_Kind != VALUE_ARRAY)
        return RValue(deger.ToDouble() * carpan);
    int n = (int)g_Yytk->CallBuiltin("array_length", { deger }).ToDouble();
    if (n <= 0) return deger;
    RValue kopya = g_Yytk->CallBuiltin("array_create", { RValue((double)n) });
    for (int i = 0; i < n; i++) {
        RValue e = g_Yytk->CallBuiltin("array_get", { deger, RValue((double)i) });
        if (i == 0) e = RValue(e.ToDouble() * carpan);
        g_Yytk->CallBuiltin("array_set", { kopya, RValue((double)i), e });
    }
    return kopya;
}

// Faster Cast Rate'in vanilya tabani 0'dır.  Sifiri carpmak her zaman sifir
// verdigi icin bu statta yuzde puani EKLEMEK gerekir (+50 -> FCR 0'dan 50'ye).
// Diger statlar mevcut toplam uzerinden oransal olarak carpilir.
static RValue StatEkle(RValue& deger, double ek)
{
    if (deger.m_Kind != VALUE_ARRAY)
        return RValue(deger.ToDouble() + ek);
    int n = (int)g_Yytk->CallBuiltin("array_length", { deger }).ToDouble();
    if (n <= 0) return deger;
    RValue kopya = g_Yytk->CallBuiltin("array_create", { RValue((double)n) });
    for (int i = 0; i < n; i++) {
        RValue e = g_Yytk->CallBuiltin("array_get", { deger, RValue((double)i) });
        if (i == 0) e = RValue(e.ToDouble() + ek);
        g_Yytk->CallBuiltin("array_set", { kopya, RValue((double)i), e });
    }
    return kopya;
}

#define STAT_HOOK(NAME) \
    static PFUNC_YYGMLScript g_OrigStat_##NAME = nullptr; \
    static volatile long g_StatSayac_##NAME = 0; \
    static double g_StatCarpan_##NAME = 1.0; \
    static RValue& HookStat_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        BP_DIAG_INCREMENT(g_StatSayac_##NAME); \
        RValue& _r = g_OrigStat_##NAME ? g_OrigStat_##NAME(S, O, R, argc, A) : R; \
        if (g_StatCarpan_##NAME != 1.0) { \
            try { _r = StatOlcekle(_r, g_StatCarpan_##NAME); } catch (...) {} \
        } \
        return _r; \
    }

#define STAT_ADD_HOOK(NAME) \
    static PFUNC_YYGMLScript g_OrigStatAdd_##NAME = nullptr; \
    static volatile long g_StatAddSayac_##NAME = 0; \
    static double g_StatEk_##NAME = 0.0; \
    static RValue& HookStatAdd_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        BP_DIAG_INCREMENT(g_StatAddSayac_##NAME); \
        RValue& _r = g_OrigStatAdd_##NAME ? g_OrigStatAdd_##NAME(S, O, R, argc, A) : R; \
        if (g_StatEk_##NAME != 0.0) { \
            try { _r = StatEkle(_r, g_StatEk_##NAME); } catch (...) {} \
        } \
        return _r; \
    }

STAT_HOOK(StatMagicFind)
// The aggregate StatAttackSpeed result is the value consumed by the live
// attack-timing path.  MainHand/OffHand are detail helpers used by the stat
// query/UI path and can remain completely idle after the character is loaded.
// Hooking the aggregate script also matches the previously live-verified
// ForgePact implementation.
STAT_HOOK(StatAttackSpeed)
STAT_ADD_HOOK(StatFasterCastRate)
STAT_HOOK(StatExperienceGain)
STAT_HOOK(StatMovementSpeed)
STAT_HOOK(StatExtraGold)
STAT_HOOK(StatLifeReplenish)
STAT_HOOK(StatManaReplenish)
STAT_HOOK(StatDefense)
STAT_HOOK(StatCritDamage)
STAT_HOOK(StatCritRate)
STAT_HOOK(StatSpellCritDamage)
STAT_HOOK(StatSpellCritRate)
// Gercek son vurus hasari.  StatTotalDamage yalnizca karakter istatistigi
// hesap/arayuz yoludur; onu carpmak dusmana giden hasari degistirmedi.
// Canli olcumde CalculateEndDamage her vurus icin 5645/5807/6007 gibi nihai
// sayiyi dondurdu ve ayni sayi hemen RunDamageSync'e girdi.  Bu nedenle hasar
// carpani tam burada, oyunun kendi hesaplamasi bittikten sonra uygulanir.
STAT_HOOK(CalculateEndDamage)
// XP carpani.  Hedef EnemyCalculateExperience'in DONUS degeri.
// HSStatForge burayi degil, fonksiyonun icindeki bir `1.0` SABITINI
// yamaliyordu (xmm11 basta .rdata'dan yukleniyor ve hic degismiyor),
// o yuzden orada carpan hicbir sey yapmiyor.  Donus degeri ise tanimi
// geregi hesaplanan deneyim - carpilacak dogru yer burasi.
STAT_HOOK(EnemyCalculateExperience)

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

// Yaratigin ustunde beliren "2 XP" baloncugu.
//
// Olculdu: oyuncuya verilen deger dogru carpiliyor
// (EnemyGiveExperience 200, ExperienceUpdate 200) ama baloncuk sayiyi degil
// ZATEN BICIMLENMIS bir metin aliyor - CombatText("2 XP", ...) - ve o metin
// carpandan once uretiliyor.  Sonuc: oyuncu 200 aliyor, ekranda 2 yaziyor.
//
// Burada yalnizca METNI yeniden yaziyoruz.  Verilen XP'ye dokunulmuyor;
// bu tamamen gorsel bir duzeltme.
static PFUNC_YYGMLScript g_OrigCombatText = nullptr;
static RValue& Hook_CombatText(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A)
{
    RValue yeni;
    std::vector<RValue*> A2;
    double c = g_StatCarpan_EnemyCalculateExperience;
    if (c != 1.0 && A && argc > 0 && A[0] && A[0]->m_Kind == VALUE_STRING) {
        try {
            std::string s = A[0]->ToString();
            static const std::string sonek = " XP";
            if (s.size() > sonek.size() &&
                s.compare(s.size() - sonek.size(), sonek.size(), sonek) == 0) {
                std::string sayi = s.substr(0, s.size() - sonek.size());
                size_t kac = 0;
                double n = std::stod(sayi, &kac);
                if (kac == sayi.size()) {          // tamami sayi olmali
                    char b[64];
                    sprintf_s(b, "%.0f XP", n * c);
                    yeni = RValue(b);
                    A2.assign(A, A + argc);
                    A2[0] = &yeni;
                }
            }
        } catch (...) {}
    }
    RValue** kullan = A2.empty() ? A : A2.data();
    return g_OrigCombatText ? g_OrigCombatText(S, O, R, argc, kullan) : R;
}

DROP_HOOK(DropRelic)
DROP_HOOK(DropBossGems)
DROP_HOOK(DropDungeonKeys)
DROP_HOOK(DropBossRunes)
DROP_HOOK(DropBattleFragments)
DROP_HOOK(DropDimensionalShard)
DROP_HOOK(DropBifrostKey)
DROP_HOOK(DropGold)
DROP_HOOK(DropItemBoss)
DROP_HOOK(DropItem)
DROP_HOOK(CreateItemDrop)
DROP_HOOK(DropItemAngelic)
DROP_HOOK(DropAngelicKey)
DROP_HOOK(DropAngelicCharm)
// Asil trafik bu yollardan geciyor - olculdu: DropGold 4 cagri, DropMonsterGold
// yaratik basina.  DropDungeonKeys hic cagrilmiyor, anahtarlar DropKeys'ten.
DROP_HOOK(DropMonsterGold)
#ifdef FORGEPACT_RELEASE
DROP_HOOK(DropKeys)
#else
static PFUNC_YYGMLScript g_Orig_DropKeys = nullptr; static volatile long g_cnt_DropKeys = 0; static int g_mult_DropKeys = 1;
static RValue& Hook_DropKeys(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    InterlockedIncrement(&g_cnt_DropKeys);
    for (int i = 1; i < g_mult_DropKeys; i++) { RValue t; if (g_Orig_DropKeys) g_Orig_DropKeys(S, O, t, argc, A); }
    RValue& _res = g_Orig_DropKeys ? g_Orig_DropKeys(S, O, R, argc, A) : R;
    BP_LOGDROP("DropKeys", _res, argc, A);
    // Hangi anahtar secildi, neden - kullanici gozlemi: yalnizca Chaos/Basic/Crystal dusuyor.
    try {
        std::string ad = "?";
        if (_res.m_Kind == VALUE_OBJECT) {
            RValue info = g_Yytk->CallBuiltin("variable_struct_get", { _res, RValue("itemInfoStruct") });
            if (info.m_Kind == VALUE_OBJECT) {
                RValue nm = g_Yytk->CallBuiltin("variable_struct_get", { info, RValue("28") });
                ad = nm.ToString();
            }
        }
        RValue rm = g_Yytk->CallBuiltin("variable_global_get", { RValue("room") });
        std::ofstream f(IPC_DIR + "\\keychoice.txt", std::ios::app);
        f << "DropKeys -> " << ad << "   room=" << (int)rm.ToDouble() << "\n";
        f.flush();
    } catch (...) {}
    return _res;
}
#endif
DROP_HOOK(DropChaosKey)
DROP_HOOK(DropRubyKey)
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
    HookOneScript("DropBossGems",        "bp_dgems",    (PVOID)Hook_DropBossGems,        &g_Orig_DropBossGems);
    HookOneScript("DropDungeonKeys",     "bp_ddkeys",   (PVOID)Hook_DropDungeonKeys,     &g_Orig_DropDungeonKeys);
    HookOneScript("DropBossRunes",       "bp_drunes",   (PVOID)Hook_DropBossRunes,       &g_Orig_DropBossRunes);
    HookOneScript("DropBattleFragments", "bp_dfrag",    (PVOID)Hook_DropBattleFragments, &g_Orig_DropBattleFragments);
    HookOneScript("DropDimensionalShard","bp_dshard",   (PVOID)Hook_DropDimensionalShard,&g_Orig_DropDimensionalShard);
    HookOneScript("DropBifrostKey",      "bp_dbifrost", (PVOID)Hook_DropBifrostKey,      &g_Orig_DropBifrostKey);
    HookOneScript("DropGold",            "bp_dgold",    (PVOID)Hook_DropGold,            &g_Orig_DropGold);
    HookOneScript("DropItemBoss",        "bp_dibos",    (PVOID)Hook_DropItemBoss,        &g_Orig_DropItemBoss);
    HookOneScript("DropItem",            "bp_ditem",    (PVOID)Hook_DropItem,            &g_Orig_DropItem);
    HookOneScript("CreateItemDrop",      "bp_citemd",   (PVOID)Hook_CreateItemDrop,      &g_Orig_CreateItemDrop);
    HookOneScript("DropItemAngelic",     "bp_dangit",   (PVOID)Hook_DropItemAngelic,     &g_Orig_DropItemAngelic);
    HookOneScript("DropAngelicKey",      "bp_dangkey",  (PVOID)Hook_DropAngelicKey,      &g_Orig_DropAngelicKey);
    HookOneScript("DropAngelicCharm",    "bp_dangchm",  (PVOID)Hook_DropAngelicCharm,    &g_Orig_DropAngelicCharm);
    HookOneScript("DropMonsterGold",     "bp_dmgold",   (PVOID)Hook_DropMonsterGold,     &g_Orig_DropMonsterGold);
    HookOneScript("DropKeys",            "bp_dkeys",    (PVOID)Hook_DropKeys,            &g_Orig_DropKeys);
    HookOneScript("DropChaosKey",        "bp_dckey",    (PVOID)Hook_DropChaosKey,        &g_Orig_DropChaosKey);
    HookOneScript("DropRubyKey",         "bp_drkey",    (PVOID)Hook_DropRubyKey,         &g_Orig_DropRubyKey);
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
    HookOneScript("anon@849@gml_Object_Enemy_Creator_obj_Create_0", "bp_tr_checkspawn", (PVOID)Hook_TraceCreatorCheckSpawn, &g_OrigCreatorCheckSpawn);
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
    HookOneScript("DrawTooltip",         "bp_drawtip",  (PVOID)Hook_DrawTooltip,         &g_Orig_DrawTooltip);
    InstallForgedTooltipHooks();   // research build: always, so tiptrace can watch any item
    HookOneScript("draw_text_outline",   "bp_dto",      (PVOID)Hook_TraceDrawTextOutline, &g_Orig_DrawTextOutline);
    HookOneScript("GetItemTooltipString","bp_gitip",    (PVOID)Hook_GetItemTooltipString,&g_Orig_GetItemTooltipString);
    HookOneScript("GetItemStatString",   "bp_gistat",   (PVOID)Hook_GetItemStatString,   &g_Orig_GetItemStatString);
    if (!g_Orig_CreateItemNew)
        HookOneScript("CreateItemNew",       "bp_citemn",   (PVOID)Hook_CreateItemNew,       &g_Orig_CreateItemNew);
    if (!g_Orig_CreateItemInit)
        HookOneScript("CreateItemInit",      "bp_citemi",   (PVOID)Hook_CreateItemInit,      &g_Orig_CreateItemInit);
    if (!g_Orig_GenerateItemRandomStats)
        HookOneScript("GenerateItemRandomStats","bp_girs",  (PVOID)Hook_GenerateItemRandomStats,&g_Orig_GenerateItemRandomStats);
    HookOneScript("LootGroundCreate",    "bp_lgc",      (PVOID)Hook_LootGroundCreate,    &g_Orig_LootGroundCreate);
    HookOneScript("LootGroundCreateFromItem", "bp_lgcfi", (PVOID)Hook_LootGroundCreateFromItem, &g_Orig_LootGroundCreateFromItem);
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
    sprintf_s(b, "dropstats: Relic c=%ld x%d | BossGems c=%ld x%d | DungeonKeys c=%ld x%d | BossRunes c=%ld x%d",
        g_cnt_DropRelic, g_mult_DropRelic, g_cnt_DropBossGems, g_mult_DropBossGems,
        g_cnt_DropDungeonKeys, g_mult_DropDungeonKeys, g_cnt_DropBossRunes, g_mult_DropBossRunes);
    Out(b);
    sprintf_s(b, "          BattleFrag c=%ld x%d | DimShard c=%ld x%d | Bifrost c=%ld x%d | Gold c=%ld x%d",
        g_cnt_DropBattleFragments, g_mult_DropBattleFragments, g_cnt_DropDimensionalShard, g_mult_DropDimensionalShard,
        g_cnt_DropBifrostKey, g_mult_DropBifrostKey, g_cnt_DropGold, g_mult_DropGold);
    Out(b);
    sprintf_s(b, "          ItemBoss c=%ld x%d | Item c=%ld x%d | CreateItemDrop c=%ld x%d",
        g_cnt_DropItemBoss, g_mult_DropItemBoss, g_cnt_DropItem, g_mult_DropItem,
        g_cnt_CreateItemDrop, g_mult_CreateItemDrop);
    Out(b);
    // Asil trafigin gectigi yollar - DropGold/DropDungeonKeys neredeyse hic
    // cagrilmiyor, gercek altin ve anahtarlar buradan geliyor.
    sprintf_s(b, "          MonsterGold c=%ld x%d | Keys c=%ld x%d | ChaosKey c=%ld x%d | RubyKey c=%ld x%d",
        g_cnt_DropMonsterGold, g_mult_DropMonsterGold, g_cnt_DropKeys, g_mult_DropKeys,
        g_cnt_DropChaosKey, g_mult_DropChaosKey, g_cnt_DropRubyKey, g_mult_DropRubyKey);
    Out(b);
}

static void SetDropMult(const std::string& name, int n)
{
    std::string l = Lower(name);
    if (n < 1) n = 1;
    // Shipped builds start without drop hooks. Install them only when a real
    // multiplier is requested; x1 is native behaviour and needs no interception.
    if (n > 1) InstallDropMultHooks();
    // Tek kaydirac, ilgili BUTUN yollari ayarlar (olculdu: tek fonksiyon yetmiyor).
    if (l == "relic") { g_mult_DropRelic = n; }
    else if (l == "bossgems" || l == "gems") { g_mult_DropBossGems = n; }
    else if (l == "dungeonkeys" || l == "keys") {
        g_mult_DropDungeonKeys = n; g_mult_DropKeys = n;
        g_mult_DropChaosKey = n; g_mult_DropRubyKey = n;
    }
    else if (l == "bossrunes" || l == "runes") { g_mult_DropBossRunes = n; }
    else if (l == "battlefragments" || l == "frags") { g_mult_DropBattleFragments = n; }
    else if (l == "dimshard" || l == "shards") { g_mult_DropDimensionalShard = n; }
    else if (l == "bifrost") { g_mult_DropBifrostKey = n; }
    else if (l == "gold") { g_mult_DropGold = n; g_mult_DropMonsterGold = n; }
    else if (l == "bossitem" || l == "itemboss") { g_mult_DropItemBoss = n; }
    else if (l == "item") { g_mult_DropItem = n; }
    else if (l == "createitem") { g_mult_CreateItemDrop = n; }
    else if (l == "angelic" || l == "angelicitem") { g_mult_DropItemAngelic = n; }
    else if (l == "angelickey") { g_mult_DropAngelicKey = n; }
    else if (l == "angeliccharm") { g_mult_DropAngelicCharm = n; }
    else { Out("dropmult: unknown '" + name + "' (relic|gems|keys|runes|frags|shards|bifrost|gold|bossitem|item|createitem|angelic|angelickey|angeliccharm)"); return; }
    Out("dropmult " + name + " -> " + std::to_string(n));
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
        if (!HookOneScript(ad.c_str(), kimlik[i], SayacKancasi(i), &g_Yuva[i].orij)) {
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
        if (!*k.o) HookOneScript(k.ad, k.kimlik, k.k, k.o);
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
    LoadCustomForgeEntries();
    InstallCustomForgeItemHooks();
    HeadhunterAutoArm();
    TyrantAutoArm();
    BeaconAutoArm();

    // Development builds install the complete research surface eagerly.
    // Player builds install only the functional hook group requested by a
    // non-vanilla command (density/specialrate/dropmult).
#ifndef FORGEPACT_RELEASE
    InstallCreateHooks();
    InstallDropMultHooks();
    InstallNecroBalanceHooks();
#endif

#ifdef FORGEPACT_RELEASE
    // Yayin derlemesi: arastirma kancasi ve teshis gunlugu yok.
    g_HookInstalled = true;
    Out("BloodPact: yayin modu (density + ozel icerik + minimap)");
    return;
#else
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

// Auto map-reveal: fill each zone's discovered grid once.  The previous version
// cleared the already-revealed grid every 20 frames for the entire session,
// producing avoidable GameMaker calls during dense combat.
static bool g_AutoReveal = false;
static int64_t g_AutoRevealLastInstance = INT64_MIN;
static int64_t g_AutoRevealLastGrid = INT64_MIN;
static int64_t g_AutoRevealLastRoom = INT64_MIN;

static void ResetAutoRevealIdentity()
{
    g_AutoRevealLastInstance = INT64_MIN;
    g_AutoRevealLastGrid = INT64_MIN;
    g_AutoRevealLastRoom = INT64_MIN;
}

static void AutoRevealTick()
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("objMinimap") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) { ResetAutoRevealIdentity(); return; }
        RValue ex0 = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue("minimapDiscoveredGrid") });
        if (!ex0.ToBoolean()) { ResetAutoRevealIdentity(); return; }
        RValue grid = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("minimapDiscoveredGrid") });
        double gid = grid.ToDouble();
        if (gid < 0) { ResetAutoRevealIdentity(); return; }
        RValue ex = g_Yytk->CallBuiltin("ds_exists", { RValue(gid), RValue(1.0) });
        if (!ex.ToBoolean()) { ResetAutoRevealIdentity(); return; }

        int64_t roomKey = INT64_MIN;
        CInstance* global = nullptr;
        if (AurieSuccess(g_Yytk->GetGlobalInstance(&global)) && global) {
            RValue* room = nullptr;
            if (AurieSuccess(g_Yytk->GetInstanceMember(RValue(global), "room", room)) && room) {
                try { roomKey = static_cast<int64_t>(std::llround(room->ToDouble())); } catch (...) {}
            }
        }

        const int64_t instanceKey = static_cast<int64_t>(std::llround(id.ToDouble()));
        const int64_t gridKey = static_cast<int64_t>(std::llround(gid));
        if (instanceKey == g_AutoRevealLastInstance &&
            gridKey == g_AutoRevealLastGrid && roomKey == g_AutoRevealLastRoom)
            return;

        g_Yytk->CallBuiltin("ds_grid_clear", { RValue(gid), RValue(1.0) });
        g_AutoRevealLastInstance = instanceKey;
        g_AutoRevealLastGrid = gridKey;
        g_AutoRevealLastRoom = roomKey;
    } catch (...) {}
}

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
static double g_AngelicRateMult = 1.0;   // 1 = the game's own rate

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
                    sprintf_s(b, "angelic: roll #%ld  chance=%g", n, chance);
                    Out(b);
                }
            } else if (n <= 5) {
                Out("angelic: roll with a non-finite chance value, not logged");
            }
        } catch (...) {}
    }
#endif
    if (g_AngelicRateMult > 1.0 && argc >= 3 && A && A[2] && A[2]->m_Kind == VALUE_REAL) {
        try {
            double c = A[2]->ToDouble();
            if (std::isfinite(c) && c > 0.0) {
                *A[2] = RValue(c * g_AngelicRateMult);
#ifndef FORGEPACT_RELEASE
                long hits = InterlockedIncrement(&g_AngRateHits);
                if (hits <= 5) {
                    char b[192];
                    sprintf_s(b, "angelic: chance %g -> %g in place (x%.2f)",
                              c, c * g_AngelicRateMult, g_AngelicRateMult);
                    Out(b);
                }
#endif
            }
        } catch (...) {}
    }
    return g_OrigAngChance ? g_OrigAngChance(S, O, R, argc, A) : R;
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
        && !HookOneScript("cpr_irandom", "fp_cprir", (PVOID)HookCprIrandom, &g_OrigCprIrandom)) {
        Out("socketprobe: could not hook cpr_irandom"); return;
    }
    if (!g_OrigCreateItemNew
        && !HookOneScript("CreateItemNew", "fp_citemnew", (PVOID)HookCreateItemNew, &g_OrigCreateItemNew)) {
        Out("socketprobe: could not hook CreateItemNew"); return;
    }
    if (!g_OrigLoadCommonItems
        && !HookOneScript("LoadCommonItems", "fp_loadcommon", (PVOID)HookLoadCommonItems, &g_OrigLoadCommonItems)) {
        Out("socketprobe: could not hook LoadCommonItems"); return;
    }
    if (!g_OrigCreateItemInit
        && !HookOneScript("CreateItemInit", "fp_citeminit", (PVOID)HookCreateItemInit, &g_OrigCreateItemInit)) {
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
        if (g_AngelicRateMult > 1.0) {
            if (ResolveRateKeys() && !g_OrigGpvRate)
                HookOneScript("GPV", "fp_gpvrate", (PVOID)HookGpvRate, &g_OrigGpvRate);
        }
        char b[176];
        sprintf_s(b, "raredrop angelic: x%.2f -> gate open, rate x%.2f (x2 = the game's own rate)",
                  mult, g_AngelicRateMult);
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

static void StatCmd(const std::string& rest)
{
    std::string a1, a2; a1 = FirstToken(rest, a2);
    while (!a1.empty() && std::isspace((unsigned char)a1.back())) a1.pop_back();
    while (!a2.empty() && std::isspace((unsigned char)a2.back())) a2.pop_back();

    struct Kayit { const char* ad; const char* kimlik; PVOID kanca; PFUNC_YYGMLScript* orij; volatile long* sayac; double* carpan; const char* takma; };
    static const Kayit kTablo[] = {
        { "StatMagicFind", "fp_st_mf",      (PVOID)HookStat_StatMagicFind,      &g_OrigStat_StatMagicFind, &g_StatSayac_StatMagicFind, &g_StatCarpan_StatMagicFind, "magicfind" },
        { "StatAttackSpeed", "fp_st_as", (PVOID)HookStat_StatAttackSpeed, &g_OrigStat_StatAttackSpeed, &g_StatSayac_StatAttackSpeed, &g_StatCarpan_StatAttackSpeed, "attackspeed" },
        { "StatExperienceGain", "fp_st_xp", (PVOID)HookStat_StatExperienceGain, &g_OrigStat_StatExperienceGain, &g_StatSayac_StatExperienceGain, &g_StatCarpan_StatExperienceGain, "expgain" },
        { "StatMovementSpeed", "fp_st_ms",  (PVOID)HookStat_StatMovementSpeed,  &g_OrigStat_StatMovementSpeed, &g_StatSayac_StatMovementSpeed, &g_StatCarpan_StatMovementSpeed, "movespeed" },
        { "CalculateEndDamage", "fp_st_td", (PVOID)HookStat_CalculateEndDamage, &g_OrigStat_CalculateEndDamage, &g_StatSayac_CalculateEndDamage, &g_StatCarpan_CalculateEndDamage, "damage" },
        { "StatExtraGold", "fp_st_eg",      (PVOID)HookStat_StatExtraGold,      &g_OrigStat_StatExtraGold, &g_StatSayac_StatExtraGold, &g_StatCarpan_StatExtraGold, "extragold" },
        { "StatLifeReplenish", "fp_st_lr",  (PVOID)HookStat_StatLifeReplenish,  &g_OrigStat_StatLifeReplenish, &g_StatSayac_StatLifeReplenish, &g_StatCarpan_StatLifeReplenish, "lifereplenish" },
        { "StatManaReplenish", "fp_st_mr",  (PVOID)HookStat_StatManaReplenish,  &g_OrigStat_StatManaReplenish, &g_StatSayac_StatManaReplenish, &g_StatCarpan_StatManaReplenish, "manareplenish" },
        { "StatDefense", "fp_st_def",        (PVOID)HookStat_StatDefense,        &g_OrigStat_StatDefense, &g_StatSayac_StatDefense, &g_StatCarpan_StatDefense, "defense" },
        { "StatCritDamage", "fp_st_cd",      (PVOID)HookStat_StatCritDamage,     &g_OrigStat_StatCritDamage, &g_StatSayac_StatCritDamage, &g_StatCarpan_StatCritDamage, "critdamage" },
        { "StatCritRate", "fp_st_cr",        (PVOID)HookStat_StatCritRate,       &g_OrigStat_StatCritRate, &g_StatSayac_StatCritRate, &g_StatCarpan_StatCritRate, "critchance" },
        { "StatSpellCritDamage", "fp_st_scd", (PVOID)HookStat_StatSpellCritDamage, &g_OrigStat_StatSpellCritDamage, &g_StatSayac_StatSpellCritDamage, &g_StatCarpan_StatSpellCritDamage, "spellcritdamage" },
        { "StatSpellCritRate", "fp_st_scr",  (PVOID)HookStat_StatSpellCritRate,  &g_OrigStat_StatSpellCritRate, &g_StatSayac_StatSpellCritRate, &g_StatCarpan_StatSpellCritRate, "spellcritchance" },
        { "EnemyCalculateExperience", "fp_st_xpc", (PVOID)HookStat_EnemyCalculateExperience, &g_OrigStat_EnemyCalculateExperience, &g_StatSayac_EnemyCalculateExperience, &g_StatCarpan_EnemyCalculateExperience, "exp" },
    };

    if (Lower(a1) == "list" || a1.empty()) {
        for (const auto& k : kTablo) {
            char b[160];
            sprintf_s(b, "  %-26s (%-11s) x%.2f  %-12s cagri=%ld", k.ad, k.takma ? k.takma : "-", *k.carpan,
                      *k.orij ? "kanca kurulu" : "kanca yok", *k.sayac);
            Out(b);
        }
        return;
    }

    // Kisa ad da kabul et: "magicfind" -> "StatMagicFind"
    std::string ara = Lower(a1);
    const Kayit* hedef = nullptr;
    for (const auto& k : kTablo) {
        std::string tam = Lower(k.ad);
        if (ara == tam || ara == tam.substr(4)
            || (k.takma && ara == Lower(k.takma))) { hedef = &k; break; }
    }
    if (!hedef) { Out("stat: bilinmeyen ad '" + a1 + "'  (stat list ile bak)"); return; }

    double c = 1.0;
    try { c = std::stod(a2); } catch (...) { Out("stat: carpan sayi olmali"); return; }
    if (c < 0.0) c = 0.0;

    if (c == 1.0 && !*hedef->orij) {
        *hedef->carpan = 1.0;
        Out(std::string("stat ") + hedef->ad + " -> x1.00 (native, no hook)");
        return;
    }
    if (!*hedef->orij) {
        // Betik adindaki "gml_Script_" onekini HookOneScript kendisi ekliyor.
        HookOneScript(hedef->ad, hedef->kimlik, hedef->kanca, hedef->orij);
        if (!*hedef->orij) { Out(std::string("stat: ") + hedef->ad + " kancasi kurulamadi"); return; }
    }
    *hedef->carpan = c;
    // XP carpani acilinca baloncuk metnini de duzelt (yalnizca gorsel).
    if (c != 1.0 && std::string(hedef->ad) == "EnemyCalculateExperience" && !g_OrigCombatText)
        HookOneScript("CombatText", "fp_ctext", (PVOID)Hook_CombatText, &g_OrigCombatText);
    char b[160];
    sprintf_s(b, "stat %s -> x%.2f", hedef->ad, c);
    Out(b);
}

static void StatAddCmd(const std::string& rest)
{
    std::string ad, deger; ad = FirstToken(rest, deger);
    while (!ad.empty() && std::isspace((unsigned char)ad.back())) ad.pop_back();
    while (!deger.empty() && std::isspace((unsigned char)deger.back())) deger.pop_back();
    std::string ara = Lower(ad);
    if (ara != "castrate" && ara != "statfastercastrate" && ara != "fastercastrate") {
        Out("statadd: unknown '" + ad + "' (castrate)");
        return;
    }
    double ek = 0.0;
    try { ek = std::stod(deger); } catch (...) { Out("statadd: bonus sayi olmali"); return; }
    if (ek < 0.0) ek = 0.0;
    if (ek == 0.0 && !g_OrigStatAdd_StatFasterCastRate) {
        g_StatEk_StatFasterCastRate = 0.0;
        Out("statadd StatFasterCastRate -> +0.00 (native, no hook)");
        return;
    }
    if (!g_OrigStatAdd_StatFasterCastRate) {
        HookOneScript("StatFasterCastRate", "fp_sta_fcr",
                      (PVOID)HookStatAdd_StatFasterCastRate,
                      &g_OrigStatAdd_StatFasterCastRate);
        if (!g_OrigStatAdd_StatFasterCastRate) {
            Out("statadd: StatFasterCastRate kancasi kurulamadi");
            return;
        }
    }
    g_StatEk_StatFasterCastRate = ek;
    char b[128];
    sprintf_s(b, "statadd StatFasterCastRate -> +%.2f", ek);
    Out(b);
}

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
    g_mult_DropItem      = on ? 3 : 1;
    g_mult_DropItemBoss  = on ? 3 : 1;
    g_mult_DropGold      = on ? 3 : 1;
    g_mult_DropRelic     = on ? 2 : 1;
    g_mult_DropBossGems  = on ? 2 : 1;
    g_mult_DropDungeonKeys = on ? 2 : 1;
    g_AutoReveal = on;
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
        std::ofstream f(IPC_DIR + "\\census.txt", std::ios::app);
        f << "kare=" << fc << "  TOPLAM=" << (long long)all.ToDouble();
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
// Vanilya droprate.base degerleri - carpan HER ZAMAN bunlardan hesaplanir,
// yoksa ust uste uygulayinca 5x5=25 olur.  Anahtar: kategori*1000 + indeks.
static std::map<int, double> g_DropRateVanilya;

static bool RepoStruct(int kategori, int indeks, RValue& out)
{
    if (!RepoIndexValid(kategori, indeks)) return false;
    try {
        out = g_Yytk->CallGameScript("gml_Script_GetNormalRepoStruct",
                                     { RValue((double)kategori), RValue(0.0), RValue((double)indeks) });
        return out.m_Kind == VALUE_OBJECT;
    } catch (...) { return false; }
}

// Esyanin ic adi: itemBaseInfoStruct["28"]
static std::string RepoAd(RValue& st)
{
    try {
        RValue info = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("itemBaseInfoStruct") });
        if (info.m_Kind != VALUE_OBJECT) return "?";
        RValue nm = g_Yytk->CallBuiltin("variable_struct_get", { info, RValue("28") });
        return nm.ToString();
    } catch (...) { return "?"; }
}

// Esyanin VANILYA base degeri.  Ilk cagrida o anki deger saklanir.
static double VanilyaBase(int kategori, int indeks, RValue& st, RValue& drOut)
{
    drOut = g_Yytk->CallBuiltin("variable_struct_get", { st, RValue("droprate") });
    if (drOut.m_Kind != VALUE_OBJECT) return -1.0;
    RValue simdi = g_Yytk->CallBuiltin("variable_struct_get", { drOut, RValue("base") });
    int anahtar = kategori * 1000 + indeks;
    auto it = g_DropRateVanilya.find(anahtar);
    if (it == g_DropRateVanilya.end()) {
        g_DropRateVanilya[anahtar] = simdi.ToDouble();
        return simdi.ToDouble();
    }
    return it->second;
}

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
            sprintf_s(ln, "   [%3d] %-34s base=%.0f", i, ad.c_str(), b);
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
                      v, sayac, ornekEski, ornekYeni);
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
                          g->ad, v, n, ornekEski, ornekYeni,
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
                  grup.c_str(), v, sayac, ornekEski, ornekYeni);
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
            sprintf_s(ln, "droprate set [%d] %s : %.0f -> %.0f", i, ad.c_str(), eski.ToDouble(), v);
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
            if (g_OrigLoadDrops) g_OrigLoadDrops(S, O, r2, argc, A2.data());
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
        // "all" -> tarama suresince TUM esyalarin ic zari da 1'e cekilir
        g_TypeMapTumOranlar = (Lower(bayrak).find("all") != std::string::npos);
        if (!g_OrigLoadDrops)
            HookOneScript("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
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
            HookOneScript("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
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
            if (d <= 0.0) d = 1.0;
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
            char cb[56]; sprintf_s(cb, "(x%.0f olcek %.5f)", kc, TipOlcek(x));
            liste += cb;
        }
        Out(std::string("dungeonkey: ") + (g_DkOn ? "ACIK" : "kapali")
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
                    try { g_DkTipCarpan[n] = std::stod(carpStr); } catch (...) {}
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
            if (m < 1.0) m = 1.0;
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
        if (!g_OrigLoadDrops) HookOneScript("LoadDrops", "fp_loaddrops", (PVOID)Hook_LoadDrops, &g_OrigLoadDrops);
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
        if (on == "on" || on == "1" || on == "force") { g_HhForced.store(on == "force"); InstallHeadhunterHook(); g_HhEnabled.store(g_HhHookInstalled); }
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
#endif
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
        "enemyspeed"
    };
    if (kPlayerCommands.find(lc) == kPlayerCommands.end()) {
        Out("command unavailable in player build: " + cmd);
        return;
    }
#endif

    if (HandleHeadhunterCommand(lc, rest)) return;
    if (lc == "ping") {
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
        try {
            double d = std::stod(rest);
            if (d < 1.0) d = 1.0;
            if (d > 1.0) {
                InstallCreateHooks();
                InstallDensityLifecycleHooks();
            }
            g_CreatorMult = d;
            g_CreatorFrac = 0.0;          // kademe degisince birikim sifirlanir
            if (d > 1.0) OpenDensityWindow();
            char db[64]; sprintf_s(db, "%.2g", g_CreatorMult);
            Out(std::string("density (creator mult) -> ") + db);
        }
        catch (...) { Out("density: bad value"); }
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
#ifndef FORGEPACT_RELEASE
    } else if (lc == "scount") {
        SCountCmd(rest);
    } else if (lc == "gpvlog") {
        std::string v = Lower(rest);
        while (!v.empty() && std::isspace((unsigned char)v.back())) v.pop_back();
        if (v == "off") { g_GpvLog = 0; Out("gpvlog -> KAPALI"); }
        else if (v == "stat") {
            Out("gpvlog: " + std::to_string(g_GpvGorulen.size()) + " farkli kayit -> bp_ipc\gpv.txt");
        } else {
            if (!g_OrigGPV) HookOneScript("GPV", "fp_gpv", (PVOID)HookGPV, &g_OrigGPV);
            if (!g_OrigSPV) HookOneScript("SPV", "fp_spv", (PVOID)HookSPV, &g_OrigSPV);
            g_GpvLog = 1;
            Out(std::string("gpvlog -> ACIK  (GPV kanca=") + (g_OrigGPV ? "var" : "YOK")
                + ", SPV kanca=" + (g_OrigSPV ? "var" : "YOK") + ")  -> bp_ipc\gpv.txt");
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
            g_CreatorMult, g_ExtraCreators, g_DensityRevisitSkips,
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
        g_AutoReveal = !(v == "0" || v == "off" || v == "false");
        Out(std::string("reveal: ") + (g_AutoReveal ? "ACIK" : "KAPALI"));
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
        StatCmd(rest);
    } else if (lc == "statadd") {
        StatAddCmd(rest);
    } else if (lc == "raredrop") {
        RareDropCmd(rest);
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

static void PollCommands()
{
    std::string cmdPath = CmdPath();
    std::ifstream in(cmdPath, std::ios::binary);
    if (!in.good()) return;
    std::stringstream ss; ss << in.rdbuf();
    std::string content = ss.str();
    in.close();
    if (content.empty()) return;

    // delete immediately so we don't reprocess
    DeleteFileA(cmdPath.c_str());

    Out("---- running command file ----");
    std::stringstream ls(content);
    std::string line;
    while (std::getline(ls, line)) {
        if (line.empty()) continue;
        try { RunCommand(line); }
        catch (...) { Out("command threw: " + line); }
    }
    Out("---- done ----");
}

void FrameCallback(FWFrame& FrameContext)
{
    UNREFERENCED_PARAMETER(FrameContext);
    static uint32_t fc = 0;
    g_RuntimeFrame = fc;
    FlushItemStats(fc);
    if (fc == 1) Trace("0-framecallback-running");

    // Special Content uses the game's eSt gates.  The helper is also safe in
    // all-off mode: it returns immediately while g_EstForce is empty.
    EstForceApply();
    ++g_HhFrame;
    KuyrukIsle();
#ifndef FORGEPACT_RELEASE
    CensusTick(fc);
#endif

    // one-time setup once the runner is fully alive: load config + install hook
    if (!g_Setup && fc > 60) {
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
        g_CreatorMult = (g_CreatorMult > 1.0) ? 1.0 : 3.0;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%g", g_CreatorMult);
    }
    if (f9 && !f9p) {
        if (g_CreatorMult < 20.0) g_CreatorMult += 0.5;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%g", g_CreatorMult);
    }
    if (f7 && !f7p) {
        if (g_CreatorMult > 1.0) g_CreatorMult -= 0.5;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%g", g_CreatorMult);
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
    // auto map reveal (every ~20 frames so each new map clears quickly)
    if (g_AutoReveal && (fc % 20) == 0) {
        try { AutoRevealTick(); } catch (...) {}
    }

#ifndef FORGEPACT_RELEASE
    static bool f5p = false;
    bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5 && !f5p) {
        g_AutoReveal = !g_AutoReveal;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Auto map-reveal: %s", g_AutoReveal ? "ON" : "OFF");
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
        try { PollCommands(); } catch (...) {}
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
    if (!g_Yytk) return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

    CreateDirectoryA(IPC_DIR.c_str(), nullptr);
    Out("==== BloodPact plugin loaded ====");
    LoadStartup();   // oyun kodu calismadan once uygulanmasi gereken ayarlar
#ifdef FORGEPACT_RELEASE
    KonsoluGizle();
#endif

    AurieStatus st = g_Yytk->CreateCallback(Module, EVENT_FRAME, (PVOID)FrameCallback, 0);
    InstallHeadLabelHook();
    if (!AurieSuccess(st))
        Out("FAILED to register frame callback st=" + std::to_string((int)st));
    else
        g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] ready - watching bp_ipc\\cmd.txt");

    return AURIE_SUCCESS;
}
