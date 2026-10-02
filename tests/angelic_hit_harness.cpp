// The Python test (test_angelic_hit_behavior.py) inserts the real plugin types and functions
// below, compiled as the player build (FORGEPACT_RELEASE). Only the game is modelled: its
// Angelic list (a variable of the first Controller_obj instance), the builtins the plugin reads
// and writes it with, the Angelic roll (DropItemAngelicChance) picking an entry from that list,
// CreateDefaultParams, the placement that hands the parameters to the item builder, the Custom
// Forge hook that recognises a built Headhunter / Tyrant's Crown, the script table HookOneScript
// installs into, the unique repository's drop rates and the pool. No game process, character,
// installed DLL or release asset is touched.
//
// The model follows the static reading (docs/angelic-roll-hook-research.md, Sessions 2 and 3):
// the roll returns undefined on a hit exactly as on a miss; only a hit calls CreateDefaultParams
// (sub, b, 1.0), by a direct call that reaches ForgePact only through a hook on that function;
// the picked entry's type, not the parameters', is what the roll passes on to the placement.
#define FORGEPACT_RELEASE
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_UNSET, VALUE_ARRAY };
struct CInstance;
// An array and a struct are references, as in GameMaker: a copy of the RValue shares them.
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    CInstance* instance = nullptr;
    std::string text;
    std::shared_ptr<std::vector<RValue>> array;
    std::shared_ptr<std::map<std::string, RValue>> fields;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    double ToDouble() const {
        if (m_Kind != VALUE_REAL && m_Kind != VALUE_INT32 && m_Kind != VALUE_INT64 && m_Kind != VALUE_REF) throw std::runtime_error("not a number");
        return number;
    }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};
struct CInstance {
    bool alive = true;
    double x = 0, y = 0;
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};
static RValue makeArray(std::vector<RValue> values) {
    RValue r; r.m_Kind = VALUE_ARRAY; r.array = std::make_shared<std::vector<RValue>>(std::move(values)); return r;
}
static RValue makeStruct(std::map<std::string, RValue> values) {
    RValue r; r.m_Kind = VALUE_OBJECT; r.fields = std::make_shared<std::map<std::string, RValue>>(std::move(values)); return r;
}
using Triple = std::array<double, 3>;
static RValue makeEntry(const Triple& t) { return makeArray({ RValue(t[0]), RValue(t[1]), RValue(t[2]) }); }
static Triple tripleOf(const RValue& entry) {
    if (entry.m_Kind != VALUE_ARRAY || entry.array->size() != 3) return { -1, -1, -1 };
    return { (*entry.array)[0].number, (*entry.array)[1].number, (*entry.array)[2].number };
}

using PVOID = void*;
using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);
static long InterlockedIncrement(volatile long* value) { return ++*const_cast<long*>(value); }
namespace HeroSiege::Scripts {
inline constexpr std::string_view gml_Script_DropItemAngelicChance = "gml_Script_DropItemAngelicChance";
inline constexpr std::string_view gml_Script_CreateDefaultParams = "gml_Script_CreateDefaultParams";
}
namespace HeroSiege::Objects {
enum class GameObject { Loot_Ground_obj = 902, Controller_obj = 984 };
inline constexpr std::string_view GetObjectName(GameObject o) { return o == GameObject::Controller_obj ? "Controller_obj" : "Loot_Ground_obj"; }
}
static constexpr const char* SdkShortScriptName(std::string_view sdkConstant) { return sdkConstant.data() + 11; }

// ---- log ---------------------------------------------------------------------------------
static std::vector<std::string> outLines;
static void Out(const std::string& line) { outLines.push_back(line); }
static int linesStartingWith(const char* prefix) {
    int n = 0;
    for (const auto& l : outLines) if (l.rfind(prefix, 0) == 0) ++n;
    return n;
}
static bool anyLineHas(const char* prefix, const char* text) {
    for (const auto& l : outLines) if (l.rfind(prefix, 0) == 0 && l.find(text) != std::string::npos) return true;
    return false;
}
static int linesEndingWith(const char* prefix, const std::string& tail) {
    int n = 0;
    for (const auto& l : outLines)
        if (l.rfind(prefix, 0) == 0 && l.size() >= tail.size() && l.compare(l.size() - tail.size(), tail.size(), tail) == 0) ++n;
    return n;
}

// ---- the game's data: the Controller_obj list, the unique repository ------------------------
static constexpr double kControllerId = 100001;
static const char* const kModelListName = "uniqueLoot";   // the model's name; the real one is Live 1's
static int controllerCount = 1;
static std::map<std::string, RValue> controllerVars;
static std::vector<Triple> vanilla;                       // the list as the game built it
// The vanilla list: 117 ordinary uniques plus Liquor Holster (8/0/51), Lucifer's Crown (0/0/85)
// and Mask of the Celestial (0/0/86), each once, so every stand-in's n is 1.
static std::vector<Triple> vanillaTriples(bool withLiquorHolster = true) {
    std::vector<Triple> v;
    for (int i = 0; i < 117; ++i) v.push_back({ 3, 1, (double)i });
    if (withLiquorHolster) v.push_back({ 8, 0, 51 });
    v.push_back({ 0, 0, 85 });
    v.push_back({ 0, 0, 86 });
    return v;
}
static void setList(const std::vector<Triple>& triples) {
    std::vector<RValue> entries;
    for (const Triple& t : triples) entries.push_back(makeEntry(t));
    controllerVars[kModelListName] = makeArray(std::move(entries));
    vanilla = triples;
}
static std::vector<RValue>* listVector() {
    auto it = controllerVars.find(kModelListName);
    return it == controllerVars.end() || it->second.m_Kind != VALUE_ARRAY ? nullptr : it->second.array.get();
}
static std::vector<Triple> listTriples() {
    std::vector<Triple> out;
    if (auto* l = listVector()) for (const RValue& e : *l) out.push_back(tripleOf(e));
    return out;
}
static std::vector<Triple> plus(std::vector<Triple> v, std::initializer_list<Triple> extra) { v.insert(v.end(), extra.begin(), extra.end()); return v; }
// droprate.base per (type, sub, b); the crown's default stand-in is the type-0 one with the lowest.
static std::map<Triple, double> repoBase;

// ---- the runtime the plugin calls: builtins and game scripts ---------------------------------------
static int builtinCalls = 0;
static CInstance globalInstance;
static double num(const RValue& v) { return v.ToDouble(); }
static std::vector<RValue>& arr(const RValue& v) {
    if (v.m_Kind != VALUE_ARRAY || !v.array) throw std::runtime_error("not an array");
    return *v.array;
}
static std::map<std::string, RValue>& obj(const RValue& v) {
    if (v.m_Kind != VALUE_OBJECT || !v.fields) throw std::runtime_error("not a struct");
    return *v.fields;
}
struct FakeYytk {
    RValue CallBuiltin(const char* functionName, std::vector<RValue> a) {
        ++builtinCalls;
        const std::string n(functionName);
        if (n == "asset_get_index") return RValue(a[0].text == "Controller_obj" ? 984.0 : a[0].text == "Loot_Ground_obj" ? 902.0 : -1.0);
        if (n == "instance_number") return RValue(num(a[0]) == 984.0 ? (double)controllerCount : 0.0);
        if (n == "instance_find") {
            RValue r(-4.0);   // noone
            if (num(a[0]) == 984.0 && num(a[1]) == 0.0 && controllerCount > 0) { r.m_Kind = VALUE_REF; r.number = kControllerId; }
            return r;
        }
        if (n == "variable_instance_exists")
            return RValue(a[0].number == kControllerId && controllerCount > 0 && controllerVars.count(a[1].text) ? 1.0 : 0.0);
        if (n == "variable_instance_get") {
            if (a[0].number != kControllerId || !controllerVars.count(a[1].text)) return RValue();
            return controllerVars[a[1].text];
        }
        if (n == "array_length") return RValue((double)arr(a[0]).size());
        if (n == "array_get") return arr(a[0]).at((size_t)num(a[1]));
        if (n == "array_set") { arr(a[0]).at((size_t)num(a[1])) = a[2]; return RValue(); }
        if (n == "array_push") { arr(a[0]).push_back(a[1]); return RValue(); }
        if (n == "array_resize") { arr(a[0]).resize((size_t)num(a[1]), RValue(0.0)); return RValue(); }
        if (n == "array_create") return makeArray(std::vector<RValue>((size_t)num(a[0]), a.size() > 1 ? a[1] : RValue(0.0)));
        if (n == "variable_struct_exists") return RValue(obj(a[0]).count(a[1].text) ? 1.0 : 0.0);
        if (n == "variable_struct_get") { auto& f = obj(a[0]); auto it = f.find(a[1].text); return it == f.end() ? RValue() : it->second; }
        if (n == "variable_struct_set") { obj(a[0])[a[1].text] = a[2]; return RValue(); }
        if (n == "json_stringify") {
            std::string s = "{";
            for (const auto& kv : obj(a[0])) { char b[64]; std::snprintf(b, sizeof b, "%g", kv.second.number); s += (s.size() > 1 ? "," : "") + ("\"" + kv.first + "\":") + b; }
            return RValue(s + "}");
        }
        throw std::runtime_error("unmodelled builtin " + n);
    }
    void CallBuiltinEx(RValue& out, const char* functionName, CInstance*, CInstance*, std::vector<RValue> a) { out = CallBuiltin(functionName, std::move(a)); }
    RValue CallGameScript(std::string_view name, const std::vector<RValue>& a) {
        if (name != "gml_Script_GetUniqueRepoStruct") throw std::runtime_error("unmodelled script");
        const auto it = repoBase.find({ num(a[0]), num(a[1]), num(a[2]) });
        if (it == repoBase.end()) return RValue();
        return makeStruct({ { "droprate", makeStruct({ { "base", RValue(it->second) } }) } });
    }
    void GetGlobalInstance(CInstance** out) { *out = &globalInstance; }
};
static FakeYytk fakeYytk;
static FakeYytk* g_Yytk = &fakeYytk;

// ---- what the production functions read ------------------------------------------------------
static std::atomic<bool> g_HhEnabled{ false };
static std::atomic<bool> g_TyEnabled{ false };
// The panel switches: `headhunter force` / `tyrant force` set these, `off` clears them. A forged
// item's auto-arm sets only the enabled flags above. The signature drop follows these alone.
static std::atomic<bool> g_HhForced{ false };
static std::atomic<bool> g_TyForced{ false };
static std::mt19937& TyRng() { static std::mt19937 rng{ 7 }; return rng; }
static double HhReadNumber(const RValue& value, const char* field, double fallback) {
    CInstance* instance = value.instance;
    if (!instance) return fallback;
    const std::string name(field);
    if (name == "x") return instance->x;
    if (name == "y") return instance->y;
    return fallback;
}
// The validated pool BuildAngelicPool leaves: ordinary uniques, the three real stand-in
// candidates, and whatever a scenario adds.
struct AngelicCandidate { int type, sub, b; std::string name; bool angelic; };
static std::vector<AngelicCandidate> g_AngelicPool;
static std::vector<AngelicCandidate> poolExtras;
static int poolBuilds = 0;
static void BuildAngelicPool(bool) {
    ++poolBuilds;
    if (!g_AngelicPool.empty()) return;
    for (int i = 0; i < 40; ++i) g_AngelicPool.push_back({ 3, 1, i, "unique", true });
    g_AngelicPool.push_back({ 8, 0, 51, "Liquor Holster", true });
    g_AngelicPool.push_back({ 0, 0, 85, "Lucifer's Crown", true });
    g_AngelicPool.push_back({ 0, 0, 86, "Mask of the Celestial", true });
    for (const auto& c : poolExtras) g_AngelicPool.push_back(c);
}
// Spawn recorder: SpawnSignatureItem is `sigdrop`'s path; the roll must never reach it.
struct SpawnRecord { int which; double x, y; CInstance* self; };
static std::vector<SpawnRecord> spawns;
static bool SpawnSignatureItem(int which, double x, double y, CInstance* ctx) {
    spawns.push_back({ which, x, y, ctx });
    return true;
}

// ---- globals the production hook and its helpers own -------------------------------------------
static PFUNC_YYGMLScript g_OrigAngChance = nullptr;
static double g_AngelicRateMult = 1.0;
static bool g_InAngelicExtra = false;
static volatile long g_AngRateHits = 0;
static volatile long g_SigGameRolls = 0, g_SigGameHits = 0, g_SigInjected = 0, g_SigOurHits = 0;
static volatile long g_SigBuilt = 0, g_SigBuiltCrown = 0, g_SigBuiltBelt = 0, g_SigAnomalies = 0;
// The beside design's counters (forgepact-74-inject-base), so the negative control compiles.
static volatile long g_SigShareRolls = 0, g_SigFromGame = 0, g_SigFromGameCrown = 0, g_SigFromGameBelt = 0;
static PFUNC_YYGMLScript g_Orig_CreateDefaultParams = nullptr;
static thread_local int g_SigRollDepth = 0;
static thread_local bool g_SigHitSeen = false;
static double g_SigLastSub = -1.0, g_SigLastB = -1.0;
static bool g_SigDetectNative = false;
// The detection's own positive control: every CreateDefaultParams call that reaches the hook,
// and the route its install got ("off" before any install).
static volatile long g_SigCdpCalls = 0;
static const char* g_SigDetectRoute = "off";
// `sigdrop status` (both builds) reads the forced-drop counters too.
static long g_SigDropRolls = 0, g_SigDropHits = 0, g_SigDropFails = 0;
static int g_SigDropForce = -1;
// The list, the injection and the current hit (ModuleMain.cpp's own globals, same names).
static bool g_SigStandInsResolved = false;
static const char* kAngelicListVar = "";
static std::string g_SigListName = kAngelicListVar;
static const int kSigListMinLength = 100;
static bool g_SigListOk = false, g_SigListTried = false, g_SigListRecount = true;
static int g_SigListLen = -1;
static std::string g_SigListWhy;
static double g_SigControllerIdx = -2.0;
static RValue g_SigRollList;
static int g_SigRollBefore = -1, g_SigRollPushed = 0;
static bool g_SigRollItem[2] = { false, false };
static int g_SigInjectDepth = 0;
static int g_SigHitItem = -1, g_SigHitCoin = 0, g_SigHitStandIn = -1;
static bool g_SigHitRewritten = false;
static std::string g_SigHitWhy;
static int g_SigPending[2] = { 0, 0 };
// `angelicprobe hit status` (research build) reads its levers; the harness holds them off.
struct AngelicHitBase {};
static std::vector<AngelicHitBase> g_AngHitBases;
static double g_AngHitChance = -1.0, g_AngHitRate = -1.0, g_AngHitSharePct = -1.0;
static std::string AngelicHitNumber(double v) { char b[48]; std::snprintf(b, sizeof b, "%g", v); return b; }

// PRODUCTION_TYPES

#ifdef HAS_SIGNATURESTANDIN
static SignatureStandIn g_SigStandIn[2];
#endif
#ifdef HAS_SIGNATURENOTEBUILT
static void SignatureNoteBuilt(const std::map<std::string, double>& selector);
#endif

// ---- the game: CreateDefaultParams, the item builder, the script table, DropItemAngelicChance ---
static int gameCdpCalls = 0;
static bool paramsLackJ = false;   // a CreateDefaultParams whose struct has no `j`
static RValue& gameCreateDefaultParams(CInstance*, CInstance*, RValue& result, int argc, RValue** A) {
    ++gameCdpCalls;
    std::map<std::string, RValue> f = { { "w", RValue(1.0) }, { "a", RValue(424242.0) } };
    if (argc > 0 && !paramsLackJ) f["j"] = *A[0];
    if (argc > 1) f["b"] = *A[1];
    if (argc > 2) f["c"] = *A[2];
    result = makeStruct(std::move(f));
    return result;
}
// Where the roll's direct call lands: the game's own function, or a hook installed on it.
static PFUNC_YYGMLScript cdpEntry = gameCreateDefaultParams;

// LootGroundCreate -> CreateItemNew: one item per placement, built from the parameters it was
// handed under the picked entry's type; the Custom Forge hook recognises the two built-in items
// by their selector {t, a, b, c, j} and reports each to the plugin, as TryApplyCustomForge does.
struct BuildRecord { double t, a, b, c, j; int forged; };   // forged: 0 crown, 1 belt, -1 neither
static std::vector<BuildRecord> builds;
static double field(const RValue& params, const char* key) {
    if (params.m_Kind != VALUE_OBJECT || !params.fields) return -1;
    const auto it = params.fields->find(key);
    return it == params.fields->end() ? -1 : it->second.number;
}
static void gameLootGroundCreate(double type, const RValue& params) {
    BuildRecord r{ type, field(params, "a"), field(params, "b"), field(params, "c"), field(params, "j"), -1 };
    const std::map<std::string, double> selectors[2] = {
        { { "t", 0.0 }, { "a", 777001.0 }, { "b", 7.0 }, { "c", 0.0 }, { "j", 0.0 } },
        { { "t", 8.0 }, { "a", 777002.0 }, { "b", 2.0 }, { "c", 0.0 }, { "j", 0.0 } },
    };
    for (int w = 0; w < 2; ++w) {
        const auto& s = selectors[w];
        if (r.t == s.at("t") && r.a == s.at("a") && r.b == s.at("b") && r.c == s.at("c") && r.j == s.at("j")) {
            r.forged = w;
#ifdef HAS_SIGNATURENOTEBUILT
            SignatureNoteBuilt(s);
#endif
        }
    }
    builds.push_back(r);
}
static int countBuilds(int forged) { int n = 0; for (const auto& b : builds) if (b.forged == forged) ++n; return n; }

// How the game's picker chooses among the entries on a hit: the first entry (an ordinary
// unique), the last one (where an injected entry sits), or uniformly among the entries equal to
// one of `pickTriples` (the stand-ins: the vanilla occurrences and ours alike).
enum class Pick { First, Last, Among };
static Pick pickPolicy = Pick::First;
static std::vector<Triple> pickTriples;
static std::mt19937 pickRng{ 11 };
static std::deque<bool> outcomes;          // per original call: true = hit
static int originalCalls = 0;
static bool originalThrows = false;
static bool gamePushesDuringRoll = false;  // the game itself appends to the list mid-roll
static double lastChanceSeen = -1;
static std::vector<std::vector<Triple>> listDuringCall;
static RValue originalReturn;              // what the game's roll hands back: undefined, always
static RValue& gameAngelicChance(CInstance* self, CInstance* other, RValue&, int argc, RValue** A) {
    ++originalCalls;
    lastChanceSeen = (argc > 2 && A && A[2] && A[2]->m_Kind == VALUE_REAL) ? A[2]->number : -1;
    listDuringCall.push_back(listTriples());
    if (originalThrows) throw std::runtime_error("the game's roll threw");
    const bool hit = !outcomes.empty() && outcomes.front();
    if (!outcomes.empty()) outcomes.pop_front();
    if (hit) {   // pick an entry; only a hit builds the params, by a direct call, then places the item
        std::vector<RValue>* list = listVector();
        Triple picked{ 3, 1, 15 };
        if (list && !list->empty()) {
            size_t index = 0;
            if (pickPolicy == Pick::Last) index = list->size() - 1;
            else if (pickPolicy == Pick::Among) {
                std::vector<size_t> candidates;
                for (size_t i = 0; i < list->size(); ++i)
                    if (std::find(pickTriples.begin(), pickTriples.end(), tripleOf((*list)[i])) != pickTriples.end()) candidates.push_back(i);
                if (!candidates.empty()) index = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(pickRng)];
            }
            picked = tripleOf((*list)[index]);
        }
        RValue sub(picked[1]), b(picked[2]), c(1.0), params;
        RValue* args[] = { &sub, &b, &c };
        RValue& made = cdpEntry(self, other, params, 3, args);
        gameLootGroundCreate(picked[0], made);
    }
    if (gamePushesDuringRoll) if (auto* l = listVector()) l->push_back(makeEntry({ 9, 0, 1 }));
    originalReturn = RValue();
    return originalReturn;
}

// HookOneScript into a fake table: one install per name, a second call answers from the table.
// By default every install is an inline detour (`*nativeOut = true`), so the roll's direct call
// reaches the hook. `tableOnlyHook` names one hook whose detour fails, as HookOneScript's
// fallback does: the saved original is the table entry (the game's own code) and a direct call
// still lands on the game's function. `missingHook` names one the runtime cannot resolve.
static std::map<std::string, int> installs;
static std::string tableOnlyHook, missingHook;
static std::vector<const void*> gameImage;   // saved originals that are the game's own code
static bool HookOneScript(const char* shortName, const char* /*id*/, PVOID dest, PFUNC_YYGMLScript* origOut, bool* nativeOut = nullptr) {
    const std::string name(shortName);
    if (nativeOut) *nativeOut = false;
    if (name == missingHook) return false;
    ++installs[name];
    const bool native = name != tableOnlyHook;
    if (nativeOut) *nativeOut = native;
    if (name == "CreateDefaultParams") {
        *origOut = gameCreateDefaultParams;
        if (native) cdpEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest);
        else gameImage.push_back(reinterpret_cast<const void*>(gameCreateDefaultParams));
        return true;
    }
    if (name == "DropItemAngelicChance") {
        *origOut = gameAngelicChance;
        if (!native) gameImage.push_back(reinterpret_cast<const void*>(gameAngelicChance));
        return true;
    }
    return false;
}
// The production test for "this saved original is the game's own code" (HookOneScript's
// table-only fallback), answered from what the fake above recorded.
using HMODULE = void*;
static HMODULE GetModuleHandleA(const char*) { return nullptr; }
static bool AddrIsExecutableInModule(HMODULE, const void* address) {
    for (const void* p : gameImage) if (p == address) return true;
    return false;
}

// PRODUCTION_FUNCTIONS

static void reset() {
    outLines.clear();
    g_HhEnabled = false; g_TyEnabled = false; g_HhForced = false; g_TyForced = false;
    g_AngelicPool.clear(); poolExtras.clear(); poolBuilds = 0;
    spawns.clear(); builds.clear(); builtinCalls = 0;
    controllerCount = 1; controllerVars.clear(); setList(vanillaTriples());
    repoBase = { { { 8, 0, 51 }, 5000000.0 }, { { 0, 0, 85 }, 111111111.0 }, { { 0, 0, 86 }, 30000000.0 } };
    gameCdpCalls = 0; cdpEntry = gameCreateDefaultParams; paramsLackJ = false;
    pickPolicy = Pick::First; pickTriples.clear();
    outcomes.clear(); originalCalls = 0; originalThrows = false; gamePushesDuringRoll = false; lastChanceSeen = -1;
    listDuringCall.clear();
    installs.clear(); tableOnlyHook.clear(); missingHook.clear(); gameImage.clear();
    g_SigDetectNative = false;
    g_OrigAngChance = gameAngelicChance;   // as if `raredrop angelic` (or the switch) had installed it, detoured
    g_AngelicRateMult = 1.0; g_InAngelicExtra = false; g_AngRateHits = 0;
    g_SigGameRolls = 0; g_SigGameHits = 0; g_SigInjected = 0; g_SigOurHits = 0;
    g_SigBuilt = 0; g_SigBuiltCrown = 0; g_SigBuiltBelt = 0; g_SigAnomalies = 0;
    g_SigShareRolls = 0; g_SigFromGame = 0; g_SigFromGameCrown = 0; g_SigFromGameBelt = 0;
    g_Orig_CreateDefaultParams = nullptr; g_SigRollDepth = 0; g_SigHitSeen = false;
    g_SigLastSub = -1.0; g_SigLastB = -1.0;
    g_SigCdpCalls = 0; g_SigDetectRoute = "off";
    // The list's name as Live 1 (or the research build's inject lever) gives it.
    g_SigStandInsResolved = false; g_SigListName = kModelListName;
    g_SigListOk = false; g_SigListTried = false; g_SigListRecount = true; g_SigListLen = -1; g_SigListWhy.clear();
    g_SigControllerIdx = -2.0; g_SigRollList = RValue(); g_SigRollBefore = -1; g_SigRollPushed = 0;
    g_SigRollItem[0] = g_SigRollItem[1] = false; g_SigInjectDepth = 0;
    g_SigHitItem = -1; g_SigHitCoin = 0; g_SigHitStandIn = -1; g_SigHitRewritten = false; g_SigHitWhy.clear();
    g_SigPending[0] = g_SigPending[1] = 0;
#ifdef HAS_SIGNATURESTANDIN
    g_SigStandIn[0] = SignatureStandIn{}; g_SigStandIn[1] = SignatureStandIn{};
#endif
}
static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
// What a switch turning on does: install the detection. Absent from a source without it.
static void installDetection() {
#ifdef HAS_INSTALLSIGNATUREANGELICHOOKS
    InstallSignatureAngelicHooks();
#endif
}
static bool switchOn(int which) {
#ifdef HAS_SIGNATURESWITCHON
    return SignatureSwitchOn(which);
#else
    (void)which;
    return false;
#endif
}
// One call of the game's roll as DropItem makes it: x, y, the chance, an undefined argument 3.
static RValue* roll(CInstance& monster, std::initializer_list<bool> perOriginalCall, double x = 640.5, double y = 320.25) {
    outcomes.assign(perOriginalCall.begin(), perOriginalCall.end());
    static RValue a0, a1, a2, a3, result;
    a0 = RValue(x);
    a1 = RValue(y);
    a2 = RValue(1500.0);
    a3 = RValue();
    RValue* args[] = { &a0, &a1, &a2, &a3 };
    return &HookAngelicChance(&monster, nullptr, result, 4, args);
}
static std::string describe(const std::vector<Triple>& v) {
    std::string s = std::to_string(v.size()) + " entries, last ";
    if (!v.empty()) { char b[64]; std::snprintf(b, sizeof b, "[%g,%g,%g]", v.back()[0], v.back()[1], v.back()[2]); s += b; }
    return s;
}
static std::string lastLine(const char* prefix) {
    for (auto it = outLines.rbegin(); it != outLines.rend(); ++it) if (it->rfind(prefix, 0) == 0) return *it;
    return std::string();
}
static std::string sigdropStatusLine() {
#ifdef HAS_SIGDROPSTATUS
    SigDropStatus();
    return lastLine("sigdrop:");
#else
    return std::string();
#endif
}
// Both status lines (`sigdrop status`, `angelicprobe hit status`) end with the detection's own
// positive control, ` cdpCalls=<n> detect=<route>`, which the live procedure reads before
// `gameHits=`. Absent from a source without either status line or without the tokens.
static void requireStatusTail(const std::string& tail, const std::string& when) {
    auto endsWith = [&](const std::string& line) {
        return line.size() >= tail.size() && line.compare(line.size() - tail.size(), tail.size(), tail) == 0;
    };
#if defined(HAS_SIGDROPSTATUS) && defined(HAS_ANGELICHITSTATUS)
    SigDropStatus();
    const std::string sig = lastLine("sigdrop:");
    require(endsWith(sig), when + ": `sigdrop status` does not end with `" + tail + "`: " + sig);
    AngelicHitStatus();
    const std::string hit = lastLine("angelicprobe hit:");
    require(endsWith(hit), when + ": `angelicprobe hit status` does not end with `" + tail + "`: " + hit);
#else
    require(false, when + ": a status line carrying the detection route is missing");
#endif
}

static const Triple kBeltStandIn{ 8, 0, 51 };    // Liquor Holster
static const Triple kCrownStandIn{ 0, 0, 86 };   // Mask of the Celestial: the model's lowest type-0 droprate.base

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    CInstance monster;
    monster.x = 10; monster.y = 20;
    reset();
    const std::string test = argv[1];
    try {
        // ---- baseline: holds against forgepact-74-inject-base (the beside design) too ----
        if (test == "both_off_miss_passthrough") {
            RValue* r = roll(monster, { false });
            require(originalCalls == 1, "the game's roll was not called exactly once");
            require(lastChanceSeen == 1500.0, "argument 2 (the chance) reached the game changed");
            require(r == &originalReturn, "the hook did not hand back the game's own return value");
            require(r->m_Kind == VALUE_UNDEFINED, "the return value was not the game's undefined");
            require(listDuringCall.size() == 1 && listDuringCall[0] == vanilla, "the roll saw a list that is not the game's own");
            require(builtinCalls == 0, "with both switches off and nothing installed the plugin called the runtime");
            require(spawns.empty() && builds.empty(), "a miss with both switches off placed something");
        } else if (test == "both_off_hit_passthrough") {
            installDetection();   // e.g. a research lever: detection without a switch
            roll(monster, { true });
            require(originalCalls == 1 && gameCdpCalls == 1, "the game's roll or its CreateDefaultParams did not run once");
            require(listDuringCall.size() == 1 && listDuringCall[0] == vanilla, "with both switches off the roll saw a changed list");
            require(builds.size() == 1 && builds[0].forged == -1 && builds[0].c == 1.0, "the game did not build its own unique, once");
            require(spawns.empty(), "a hit with both switches off spawned a signature item");
            require(g_SigOurHits == 0 && g_SigBuilt == 0 && g_SigFromGame == 0, "a hit with both switches off counted a signature item");
            require(listTriples() == vanilla, "the list is not the game's own after the roll");
        } else if (test == "default_params_outside_roll_not_a_hit") {
#ifdef HAS_HOOK_CREATEDEFAULTPARAMS
            g_HhEnabled = true; g_HhForced = true; g_TyEnabled = true; g_TyForced = true;
            installDetection();
            RValue sub(1.0), b(15.0), c(1.0), params;
            RValue* args[] = { &sub, &b, &c };
            cdpEntry(&monster, nullptr, params, 3, args);   // DropItem building an ordinary drop
            require(gameCdpCalls == 1, "CreateDefaultParams outside the roll did not reach the game's own function");
            require(field(params, "b") == 15.0 && field(params, "c") == 1.0, "CreateDefaultParams outside the roll was changed");
            roll(monster, { false });
            require(g_SigGameHits == 0, "a CreateDefaultParams call outside the roll counted as a hit");
            require(spawns.empty() && g_SigOurHits == 0, "a CreateDefaultParams call outside the roll was taken for a signature item");
#endif
        } else if (test == "extra_rolls_still_run") {
            g_AngelicRateMult = 3.0;
            roll(monster, { false, false, false });
            require(originalCalls == 3, "rate x3 did not run the game's roll three times");
            require(spawns.empty() && builds.empty(), "misses placed something");
        // ---- target: every scenario fails against forgepact-74-inject-base ----
        } else if (test == "switch_on_injects_for_the_call") {
            g_HhForced = true;
            installDetection();
            roll(monster, { false });
            require(listDuringCall.size() == 1, "the game's roll did not run once");
            require(listDuringCall[0] == plus(vanilla, { kBeltStandIn }),
                    "while the roll ran, the list was not the game's own plus Liquor Holster once: " + describe(listDuringCall[0]));
            require(listTriples() == vanilla, "after the roll the list is not the game's own again: " + describe(listTriples()));
            require(g_SigInjected == 1, "injected= did not count the one entry");
            require(anyLineHas("signature drops:", "Headhunter:Liquor Holster(n=1)"), "the switch-on did not name Liquor Holster and its count");
        } else if (test == "original_throw_removes_entries") {
            g_HhForced = true;
            installDetection();
            originalThrows = true;
            bool threw = false;
            try { roll(monster, { true }); } catch (const std::runtime_error&) { threw = true; }
            require(threw, "the game's exception did not reach the caller");
            require(listDuringCall.size() == 1 && listDuringCall[0].size() == vanilla.size() + 1, "the entry was not on the list while the roll ran");
            require(listTriples() == vanilla, "after a throw the injected entry stayed on the list: " + describe(listTriples()));
            require(g_SigRollDepth == 0 && g_SigInjectDepth == 0, "the roll-in-progress state stayed raised after a throw");
            originalThrows = false;
            RValue sub(0.0), b(51.0), c(1.0), params;
            RValue* args[] = { &sub, &b, &c };
            cdpEntry(&monster, nullptr, params, 3, args);   // after the throw, outside any roll
            require(!g_SigHitSeen && g_SigOurHits == 0, "a CreateDefaultParams call after a throw, outside any roll, was taken for a hit");
            setList(vanillaTriples(false));   // n = 0: every hit on the stand-in is ours
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(g_SigOurHits == 1 && countBuilds(1) == 1, "after a throw, our next hit was not built");
            require(listTriples() == vanilla, "the list is not the game's own after the next roll");
        } else if (test == "standin_hit_is_ours_one_entry_share") {
            // n = 1: the picker sees Liquor Holster twice, once ours; ours half the time.
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 2000; ++i) roll(monster, { true });
            require(g_SigGameHits == 2000 && builds.size() == 2000, "not one item built per hit");
            require(g_SigOurHits >= 850 && g_SigOurHits <= 1150, "a Liquor Holster hit was not ours about 1 in 2: " + std::to_string(g_SigOurHits));
            require(countBuilds(1) == (int)g_SigOurHits && countBuilds(0) == 0, "the game did not build a Headhunter for every our-hit");
            require(g_SigBuilt == g_SigOurHits && g_SigBuiltBelt == g_SigOurHits && g_SigBuiltCrown == 0, "built=/belt= do not match what the game built");
            int liquor = 0;
            for (const auto& b : builds) if (b.t == 8 && b.b == 51 && b.c == 1.0) ++liquor;
            require(liquor == 2000 - (int)g_SigOurHits, "a hit that was not ours did not build the game's own Liquor Holster");
            require(linesStartingWith("angelic hit:") == 2000, "not one `angelic hit:` line per hit");
            require(linesEndingWith("angelic hit:", "Headhunter (stand-in Liquor Holster, 1 in 2) built by the game") == (int)g_SigOurHits,
                    "an our-hit line does not say the game built it");
            require(spawns.empty(), "the roll path spawned an item of its own");
        } else if (test == "our_hit_rewrites_and_the_game_builds_once") {
            setList(vanillaTriples(false));   // n = 0: the injected entry is the only Liquor Holster
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(g_SigOurHits == 1, "the hit on our entry was not ours");
            require(builds.size() == 1, "the game did not build exactly one item for one hit");
            const BuildRecord b = builds[0];
            require(b.t == 8 && b.a == 777002 && b.b == 2 && b.c == 0 && b.j == 0,
                    "the game was not handed Headhunter's own definition {t 8, a 777002, b 2, c 0, j 0}");
            require(b.forged == 1 && g_SigBuilt == 1 && g_SigBuiltBelt == 1, "the Custom Forge hook did not recognise the game-built Headhunter");
            require(spawns.empty(), "the roll path spawned an item of its own");
            require(anyLineHas("angelic hit:", "-> Headhunter (stand-in Liquor Holster, 1 in 1) built by the game"), "the hit line does not say what happened");
        } else if (test == "roll_path_never_spawns") {
            g_HhForced = true; g_TyForced = true;
            installDetection();
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn, kCrownStandIn };
            for (int i = 0; i < 400; ++i) roll(monster, { true });
            require(g_SigOurHits > 0, "no hit fell to a mod item");
            require(spawns.empty(), "the roll path spawned " + std::to_string(spawns.size()) + " item(s) of its own");
            require(builds.size() == 400, "not one item per hit");
        } else if (test == "off_after_on_pushes_nothing") {
            g_HhForced = true;
            installDetection();
            for (int i = 0; i < 10; ++i) roll(monster, { false });
            require(g_SigInjected == 10, "switched on, ten rolls did not push ten entries");
            const int installsBefore = installs["CreateDefaultParams"];
            g_HhForced = false;   // `headhunter off`
            listDuringCall.clear();
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 100; ++i) roll(monster, { true });
            for (const auto& seen : listDuringCall) require(seen == vanilla, "switched off, a roll saw an injected entry");
            require(g_SigInjected == 10 && g_SigOurHits == 0, "switched off, an entry was pushed or a hit taken");
            require(countBuilds(1) == 0 && listTriples() == vanilla, "switched off, a Headhunter was built or the list changed");
            require(installs["CreateDefaultParams"] == installsBefore && cdpEntry != gameCreateDefaultParams, "switching off removed or reinstalled a hook");
        } else if (test == "both_on_pushes_two_and_builds_both") {
            g_HhForced = true; g_TyForced = true;
            installDetection();
            require(anyLineHas("signature drops:", "Tyrant's Crown:Mask of the Celestial(n=1)"),
                    "the switch-on did not name the crown's stand-in (the lowest type-0 droprate.base)");
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kCrownStandIn, kBeltStandIn }), "both on, the roll did not see both stand-ins: " + describe(listDuringCall[0]));
            require(listTriples() == vanilla && g_SigInjected == 2, "both on, the list was not restored or the pushes not counted");
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn, kCrownStandIn };
            for (int i = 0; i < 2000; ++i) roll(monster, { true });
            require(countBuilds(0) >= 400 && countBuilds(1) >= 400, "both on, the game did not build both items: crowns "
                    + std::to_string(countBuilds(0)) + ", belts " + std::to_string(countBuilds(1)));
            require(g_SigBuiltCrown == countBuilds(0) && g_SigBuiltBelt == countBuilds(1), "crown=/belt= do not match what the game built");
            for (const auto& b : builds) if (b.forged == 0) require(b.t == 0 && b.a == 777001 && b.b == 7, "a crown was built from the wrong definition");
        } else if (test == "list_refusals") {
            // The player build before Live 1 (no name), a name the instance lacks, a wrong shape, a
            // short list, no Controller_obj: no push, the gate off, one refusal line each.
            struct Case { const char* name; void (*arrange)(); const char* why; };
            const Case cases[] = {
                { "no name", [] { g_SigListName = ""; }, "no list variable named yet" },
                { "absent", [] { g_SigListName = "notTheList"; }, "has no variable notTheList" },
                { "shape", [] { auto v = vanillaTriples(); setList(v); (*listVector())[5] = makeArray({ RValue(1.0), RValue(2.0) }); }, "entry 5 is not three numbers" },
                { "short", [] { const auto all = vanillaTriples(); setList(std::vector<Triple>(all.end() - 50, all.end())); }, "fewer than 100" },
                { "no controller", [] { controllerCount = 0; }, "no live Controller_obj instance" },
            };
            for (const Case& c : cases) {
                reset();
                c.arrange();
                const auto before = listTriples();
                g_HhForced = true;
                installDetection();
                for (int i = 0; i < 5; ++i) roll(monster, { false });
                require(anyLineHas("signature drops: list missing", c.why), std::string(c.name) + ": the switch-on did not refuse naming why");
                require(linesStartingWith("signature drops: list missing") == 1, std::string(c.name) + ": not exactly one refusal line");
                require(g_SigInjected == 0 && listTriples() == before, std::string(c.name) + ": an entry was pushed onto a list that did not resolve");
                require(!switchOn(1), std::string(c.name) + ": the gate armed without the list");
                require(sigdropStatusLine().find(" list=missing gate=tyrant:off,headhunter:off ") != std::string::npos,
                        std::string(c.name) + ": `sigdrop status` does not say list=missing with the gate off");
            }
        } else if (test == "list_changed_during_roll_left_as_found") {
            g_HhForced = true;
            installDetection();
            gamePushesDuringRoll = true;
            roll(monster, { false });
            require(listTriples() == plus(vanilla, { kBeltStandIn, Triple{ 9, 0, 1 } }), "a list the game changed mid-roll was cut: " + describe(listTriples()));
            require(linesStartingWith("inject: list changed during the roll, left as found") == 1, "no anomaly line");
            require(g_SigAnomalies == 1, "anomalies= did not count it");
        } else if (test == "extra_rolls_carry_the_entries") {
            g_HhForced = true; g_AngelicRateMult = 3.0;
            installDetection();
            roll(monster, { false, false, false });
            require(originalCalls == 3 && g_SigGameRolls == 3, "rate x3 did not run and count the game's roll three times");
            for (const auto& seen : listDuringCall) require(seen == plus(vanilla, { kBeltStandIn }), "an extra roll ran without the entry");
            require(g_SigInjected == 1 && listTriples() == vanilla, "the entry was not pushed once and removed after the last extra roll");
            setList(vanillaTriples(false));
            pickPolicy = Pick::Last;
            roll(monster, { false, true, false });
            require(g_SigOurHits == 1 && countBuilds(1) == 1, "a hit in an extra roll was not ours and built");
        } else if (test == "ambiguous_standin_never_arms") {
            // Another validated unique of a different type with Liquor Holster's sub/b: its hits
            // would read as the stand-in's, so Headhunter refuses to arm.
            poolExtras = { { 10, 0, 51, "Supreme Elemelon", true } };
            g_HhForced = true;
            installDetection();
            require(anyLineHas("signature drops:", "not validated: ambiguous: Supreme Elemelon"), "the ambiguous stand-in was not refused by name");
            require(!switchOn(1), "the gate armed on an ambiguous stand-in");
            roll(monster, { false });
            require(g_SigInjected == 0 && listDuringCall[0] == vanilla, "an ambiguous stand-in was injected");
        } else if (test == "missing_field_refuses_and_leaves_vanilla") {
            setList(vanillaTriples(false));
            paramsLackJ = true;
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(g_SigOurHits == 1, "the hit was not ours");
            require(anyLineHas("inject: Headhunter refused", "no field j"), "the refusal does not name the missing field");
            require(anyLineHas("inject: Headhunter refused", "\"b\":51"), "the refusal does not carry the struct's JSON");
            require(builds.size() == 1 && builds[0].b == 51 && builds[0].c == 1.0 && builds[0].a == 424242, "a refused rewrite did not leave the parameters vanilla");
            require(g_SigBuilt == 0 && spawns.empty(), "a refused rewrite counted or spawned an item");
            require(anyLineHas("angelic hit:", "refused (no field j)"), "the hit line does not say it was refused");
        } else if (test == "sigdrop_status_tokens") {
            const std::string line = sigdropStatusLine();
            const std::string head = "sigdrop: force off | rolls=0 drops=0 fails=0 | game roll: gameRolls=0 gameHits=0 injected=0 ourHits=0 built=0 crown=0 belt=0 list=none gate=tyrant:off,headhunter:off";
            require(line.rfind(head, 0) == 0, "`sigdrop status` does not begin as the live procedure reads it: " + line);
            require(line.size() >= 22 && line.compare(line.size() - 22, 22, " cdpCalls=0 detect=off") == 0, "`sigdrop status` does not end with the detection route: " + line);
            g_HhForced = true;
            installDetection();
            require(sigdropStatusLine().find(" list=uniqueLoot:120 gate=tyrant:off,headhunter:on ") != std::string::npos, "`list=` does not name the resolved list");
        // ---- detection: the beside design's install and gate, kept ----
        } else if (test == "install_is_idempotent") {
            g_OrigAngChance = nullptr;
#ifdef HAS_INSTALLSIGNATUREANGELICHOOKS
            InstallSignatureAngelicHooks();
            InstallSignatureAngelicHooks();
#endif
            require(installs["CreateDefaultParams"] == 1, "CreateDefaultParams was not hooked exactly once");
            require(installs["DropItemAngelicChance"] == 1, "DropItemAngelicChance was not hooked exactly once");
            require(g_OrigAngChance == gameAngelicChance, "the roll hook's saved original is not the game's roll");
            reset();
            installDetection();
            require(installs["DropItemAngelicChance"] == 0, "an already-held roll hook was installed again");
            RValue* r = roll(monster, { true });
            require(r == &originalReturn, "the return value is not the game's own");
        } else if (test == "detection_not_detoured_never_arms") {
            // A hit is visible only through inline detours on both hooks (the roll's call to
            // CreateDefaultParams is direct). A detection that got less must say so and leave
            // the gate off, rather than report the switch armed while it never drops anything.
            // Positive control, through the same fake: both detoured, the gate arms.
            g_HhEnabled = true; g_HhForced = true; g_TyEnabled = true; g_TyForced = true;
            g_OrigAngChance = nullptr;
            installDetection();
            require(anyLineHas("signature drops:", "detection ON"), "a detoured detection did not log `detection ON`");
            require(switchOn(0) && switchOn(1), "a detoured detection with the list resolved did not arm the gate");
            // 1. CreateDefaultParams gets only the table route: the roll's direct call bypasses it.
            reset();
            g_HhEnabled = true; g_HhForced = true; g_TyEnabled = true; g_TyForced = true;
            tableOnlyHook = "CreateDefaultParams";
            installDetection();
            require(anyLineHas("signature drops: game-roll detection NOT installed", "CreateDefaultParams TABLE-ONLY"),
                    "a table-only CreateDefaultParams did not log the detection NOT installed");
            require(!anyLineHas("signature drops:", "detection ON"), "a table-only CreateDefaultParams logged `detection ON`");
            require(!switchOn(0) && !switchOn(1), "a table-only CreateDefaultParams armed the gate");
            for (int i = 0; i < 100; ++i) roll(monster, { true });
            require(gameCdpCalls == 100 && g_SigGameHits == 0 && spawns.empty(), "a table-only CreateDefaultParams saw a direct call");
            require(g_SigInjected == 0, "a table-only detection pushed entries it could never attribute");
            installDetection();   // switching on again does not turn the table route into a detour
            require(linesStartingWith("signature drops: game-roll detection NOT installed") == 2, "a second switch-on did not report the same route");
            require(installs["CreateDefaultParams"] == 1, "a second switch-on hooked CreateDefaultParams again");
            // 2. The roll hook, first installed here, falls back to the table.
            reset();
            g_HhEnabled = true; g_HhForced = true;
            g_OrigAngChance = nullptr;
            tableOnlyHook = "DropItemAngelicChance";
            installDetection();
            require(anyLineHas("signature drops: game-roll detection NOT installed", "DropItemAngelicChance TABLE-ONLY"),
                    "a table-only roll hook did not log the detection NOT installed");
            require(!switchOn(1), "a table-only roll hook armed the gate");
            // 3. The roll hook was already held table-only (`raredrop angelic` earlier this session).
            reset();
            g_TyEnabled = true; g_TyForced = true;
            gameImage.push_back(reinterpret_cast<const void*>(gameAngelicChance));
            installDetection();
            require(anyLineHas("signature drops: game-roll detection NOT installed", "DropItemAngelicChance TABLE-ONLY"),
                    "an earlier table-only roll hook was reported as detection installed");
            require(!switchOn(0), "an earlier table-only roll hook armed the gate");
            // 4. The runtime cannot resolve the roll by name.
            reset();
            g_HhEnabled = true; g_HhForced = true;
            g_OrigAngChance = nullptr;
            missingHook = "DropItemAngelicChance";
            installDetection();
            require(anyLineHas("signature drops: game-roll detection NOT installed", "DropItemAngelicChance not found"),
                    "an unresolved roll hook did not log the detection NOT installed");
            require(!switchOn(1), "an unresolved roll hook armed the gate");
        } else if (test == "status_reports_detect_route") {
            // The live procedure reads `detect=` and `cdpCalls=` before it trusts `gameHits=0`:
            // cdpCalls counts every CreateDefaultParams call that reaches the hook, a roll in
            // progress or not, so an ordinary drop proves the hook is reachable.
            RValue sub(1.0), b(15.0), c(1.0), params;
            RValue* args[] = { &sub, &b, &c };
            // Off: nothing installed yet, and a call lands on the game's own function.
            cdpEntry(&monster, nullptr, params, 3, args);
            requireStatusTail(" cdpCalls=0 detect=off", "before any install");
            // Detoured: the non-roll call (DropItem building an ordinary drop) reaches the hook
            // and counts, without counting as a hit; a hit in the roll counts as well.
            reset();
            g_OrigAngChance = nullptr;
            installDetection();
            cdpEntry(&monster, nullptr, params, 3, args);
            require(gameCdpCalls == 1 && g_SigGameHits == 0, "a non-roll CreateDefaultParams call did not pass through, or counted as a hit");
            requireStatusTail(" cdpCalls=1 detect=detoured", "detoured, one non-roll call");
            roll(monster, { true });
            requireStatusTail(" cdpCalls=2 detect=detoured", "detoured, then one roll hit");
            // TABLE-ONLY: the direct calls bypass the hook, so cdpCalls stays 0 while the game's
            // own function runs - the instrument-blind reading the live procedure stops on.
            reset();
            tableOnlyHook = "CreateDefaultParams";
            installDetection();
            cdpEntry(&monster, nullptr, params, 3, args);
            roll(monster, { true });
            require(gameCdpCalls == 2, "the game's own CreateDefaultParams did not run");
            requireStatusTail(" cdpCalls=0 detect=TABLE-ONLY", "table-only");
            // The runtime cannot resolve CreateDefaultParams by name.
            reset();
            missingHook = "CreateDefaultParams";
            installDetection();
            requireStatusTail(" cdpCalls=0 detect=not found", "not resolved");
        } else if (test == "autoarm_enabled_not_forced_no_drop") {
            // A forged Headhunter / Tyrant's Crown arms its mechanic at every launch: the auto-arm
            // sets the enabled flags while the panel switch stays off. The owner's decision of
            // 2026-10-02: the drop follows the panel switch only, so nothing joins the list here.
            // Positive control in the same scenario: the detection is detoured and sees every hit.
            g_HhEnabled = true; g_TyEnabled = true;   // as HeadhunterAutoArm / TyrantAutoArm leave them
            installDetection();   // installed anyway (a research lever, or an earlier `force` this session)
            require(anyLineHas("signature drops:", "detection ON"), "the detection did not install detoured");
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn, kCrownStandIn };
            for (int i = 0; i < 300; ++i) roll(monster, { true });
            require(g_SigGameHits == 300, "hits were not detected: gameHits=" + std::to_string(g_SigGameHits));
            for (const auto& seen : listDuringCall) require(seen == vanilla, "an auto-armed item with its panel switch off joined the list");
            require(spawns.empty() && g_SigOurHits == 0 && g_SigFromGame == 0 && countBuilds(0) + countBuilds(1) == 0,
                    "an auto-armed item with its panel switch off dropped from the game's roll");
            require(!switchOn(0) && !switchOn(1), "the gate read the enabled state, not the panel switch");
#ifdef HAS_SIGDROPSTATUS
            require(sigdropStatusLine().find(" gate=tyrant:off,headhunter:off ") != std::string::npos, "`sigdrop status` does not report the gate off for auto-armed items");
#else
            require(false, "no `sigdrop status` line reports the gate");
#endif
        } else return 2;
        std::cout << "PASS " << test << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << test << ": " << error.what() << '\n';
        return 1;
    }
}
