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
#include <vector>
#include <cctype>
#include <unordered_map>
#include <map>
#include <cmath>
#include <intrin.h>

using namespace Aurie;
using namespace YYTK;

static YYTKInterface* g_Yytk = nullptr;

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
    std::ofstream f(IPC_DIR + "\\loadtrace.txt", std::ios::app);
    f << phase << "\n";
    f.flush();
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
static std::map<int, int>  g_ObjMult;        // object index -> spawn multiplier
static bool g_LogCreates = true;
static int  g_WatchObj = -1;                 // when this object is created, log the caller RVA
static std::string g_WatchCallers;           // distinct caller RVAs of the watched object's creation
static int  g_EnemyParentIdx = -1;           // asset index of Enemy_Parent_obj
static int  g_EnemyMultAll = 1;              // multiplier applied to ALL enemy descendants (direct)
static int  g_CreatorMult = 3;               // multiplier applied to ALL Enemy_Creator* spawners (density) - default 3x ON
static std::map<int, bool> g_IsEnemyCache;   // object index -> is enemy descendant
static std::map<int, bool> g_IsCreatorCache; // object index -> name starts with "Enemy_Creator"
static volatile long g_ExtraCreators = 0;    // extra spawner instances the multiplier created
static volatile long g_ExtraEnemies = 0;     // extra enemy instances the multiplier created

static bool IsCreatorObject(int objIdx)
{
    if (objIdx < 0) return false;
    auto it = g_IsCreatorCache.find(objIdx);
    if (it != g_IsCreatorCache.end()) return it->second;
    bool res = false;
    try {
        RValue n = g_Yytk->CallBuiltin("object_get_name", { RValue((double)objIdx) });
        std::string name = n.ToString();
        res = (name.rfind("Enemy_Creator", 0) == 0);
    } catch (...) { res = false; }
    g_IsCreatorCache[objIdx] = res;
    return res;
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

static void DoMultiCreate(TRoutine orig, RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args, int objIdx)
{
    if (objIdx >= 0 && g_LogCreates) g_CreateCounts[objIdx]++;
    int mult = 1;
    auto it = g_ObjMult.find(objIdx);
    if (it != g_ObjMult.end()) mult = it->second;
    // density: multiply all Enemy_Creator* spawners (produces fully-configured enemies)
    if (g_CreatorMult > 1 && IsCreatorObject(objIdx) && g_CreatorMult > mult)
        mult = g_CreatorMult;
    // (optional) direct enemy-descendant multiplier — off by default, creators are the right layer
    else if (g_EnemyMultAll > 1 && IsEnemyObject(objIdx) && g_EnemyMultAll > mult)
        mult = g_EnemyMultAll;
    if (mult > 1 && argc >= 4 && orig) {
        for (int i = 1; i < mult; i++) {
            try {
                std::vector<RValue> a(Args, Args + argc);
                a[0] = RValue(Args[0].ToDouble() + (double)(((i % 5) - 2) * 28));
                a[1] = RValue(Args[1].ToDouble() + (double)(((i / 5) - 2) * 28));
                RValue tmp;
                orig(tmp, S, O, argc, a.data());
                if (IsCreatorObject(objIdx)) InterlockedIncrement(&g_ExtraCreators);
                else InterlockedIncrement(&g_ExtraEnemies);
            } catch (...) {}
        }
    }
    orig(Result, S, O, argc, Args);
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
    void* ret = _ReturnAddress();
    int objIdx = -1;
    try { if (argc >= 4) objIdx = (int)Args[3].ToDouble(); } catch (...) {}
    WatchLog(ret, objIdx);
    if (g_OrigICD) DoMultiCreate(g_OrigICD, Result, S, O, argc, Args, objIdx);
}
static void HookICL(RValue& Result, CInstance* S, CInstance* O, int argc, RValue* Args)
{
    void* ret = _ReturnAddress();
    int objIdx = -1;
    try { if (argc >= 4) objIdx = (int)Args[3].ToDouble(); } catch (...) {}
    WatchLog(ret, objIdx);
    if (g_OrigICL) DoMultiCreate(g_OrigICL, Result, S, O, argc, Args, objIdx);
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
    HookBuiltin("instance_create_depth", "bp_icd", (PVOID)HookICD, &g_OrigICD);
    HookBuiltin("instance_create_layer", "bp_icl", (PVOID)HookICL, &g_OrigICL);
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
static void LogDrop(const char* fn, RValue& res, int argc, RValue** A)
{
    try {
        CInstance* g = nullptr; g_Yytk->GetGlobalInstance(&g);
        auto tryLog = [&](RValue* v) {
            if (!v || v->m_Kind != VALUE_OBJECT) return;
            RValue js; g_Yytk->CallBuiltinEx(js, "json_stringify", g, g, { *v });
            std::string s = js.ToString();
            if (s.find("itemDefinitionStruct") == std::string::npos) return; // must be an item
            if (!g_SeenDrop.insert(s).second) return;
            std::ofstream of(IPC_DIR + "\\itemdrops.jsonl", std::ios::app);
            of << "{\"fn\":\"" << fn << "\",\"it\":" << s << "}\n";
        };
        tryLog(&res);
        for (int i = 0; i < argc && i < 8; i++) if (A && A[i]) tryLog(A[i]);
    } catch (...) {}
}

// ===== Drop-rate hooks (multiply drop calls = more items per drop event) =====
#define DROP_HOOK(NAME) \
    static PFUNC_YYGMLScript g_Orig_##NAME = nullptr; \
    static volatile long g_cnt_##NAME = 0; \
    static int g_mult_##NAME = 1; \
    static RValue& Hook_##NAME(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) { \
        InterlockedIncrement(&g_cnt_##NAME); \
        for (int i = 1; i < g_mult_##NAME; i++) { RValue t; if (g_Orig_##NAME) g_Orig_##NAME(S, O, t, argc, A); } \
        RValue& _res = g_Orig_##NAME ? g_Orig_##NAME(S, O, R, argc, A) : R; \
        LogDrop(#NAME, _res, argc, A); \
        return _res; \
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
// item CREATION hooks: fire for every item built (incl. all jewels on save load).
// LogDrop captures (raw definition n -> computed itemStatStruct) automatically.
DROP_HOOK(CreateItemNew)
DROP_HOOK(CreateItemInit)
DROP_HOOK(GenerateItemRandomStats)

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
static PFUNC_YYGMLScript g_Orig_GetItemStatString = nullptr;
static RValue& Hook_GetItemStatString(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
    RValue& r = g_Orig_GetItemStatString ? g_Orig_GetItemStatString(S, O, R, argc, A) : R;
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

static void InstallDropHooks()
{
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
    HookOneScript("GetItemTooltipString","bp_gitip",    (PVOID)Hook_GetItemTooltipString,&g_Orig_GetItemTooltipString);
    HookOneScript("GetItemStatString",   "bp_gistat",   (PVOID)Hook_GetItemStatString,   &g_Orig_GetItemStatString);
    HookOneScript("CreateItemNew",       "bp_citemn",   (PVOID)Hook_CreateItemNew,       &g_Orig_CreateItemNew);
    HookOneScript("CreateItemInit",      "bp_citemi",   (PVOID)Hook_CreateItemInit,      &g_Orig_CreateItemInit);
    HookOneScript("GenerateItemRandomStats","bp_girs",  (PVOID)Hook_GenerateItemRandomStats,&g_Orig_GenerateItemRandomStats);
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
}

static void SetDropMult(const std::string& name, int n)
{
    std::string l = Lower(name);
    if (l == "relic") { g_mult_DropRelic = n; }
    else if (l == "bossgems" || l == "gems") { g_mult_DropBossGems = n; }
    else if (l == "dungeonkeys" || l == "keys") { g_mult_DropDungeonKeys = n; }
    else if (l == "bossrunes" || l == "runes") { g_mult_DropBossRunes = n; }
    else if (l == "battlefragments" || l == "frags") { g_mult_DropBattleFragments = n; }
    else if (l == "dimshard" || l == "shards") { g_mult_DropDimensionalShard = n; }
    else if (l == "bifrost") { g_mult_DropBifrostKey = n; }
    else if (l == "gold") { g_mult_DropGold = n; }
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

static void InstallHook()
{
    if (g_HookInstalled) { Out("hook already installed"); return; }
    g_Base = (uintptr_t)GetModuleHandleA(nullptr);
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
    InstallCreateHooks();
    InstallChaosTowerHooks();
    InstallDropHooks();
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

// Auto map-reveal: fill the current minimap discovered grid (ds_type_grid=1) every tick.
static bool g_AutoReveal = true;
static void AutoRevealTick()
{
    try {
        RValue oi = g_Yytk->CallBuiltin("asset_get_index", { RValue("objMinimap") });
        RValue id = g_Yytk->CallBuiltin("instance_find", { oi, RValue(0.0) });
        if (id.ToDouble() < 0) return;
        RValue ex0 = g_Yytk->CallBuiltin("variable_instance_exists", { id, RValue("minimapDiscoveredGrid") });
        if (!ex0.ToBoolean()) return;
        RValue grid = g_Yytk->CallBuiltin("variable_instance_get", { id, RValue("minimapDiscoveredGrid") });
        double gid = grid.ToDouble();
        if (gid < 0) return;
        RValue ex = g_Yytk->CallBuiltin("ds_exists", { RValue(gid), RValue(1.0) });
        if (!ex.ToBoolean()) return;
        g_Yytk->CallBuiltin("ds_grid_clear", { RValue(gid), RValue(1.0) });
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

// ===== Relic gate: NOP the conditional jump in gml_Script_DropItem that gates the relic
// drop, so the game's OWN relic drop fires on EVERY kill (then the relic dropmult multiplies
// it). Verified-safe length-preserving 6-byte NOP at VA 0x14207F837 (RVA 0x207F837). The
// original bytes are signature-checked first, so a different exe build is left untouched.
static BYTE g_RelicGateOrig[6] = { 0 };
static bool g_RelicGatePatched = false;
static void SetRelicGate(bool open)
{
    const uintptr_t RVA = 0x207F837;                                 // VA - imagebase(0x140000000)
    const BYTE EXPECT[6] = { 0x0F, 0x8E, 0xD3, 0x02, 0x00, 0x00 };   // jle 0x14207FB10
    const BYTE NOPS[6]   = { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 };
    BYTE* addr = (BYTE*)GetModuleHandleW(nullptr) + RVA;
    DWORD oldProt = 0;
    if (open) {
        if (g_RelicGatePatched) { Out("relicgate: already OPEN"); return; }
        if (memcmp(addr, EXPECT, 6) != 0) { Out("relicgate: signature mismatch (different exe build) - NOT patching"); return; }
        memcpy(g_RelicGateOrig, addr, 6);
        if (!VirtualProtect(addr, 6, PAGE_EXECUTE_READWRITE, &oldProt)) { Out("relicgate: VirtualProtect failed"); return; }
        memcpy(addr, NOPS, 6);
        VirtualProtect(addr, 6, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), addr, 6);
        g_RelicGatePatched = true;
        Out("relicgate: OPEN - DropItem now drops a relic every kill (scale with relic dropmult)");
    } else {
        if (!g_RelicGatePatched) { Out("relicgate: already CLOSED"); return; }
        if (!VirtualProtect(addr, 6, PAGE_EXECUTE_READWRITE, &oldProt)) { Out("relicgate: VirtualProtect failed"); return; }
        memcpy(addr, g_RelicGateOrig, 6);
        VirtualProtect(addr, 6, oldProt, &oldProt);
        FlushInstructionCache(GetCurrentProcess(), addr, 6);
        g_RelicGatePatched = false;
        Out("relicgate: CLOSED - relic drops back to normal");
    }
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

static void RunCommand(const std::string& line)
{
    std::string rest;
    std::string cmd = FirstToken(line, rest);
    if (cmd.empty()) return;
    std::string lc = Lower(cmd);

    if (lc == "ping") {
        short ma=0, mi=0, pa=0; g_Yytk->QueryVersion(ma, mi, pa);
        Out("pong (YYTK " + std::to_string(ma) + "." + std::to_string(mi) + "." + std::to_string(pa) + ")");
    } else if (lc == "script") {
        DoScriptLookup(rest);
    } else if (lc == "exists") {
        DoExists(rest);
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
        try { g_CreatorMult = std::stoi(rest); Out("density (creator mult) -> " + std::to_string(g_CreatorMult)); }
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
        char b[200];
        sprintf_s(b, "PROOF: density=x%d | extra spawners created=%ld | extra enemies created=%ld",
            g_CreatorMult, g_ExtraCreators, g_ExtraEnemies);
        Out(b);
    } else if (lc == "clearlog") {
        g_CreateCounts.clear(); Out("create log cleared");
    } else if (lc == "spawnat") {
        try { SpawnAtPlayer(std::stoi(rest)); } catch (...) { Out("spawnat: bad index"); }
    } else if (lc == "cb") {
        CallBuiltinCmd(rest);
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
    } else if (lc == "multobj") {
        std::string idx, num; idx = FirstToken(rest, num);
        try { int oi = std::stoi(idx); int n = std::stoi(num); g_ObjMult[oi] = n; Out("multobj " + idx + " -> " + num); }
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
    if (fc == 1) Trace("0-framecallback-running");

    // one-time setup once the runner is fully alive: load config + install hook
    if (!g_Setup && fc > 60) {
        g_Setup = true;
        Trace("1-setup-start");
        try { LoadConfig(); Trace("2-loadconfig-ok"); InstallHook(); Trace("3-installhook-ok"); }
        catch (...) { Out("setup EXCEPTION"); Trace("X-setup-cppexception"); }
        try { LoadCoopConfigAndMaybeStart(); Trace("4-coop-ok"); }
        catch (...) { Out("coop auto-start EXCEPTION"); Trace("X-coop-cppexception"); }
        try { SetRelicGate(true); } catch (...) {}   // relic gate ALWAYS ON (every kill drops a relic; only generates in Satanic Zones)
        Trace("5-setup-done");
    }

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
        g_CreatorMult = (g_CreatorMult > 1) ? 1 : 3;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%d", g_CreatorMult);
    }
    if (f9 && !f9p) {
        if (g_CreatorMult < 20) g_CreatorMult++;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%d", g_CreatorMult);
    }
    if (f7 && !f7p) {
        if (g_CreatorMult > 1) g_CreatorMult--;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Monster density x%d", g_CreatorMult);
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

    // auto map reveal (every ~20 frames so each new map clears quickly)
    if (g_AutoReveal && (fc % 20) == 0) {
        try { AutoRevealTick(); } catch (...) {}
    }

    static bool f5p = false;
    bool f5 = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (f5 && !f5p) {
        g_AutoReveal = !g_AutoReveal;
        if (g_Yytk) g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] Auto map-reveal: %s", g_AutoReveal ? "ON" : "OFF");
    }
    f5p = f5;

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

    if ((fc++ % 12) == 0) {
        try { PollCommands(); } catch (...) {}
    }
}

// ===== Single-instance bypass: clear ERROR_ALREADY_EXISTS on mutex/event creation so a
// 2nd game copy doesn't detect the 1st and self-exit. Installed as early as possible. =====
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

    AurieStatus st = g_Yytk->CreateCallback(Module, EVENT_FRAME, (PVOID)FrameCallback, 0);
    if (!AurieSuccess(st))
        Out("FAILED to register frame callback st=" + std::to_string((int)st));
    else
        g_Yytk->Print(CM_LIGHTGREEN, "[BloodPact] ready - watching bp_ipc\\cmd.txt");

    return AURIE_SUCCESS;
}
