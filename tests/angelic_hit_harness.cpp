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
// the roll picks an entry and reads its definition through GetUniqueRepoStruct(type, sub, b)
// before its die (a switch skips that read, or reads another entry last); it returns undefined
// on a hit exactly as on a miss; only a hit calls CreateDefaultParams (sub, b, 1.0), by a direct
// call that reaches ForgePact only through a hook on that function; the picked entry's type, not
// the parameters', is what the roll passes on to the placement. The runtime may hand back the
// Controller_obj instance as VALUE_REF or VALUE_OBJECT, and a switch makes variable_instance_get
// return a copy of the list, so a push onto what it returned is not visible on a fresh read.
//
// The list's layout is replan 2's static reading (Session 4): the Controller_obj variable is an
// array whose element 5 is a ds_list (the model holds a ds_list at every element; what elements
// 0-4 hold is not established), the roll reads element 5 - ds_list_size, an index drawn up to it,
// ds_list_find_value, and a fresh draw unless is_array - and each entry of that ds_list is an
// array [type, sub, b]. The ds_lists live in a model store (id -> entries) that the stub runtime's
// ds_exists / ds_list_size / ds_list_find_value / ds_list_add / ds_list_delete read and write.
//
// The build is Live 2's measurement and Session 5's static reading: CreateDefaultParams returns
// {j, b, c} only - no field a and no w (Live 2: {"b":51.0,"j":0.0,"c":1.0}); LootGroundCreate
// stores a value of its own into the record's a, unconditionally, hands CreateItemNew an item
// instance whose itemDefinitionStruct is that same record and whose itemType is the placement's
// type, and CreateItemNew reads a, b, c, j from the record and stores none of them. Its call is
// direct, so only a detoured hook on CreateItemNew (the Custom Forge's, by default here, as in
// Live 2) sees its entry - the rewrite point - and its return, where the forge recognises a
// built Headhunter / Tyrant's Crown.
#define FORGEPACT_RELEASE
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_UNSET, VALUE_ARRAY,
       VALUE_BOOL, VALUE_PTR, VALUE_NULL };
struct CInstance;
// Each conversion ToDouble refuses, counted the way the runner counts a raised error (the stand-in
// for `[hs] YYError summary: total=`): on this runner the error is reported even when a C++ catch
// then swallows the failure, so a gate that refuses a kind must do it before converting (#74).
static long refusedConversions = 0;
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
        if (m_Kind != VALUE_REAL && m_Kind != VALUE_INT32 && m_Kind != VALUE_INT64 && m_Kind != VALUE_REF) {
            ++refusedConversions;
            throw std::runtime_error("not a number");
        }
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
// The clock the pool build's retry throttle reads; a scenario moves it by hand.
static unsigned long long fakeNowMs = 1000;
static unsigned long long GetTickCount64() { return fakeNowMs; }
namespace HeroSiege::Scripts {
inline constexpr std::string_view gml_Script_DropItemAngelicChance = "gml_Script_DropItemAngelicChance";
inline constexpr std::string_view gml_Script_CreateDefaultParams = "gml_Script_CreateDefaultParams";
inline constexpr std::string_view gml_Script_GetUniqueRepoStruct = "gml_Script_GetUniqueRepoStruct";
inline constexpr std::string_view gml_Script_CreateItemNew = "gml_Script_CreateItemNew";
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
static const char* const kModelListName = "uniqueLoot";   // the model's name; the real one is lootListUnique
static constexpr size_t kModelIndex = 5;                   // the element the roll reads (static reading)
static int controllerCount = 1;
static std::map<std::string, RValue> controllerVars;
// How instance_find hands back the Controller_obj instance: a VALUE_REF (this runner's usual
// answer) or a VALUE_OBJECT; either one must reach the same variables.
static int controllerKind = VALUE_REF;
static CInstance controllerInstance;
static bool isController(const RValue& v) {
    if (controllerCount < 1) return false;
    if (v.m_Kind == VALUE_REF) return v.number == kControllerId;
    return v.m_Kind == VALUE_OBJECT && v.instance == &controllerInstance;
}
// The runtime's ds_list store: id -> entries. Ids are handed out from 40 up, so a small number
// such as 7 is never a live list. `dsIdKind` is how the outer array holds each id: a number, or
// a reference (a live data-structure handle may arrive as VALUE_REF on current runners).
static std::map<double, std::vector<RValue>> dsLists;
static double nextDsId = 40;
static int dsIdKind = VALUE_REAL;
static RValue dsHandle(double id) { RValue r(id); r.m_Kind = dsIdKind; return r; }
static double dsCreate(std::vector<RValue> entries) { const double id = nextDsId++; dsLists[id] = std::move(entries); return id; }
static std::vector<RValue>* dsFind(const RValue& v) {
    if (v.m_Kind != VALUE_REAL && v.m_Kind != VALUE_INT32 && v.m_Kind != VALUE_INT64 && v.m_Kind != VALUE_REF) return nullptr;
    const auto it = dsLists.find(v.number);
    return it == dsLists.end() ? nullptr : &it->second;
}
static std::vector<RValue>& dsList(const RValue& v) {
    auto* l = dsFind(v);
    if (!l) throw std::runtime_error("not a live ds_list");
    return *l;
}
static std::vector<RValue> entriesOf(const std::vector<Triple>& triples) {
    std::vector<RValue> entries;
    for (const Triple& t : triples) entries.push_back(makeEntry(t));
    return entries;
}
static std::vector<Triple> triplesOf(const std::vector<RValue>& entries) {
    std::vector<Triple> out;
    for (const RValue& e : entries) out.push_back(tripleOf(e));
    return out;
}
// variable_instance_get hands back a copy of the variable instead of the variable itself, and
// the copy holds another ds_list (a fresh id, the same entries) at the roll's index: a push onto
// what it returned never reaches the list the roll reads. `copiedIds` are those lists, in order.
static bool getReturnsCopy = false;
static std::vector<double> copiedIds;
static RValue copyOf(const RValue& v) {
    if (v.m_Kind != VALUE_ARRAY || !v.array) return v;
    RValue c = v;
    c.array = std::make_shared<std::vector<RValue>>(*v.array);
    if (c.array->size() > kModelIndex) {
        if (auto* l = dsFind((*c.array)[kModelIndex])) {
            std::vector<RValue> entries;
            for (const RValue& e : *l) entries.push_back(e.m_Kind == VALUE_ARRAY ? makeArray(*e.array) : e);
            const double id = dsCreate(std::move(entries));
            copiedIds.push_back(id);
            (*c.array)[kModelIndex] = dsHandle(id);
        }
    }
    return c;
}
static std::vector<Triple> vanilla;                       // element 5's list as the game built it
static std::vector<double> vanillaIds;                    // the outer array's ids as the game built it
static std::vector<std::vector<Triple>> vanillaLayout;    // every element's list as the game built it
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
// Elements 0-4: small lists of other triples. Element 1 holds Liquor Holster too, so a count of
// the stand-in over the wrong element (or over every element) is not element 5's n.
static std::vector<std::vector<Triple>> otherElements() {
    return { { { 3, 2, 0 }, { 3, 2, 1 } }, { { 8, 0, 51 }, { 3, 2, 2 } }, { { 5, 1, 0 } }, { { 5, 1, 1 } }, { { 5, 1, 2 } } };
}
static RValue* listVariable() {
    auto it = controllerVars.find(kModelListName);
    return it == controllerVars.end() ? nullptr : &it->second;
}
// The outer array's elements as numbers (-1 for one that is not a number), and each element's
// list as triples (empty for one that is not a live ds_list): what "untouched" is checked on.
static std::vector<double> idsNow() {
    std::vector<double> out;
    if (RValue* v = listVariable(); v && v->m_Kind == VALUE_ARRAY)
        for (const RValue& e : *v->array) out.push_back(dsFind(e) || e.m_Kind == VALUE_REAL ? e.number : -1);
    return out;
}
static std::vector<std::vector<Triple>> layoutNow() {
    std::vector<std::vector<Triple>> out;
    if (RValue* v = listVariable(); v && v->m_Kind == VALUE_ARRAY)
        for (const RValue& e : *v->array) { auto* l = dsFind(e); out.push_back(l ? triplesOf(*l) : std::vector<Triple>{}); }
    return out;
}
static void setList(const std::vector<Triple>& triples) {
    dsLists.clear(); nextDsId = 40;
    std::vector<RValue> ids;
    for (const auto& other : otherElements()) ids.push_back(dsHandle(dsCreate(entriesOf(other))));
    ids.push_back(dsHandle(dsCreate(entriesOf(triples))));
    controllerVars[kModelListName] = makeArray(std::move(ids));
    vanilla = triples;
    vanillaIds = idsNow();
    vanillaLayout = layoutNow();
}
// The old fixture, the layout the plan had before Live 1: one flat array of triples.
static void setFlatList(const std::vector<Triple>& triples) {
    controllerVars[kModelListName] = makeArray(entriesOf(triples));
    vanilla.clear(); vanillaIds.clear(); vanillaLayout.clear();
}
// Element 5's list as the roll reads it, off the variable itself (never a copy).
static std::vector<RValue>* listVector() {
    RValue* v = listVariable();
    if (!v || v->m_Kind != VALUE_ARRAY || v->array->size() <= kModelIndex) return nullptr;
    return dsFind((*v->array)[kModelIndex]);
}
static std::vector<Triple> listTriples() {
    std::vector<Triple> out;
    if (auto* l = listVector()) out = triplesOf(*l);
    return out;
}
// The variable two levels down, as text: for a refusal's "left as found" check, whatever shape it has.
static std::string shapeOf(const RValue& v, int depth = 0) {
    char b[64];
    if (v.m_Kind == VALUE_ARRAY) {
        std::string s = "[";
        for (const RValue& e : *v.array) s += shapeOf(e, depth + 1) + ",";
        return s + "]";
    }
    std::snprintf(b, sizeof b, "%d:%g", v.m_Kind, v.number);
    std::string s = b;
    if (depth < 2) if (auto* l = dsFind(v)) { s += "{"; for (const RValue& e : *l) s += shapeOf(e, 2) + ","; s += "}"; }
    return s;
}
static std::string variableShape() { RValue* v = listVariable(); return v ? shapeOf(*v) : std::string("absent"); }
static std::vector<Triple> plus(std::vector<Triple> v, std::initializer_list<Triple> extra) { v.insert(v.end(), extra.begin(), extra.end()); return v; }
// droprate.base per (type, sub, b); the crown's default stand-in is the type-0 one with the lowest.
static std::map<Triple, double> repoBase;
// The unique data the plugin's own pool build reads (scenarios with `gamePool` only): each known
// triple's name key (itemBaseInfoStruct "28"), and whether the game has it yet. Before it does -
// the panel's auto-apply at the main menu - the model's GetUniqueRepoStruct gives no definition;
// what the game really returns there is not established, only that the build can come back empty.
static bool gamePool = false, uniqueDataReady = true;
static std::map<Triple, std::string> uniqueKeys;
static int poolRepoReads = 0;   // GetUniqueRepoStruct calls the pool build (and the stand-ins) made
// A script InitItemFromJson also calls, set by a scenario: whether the real one calls any hooked
// script is not established, so a scenario can give it the worst case, CreateDefaultParams.
static void (*initItemAlsoCalls)() = nullptr;

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
// A field name the runtime will not let variable_struct_set change (empty: none): what the
// rewrite reads back is then not what it wrote, the one refusal a created field cannot cure.
static std::string frozenField;
struct FakeYytk {
    RValue CallBuiltin(const char* functionName, std::vector<RValue> a) {
        ++builtinCalls;
        const std::string n(functionName);
        if (n == "asset_get_index") return RValue(a[0].text == "Controller_obj" ? 984.0 : a[0].text == "Loot_Ground_obj" ? 902.0 : -1.0);
        if (n == "instance_number") return RValue(num(a[0]) == 984.0 ? (double)controllerCount : 0.0);
        if (n == "instance_find") {
            RValue r(-4.0);   // noone
            if (num(a[0]) == 984.0 && num(a[1]) == 0.0 && controllerCount > 0) {
                if (controllerKind == VALUE_OBJECT) return RValue(&controllerInstance);
                r.m_Kind = VALUE_REF; r.number = kControllerId;
            }
            return r;
        }
        if (n == "variable_instance_exists")
            return RValue(isController(a[0]) && controllerVars.count(a[1].text) ? 1.0 : 0.0);
        if (n == "variable_instance_get") {
            if (!isController(a[0]) || !controllerVars.count(a[1].text)) return RValue();
            return getReturnsCopy ? copyOf(controllerVars[a[1].text]) : controllerVars[a[1].text];
        }
        if (n == "array_length") return RValue((double)arr(a[0]).size());
        if (n == "array_get") return arr(a[0]).at((size_t)num(a[1]));
        if (n == "array_set") { arr(a[0]).at((size_t)num(a[1])) = a[2]; return RValue(); }
        if (n == "array_push") { arr(a[0]).push_back(a[1]); return RValue(); }
        if (n == "array_resize") { arr(a[0]).resize((size_t)num(a[1]), RValue(0.0)); return RValue(); }
        if (n == "array_create") return makeArray(std::vector<RValue>((size_t)num(a[0]), a.size() > 1 ? a[1] : RValue(0.0)));
        // The ds_list builtins: ds_exists takes a number (an array or a struct handed to it is an
        // error, as in GameMaker) and answers for type 2, ds_type_list; the others need a live list.
        if (n == "ds_exists") { num(a[0]); return RValue(num(a[1]) == 2.0 && dsFind(a[0]) ? 1.0 : 0.0); }
        if (n == "ds_list_size") return RValue((double)dsList(a[0]).size());
        if (n == "ds_list_find_value") {
            auto& l = dsList(a[0]);
            const double i = num(a[1]);
            return i >= 0 && i < (double)l.size() ? l[(size_t)i] : RValue();
        }
        if (n == "ds_list_add") { dsList(a[0]).push_back(a[1]); return RValue(); }
        if (n == "ds_list_delete") {
            auto& l = dsList(a[0]);
            const double i = num(a[1]);
            if (i >= 0 && i < (double)l.size()) l.erase(l.begin() + (std::ptrdiff_t)i);
            return RValue();
        }
        if (n == "variable_struct_exists") return RValue(obj(a[0]).count(a[1].text) ? 1.0 : 0.0);
        if (n == "variable_struct_get") { auto& f = obj(a[0]); auto it = f.find(a[1].text); return it == f.end() ? RValue() : it->second; }
        // variable_struct_set creates a field the struct lacks, as in GameMaker.
        if (n == "variable_struct_set") { if (a[1].text != frozenField) obj(a[0])[a[1].text] = a[2]; return RValue(); }
        if (n == "variable_struct_remove") { obj(a[0]).erase(a[1].text); return RValue(); }
        if (n == "json_stringify") {
            std::string s = "{";
            for (const auto& kv : obj(a[0])) { char b[64]; std::snprintf(b, sizeof b, "%g", kv.second.number); s += (s.size() > 1 ? "," : "") + ("\"" + kv.first + "\":") + b; }
            return RValue(s + "}");
        }
        if (n == "json_parse") {   // the pool build's probe item, {"w":1,"a":...,"j":<sub>,"b":<b>,"c":1}
            std::map<std::string, RValue> f;
            const std::string& s = a[0].text;
            for (const char* key : { "w", "a", "j", "b", "c" }) {
                const size_t at = s.find(std::string("\"") + key + "\":");
                if (at != std::string::npos) f[key] = RValue(std::strtod(s.c_str() + at + std::strlen(key) + 3, nullptr));
            }
            return makeStruct(f);
        }
        throw std::runtime_error("unmodelled builtin " + n);
    }
    void CallBuiltinEx(RValue& out, const char* functionName, CInstance*, CInstance*, std::vector<RValue> a) { out = CallBuiltin(functionName, std::move(a)); }
    RValue CallGameScript(std::string_view name, const std::vector<RValue>& a) {
        if (name != "gml_Script_GetUniqueRepoStruct") throw std::runtime_error("unmodelled script");
        const Triple t{ num(a[0]), num(a[1]), num(a[2]) };
        const auto it = repoBase.find(t);
        if (gamePool) {   // the definition the pool build validates: its name key, and its droprate
            ++poolRepoReads;
            const auto key = uniqueKeys.find(t);
            if (!uniqueDataReady || key == uniqueKeys.end()) return RValue();
            return makeStruct({ { "itemBaseInfoStruct", makeStruct({ { "28", RValue(key->second) } }) },
                                { "droprate", makeStruct({ { "base", RValue(it == repoBase.end() ? 50000000.0 : it->second) } }) } });
        }
        if (it == repoBase.end()) return RValue();
        return makeStruct({ { "droprate", makeStruct({ { "base", RValue(it->second) } }) } });
    }
    // InitItemFromJson(parsed, "0-0-1-<type>"): the pool build's probe item, rarity 7 (Angelic)
    // for a unique the game knows, nothing before its unique data is there.
    void CallGameScriptEx(RValue& out, std::string_view name, CInstance*, CInstance*, std::vector<RValue> a) {
        if (name != "gml_Script_InitItemFromJson") throw std::runtime_error("unmodelled script");
        out = RValue();
        if (initItemAlsoCalls) initItemAlsoCalls();
        const std::string& key = a[1].text;
        const Triple t{ std::strtod(key.c_str() + key.rfind('-') + 1, nullptr), obj(a[0])["j"].number, obj(a[0])["b"].number };
        if (uniqueDataReady && uniqueKeys.count(t)) out = makeStruct({ { "itemInfoStruct", makeStruct({ { "27", RValue(7.0) } }) } });
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
// candidates, and whatever a scenario adds. A scenario with `gamePool` runs the plugin's own
// build instead (ProductionBuildAngelicPool, set in reset()) against the model's unique data.
struct AngelicCandidate { int type, sub, b; std::string name; bool angelic; };
static std::vector<AngelicCandidate> g_AngelicPool;
static bool g_AngelicPoolBuilt = false;
static std::vector<AngelicCandidate> poolExtras;
static int poolBuilds = 0;
static void (*productionPoolBuild)(bool) = nullptr;
static void BuildAngelicPool(bool verbose) {
    ++poolBuilds;
    if (gamePool) {
        if (!productionPoolBuild) throw std::runtime_error("this source has no BuildAngelicPool to run");
        productionPoolBuild(verbose);
        return;
    }
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
static std::string g_SigDetectRoute = "off";
// Typing the hit (replan 1): the GetUniqueRepoStruct hook's roll-scoped record, the hit's typed
// triple, and the hits that could not be typed.
static PFUNC_YYGMLScript g_Orig_GetUniqueRepoStruct = nullptr;
static bool g_SigRepoSeen = false;
static double g_SigRepoType = -1.0, g_SigRepoSub = -1.0, g_SigRepoB = -1.0;
static bool g_SigHitTyped = false;
static double g_SigHitType = -1.0;
static volatile long g_SigUntyped = 0;
// The copies per enabled item (the constant 1 in the player build; settable here, as the research
// lever `angelicprobe inject copies <k>` sets it), what this roll pushed of each, the share of
// the current hit, and whether the held read-back's refusal has been logged since it last passed.
static int g_SigCopies = 1;
static int g_SigRollCopies[2] = { 0, 0 };
static int g_SigHitShare = 0;
static bool g_SigHeldMissLogged = false;
// `sigdrop status` (both builds) reads the forced-drop counters too.
static long g_SigDropRolls = 0, g_SigDropHits = 0, g_SigDropFails = 0;
static int g_SigDropForce = -1;
// The list, the injection and the current hit (ModuleMain.cpp's own globals, same names).
static bool g_SigStandInsResolved = false;
// Every name under both its old and its new spelling, so the harness compiles against
// forgepact-74-replan1-base and forgepact-74-replan2-base (the flat list, kSigListMinLength) and
// the replan-2 source (the sub-list at kAngelicListIndex, kSigListMinSize, the roll's ds_list).
static const char* kAngelicListVar = "";
static std::string g_SigListName = kAngelicListVar;
static const int kSigListMinLength = 100;
static const int kSigListMinSize = 10;
static const int kAngelicListIndex = 5;
static int g_SigListIndex = kAngelicListIndex;
static bool g_SigListOk = false, g_SigListTried = false, g_SigListRecount = true;
static int g_SigListLen = -1;
static double g_SigListId = -1.0;
static std::string g_SigListWhy;
static double g_SigControllerIdx = -2.0;
static RValue g_SigRollList;
static RValue g_SigRollListId;
static int g_SigRollIndex = -1;
static int g_SigRollBefore = -1, g_SigRollPushed = 0;
// `angelicprobe inject status` (research build) reads these; the harness compiles it for its `list=`.
static bool g_SigReplaceMode = false;
static volatile long g_SigRemoved = 0, g_SigStandinPicks = 0, g_SigHeldMiss = 0, g_SigTypeAgree = 0, g_SigTypeDisagree = 0;
static bool g_SigRollItem[2] = { false, false };
static int g_SigInjectDepth = 0;
static int g_SigHitItem = -1, g_SigHitCoin = 0, g_SigHitStandIn = -1;
static bool g_SigHitRewritten = false;
static bool g_SigHitAtPoint = false;   // the current hit reached the rewrite point (CreateItemNew's entry)
static std::string g_SigHitWhy;
static int g_SigPending[2] = { 0, 0 };
// The refusal latch: an item whose rewrite was refused is off for the rest of the session, why,
// and how many rewrites were refused (`refused=`).
static bool g_SigRefused[2] = { false, false };
static std::string g_SigRefusedWhy[2];
static volatile long g_SigRefusals = 0;
// The CreateItemNew hook the rewrite runs in (the Custom Forge's Hook_CreateItemNew in the plugin;
// the model's stand-in for it is defined after the production functions) and its saved original.
static PFUNC_YYGMLScript g_Orig_CreateItemNew = nullptr;
static RValue& Hook_CreateItemNew(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A);
// `angelicprobe hit show <k>`'s remaining hits (research build; `angelicprobe hit status` prints it).
static long g_SigShowLeft = 0;
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
// CreateDefaultParams(sub, b, c) returns the struct Live 2 measured, {"b":51.0,"j":0.0,"c":1.0}:
// `j` the first argument, `b` the second, `c` the third, and no field a and no w. The built
// item's `a` is LootGroundCreate's, below.
static int gameCdpCalls = 0;
static bool paramsLackJ = false;   // a CreateDefaultParams whose struct has no `j`
static std::vector<std::string> lastParamsFields;   // the field names the last call returned
static RValue& gameCreateDefaultParams(CInstance*, CInstance*, RValue& result, int argc, RValue** A) {
    ++gameCdpCalls;
    std::map<std::string, RValue> f;
    if (argc > 0 && !paramsLackJ) f["j"] = *A[0];
    if (argc > 1) f["b"] = *A[1];
    if (argc > 2) f["c"] = *A[2];
    lastParamsFields.clear();
    for (const auto& kv : f) lastParamsFields.push_back(kv.first);
    result = makeStruct(std::move(f));
    return result;
}
// Where the roll's direct call lands: the game's own function, or a hook installed on it.
static PFUNC_YYGMLScript cdpEntry = gameCreateDefaultParams;

// GetUniqueRepoStruct(type, sub, b): the unique's definition from the repository (a struct with
// its droprate.base when the model knows the triple). The roll reads the picked entry's
// definition through it before the die, by a direct call like the one to CreateDefaultParams.
static int gameRepoCalls = 0;
static RValue& gameGetUniqueRepoStruct(CInstance*, CInstance*, RValue& result, int argc, RValue** A) {
    ++gameRepoCalls;
    result = makeStruct({});
    if (argc > 2) {
        const auto it = repoBase.find({ A[0]->number, A[1]->number, A[2]->number });
        if (it != repoBase.end()) result = makeStruct({ { "droprate", makeStruct({ { "base", RValue(it->second) } }) } });
    }
    return result;
}
static PFUNC_YYGMLScript repoEntry = gameGetUniqueRepoStruct;
// Which definition reads the roll makes before its die: the picked entry's (the static reading),
// none, or the picked entry's and then another one last.
enum class RepoRead { Picked, Skip, PickedThenOther };
static RepoRead repoRead = RepoRead::Picked;
static const Triple kOtherRead{ 3, 1, 7 };
static void readDefinition(CInstance* self, CInstance* other, const Triple& t) {
    RValue type(t[0]), sub(t[1]), b(t[2]), result;
    RValue* args[] = { &type, &sub, &b };
    repoEntry(self, other, result, 3, args);
}

// LootGroundCreate -> CreateItemNew: one item per placement (Session 5's static reading).
// LootGroundCreate stores a value of its own into the record's `a` - unconditionally, whatever
// was there - then builds the item instance, whose itemType is the placement's type argument (the
// picked entry's) and whose itemDefinitionStruct is the record itself (a second reference, not a
// copy), and calls CreateItemNew on it directly. CreateItemNew reads itemType and the record's
// a, b, c, j and stores none of them. The Custom Forge hook on CreateItemNew (Hook_CreateItemNew,
// below the production functions) sees the call's entry - the rewrite point - and, on its
// return, recognises the two items by their selector {t, a, b, c, j} and reports each to the
// plugin, as TryApplyCustomForge does.
static constexpr double kLootGroundSeed = 515151.0;   // the model's own `a`, LootGroundCreate's store
struct BuildRecord { double t, a, b, c, j; int forged; };   // forged: what was built, 0 crown, 1 belt, -1 neither
static std::vector<BuildRecord> builds;
static double field(const RValue& params, const char* key) {
    if (params.m_Kind != VALUE_OBJECT || !params.fields) return -1;
    const auto it = params.fields->find(key);
    return it == params.fields->end() ? -1 : it->second.number;
}
static const std::map<std::string, double> kSelectors[2] = {
    { { "t", 0.0 }, { "a", 777001.0 }, { "b", 7.0 }, { "c", 0.0 }, { "j", 0.0 } },
    { { "t", 8.0 }, { "a", 777002.0 }, { "b", 2.0 }, { "c", 0.0 }, { "j", 0.0 } },
};
static int selectorOf(double t, const RValue& definition) {
    for (int w = 0; w < 2; ++w) {
        const auto& s = kSelectors[w];
        if (t == s.at("t") && field(definition, "a") == s.at("a") && field(definition, "b") == s.at("b")
            && field(definition, "c") == s.at("c") && field(definition, "j") == s.at("j")) return w;
    }
    return -1;
}
static RValue& gameCreateItemNew(CInstance*, CInstance*, RValue& result, int argc, RValue** A) {
    result = argc > 0 && A && A[0] ? *A[0] : RValue();
    if (result.m_Kind != VALUE_OBJECT) return result;
    const double t = obj(result)["itemType"].number;
    const RValue definition = obj(result)["itemDefinitionStruct"];
    builds.push_back({ t, field(definition, "a"), field(definition, "b"), field(definition, "c"), field(definition, "j"), selectorOf(t, definition) });
    return result;
}
// Where LootGroundCreate's direct call lands: the game's own function, or a detoured hook on it.
static PFUNC_YYGMLScript citemEntry = gameCreateItemNew;
static void gameLootGroundCreate(double type, RValue record) {
    if (record.m_Kind == VALUE_OBJECT) obj(record)["a"] = RValue(kLootGroundSeed);
    RValue instance = makeStruct({ { "itemType", RValue(type) }, { "itemDefinitionStruct", record } });
    RValue* args[] = { &instance };
    RValue built;
    citemEntry(nullptr, nullptr, built, 1, args);
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
static std::vector<std::vector<Triple>> listDuringCall;                 // element 5, per original call
static std::vector<std::vector<std::vector<Triple>>> layoutDuringCall;  // every element, per original call
static std::vector<std::vector<double>> idsDuringCall;                  // the outer array, per original call
static RValue originalReturn;              // what the game's roll hands back: undefined, always
static RValue& gameAngelicChance(CInstance* self, CInstance* other, RValue&, int argc, RValue** A) {
    ++originalCalls;
    lastChanceSeen = (argc > 2 && A && A[2] && A[2]->m_Kind == VALUE_REAL) ? A[2]->number : -1;
    listDuringCall.push_back(listTriples());
    layoutDuringCall.push_back(layoutNow());
    idsDuringCall.push_back(idsNow());
    if (originalThrows) throw std::runtime_error("the game's roll threw");
    // Read element 5 of the variable (the ds_list, never the outer array): an index drawn up to
    // ds_list_size, the entry at it (ds_list_find_value), drawn again unless is_array - the
    // picker below chooses among the entries is_array accepts. Then read the picked entry's
    // definition before the die; only a hit builds the params, by a direct call, then places
    // the item under the picked entry's type.
    std::vector<RValue>* list = listVector();
    Triple picked{ 3, 1, 15 };
    std::vector<size_t> arrays;
    if (list) for (size_t i = 0; i < list->size(); ++i) if ((*list)[i].m_Kind == VALUE_ARRAY) arrays.push_back(i);
    if (!arrays.empty()) {
        size_t index = arrays.front();
        if (pickPolicy == Pick::Last) index = arrays.back();
        else if (pickPolicy == Pick::Among) {
            std::vector<size_t> candidates;
            for (size_t i : arrays)
                if (std::find(pickTriples.begin(), pickTriples.end(), tripleOf((*list)[i])) != pickTriples.end()) candidates.push_back(i);
            if (!candidates.empty()) index = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(pickRng)];
        }
        picked = tripleOf((*list)[index]);
    }
    if (repoRead != RepoRead::Skip) readDefinition(self, other, picked);
    if (repoRead == RepoRead::PickedThenOther) readDefinition(self, other, kOtherRead);
    const bool hit = !outcomes.empty() && outcomes.front();
    if (!outcomes.empty()) outcomes.pop_front();
    if (hit) {
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
    if (name == "GetUniqueRepoStruct") {
        *origOut = gameGetUniqueRepoStruct;
        if (native) repoEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest);
        else gameImage.push_back(reinterpret_cast<const void*>(gameGetUniqueRepoStruct));
        return true;
    }
    if (name == "CreateItemNew") {
        *origOut = gameCreateItemNew;
        if (native) citemEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest);
        else gameImage.push_back(reinterpret_cast<const void*>(gameCreateItemNew));
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

// ForgePact's Hook_CreateItemNew (the ITEM_CREATE_HOOK macro) as far as #74 reaches it: the
// pre-call slot where GemsBeforeCreate and SignatureBeforeCreate run (the rewrite point), the
// original, then the Custom Forge's final pass, which reports a recognised Headhunter / Tyrant's
// Crown through SignatureNoteBuilt.
static RValue& Hook_CreateItemNew(CInstance* S, CInstance* O, RValue& R, int argc, RValue** A) {
#ifdef HAS_SIGNATUREBEFORECREATE
    SignatureBeforeCreate(argc, A);
#endif
    RValue& res = g_Orig_CreateItemNew ? g_Orig_CreateItemNew(S, O, R, argc, A) : R;
    if (res.m_Kind == VALUE_OBJECT) {
        const int w = selectorOf(obj(res)["itemType"].number, obj(res)["itemDefinitionStruct"]);
#ifdef HAS_SIGNATURENOTEBUILT
        if (w >= 0) SignatureNoteBuilt(kSelectors[w]);
#else
        (void)w;
#endif
    }
    return res;
}

static void reset() {
    outLines.clear();
    g_HhEnabled = false; g_TyEnabled = false; g_HhForced = false; g_TyForced = false;
    g_AngelicPool.clear(); poolExtras.clear(); poolBuilds = 0;
    g_AngelicPoolBuilt = false; gamePool = false; uniqueDataReady = true; uniqueKeys.clear(); poolRepoReads = 0; initItemAlsoCalls = nullptr;
#ifdef HAS_PRODUCTIONBUILDANGELICPOOL
    productionPoolBuild = ProductionBuildAngelicPool;
#endif
    spawns.clear(); builds.clear(); builtinCalls = 0;
    controllerCount = 1; controllerVars.clear(); dsIdKind = VALUE_REAL; setList(vanillaTriples());
    controllerKind = VALUE_REF; getReturnsCopy = false; copiedIds.clear();
    repoBase = { { { 8, 0, 51 }, 5000000.0 }, { { 0, 0, 85 }, 111111111.0 }, { { 0, 0, 86 }, 30000000.0 } };
    gameCdpCalls = 0; cdpEntry = gameCreateDefaultParams; paramsLackJ = false; lastParamsFields.clear();
    // The Custom Forge installed its CreateItemNew hook at startup, detoured, as in Live 2 (where
    // builtType= read every hit's built item inside the roll).
    g_Orig_CreateItemNew = gameCreateItemNew; citemEntry = Hook_CreateItemNew; frozenField.clear();
    g_SigHitAtPoint = false; g_SigRefused[0] = g_SigRefused[1] = false; g_SigRefusedWhy[0].clear(); g_SigRefusedWhy[1].clear();
    g_SigRefusals = 0; g_SigShowLeft = 0;
    gameRepoCalls = 0; repoEntry = gameGetUniqueRepoStruct; repoRead = RepoRead::Picked;
    g_Orig_GetUniqueRepoStruct = nullptr; g_SigRepoSeen = false; g_SigRepoType = g_SigRepoSub = g_SigRepoB = -1.0;
    g_SigHitTyped = false; g_SigHitType = -1.0; g_SigUntyped = 0;
    g_SigCopies = 1; g_SigRollCopies[0] = g_SigRollCopies[1] = 0; g_SigHitShare = 0; g_SigHeldMissLogged = false;
    pickPolicy = Pick::First; pickTriples.clear();
    outcomes.clear(); originalCalls = 0; originalThrows = false; gamePushesDuringRoll = false; lastChanceSeen = -1;
    listDuringCall.clear(); layoutDuringCall.clear(); idsDuringCall.clear();
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
    g_SigListIndex = kAngelicListIndex; g_SigListId = -1.0;
    g_SigControllerIdx = -2.0; g_SigRollList = RValue(); g_SigRollBefore = -1; g_SigRollPushed = 0;
    g_SigRollListId = RValue(); g_SigRollIndex = -1;
    g_SigReplaceMode = false; g_SigRemoved = 0; g_SigStandinPicks = 0; g_SigHeldMiss = 0; g_SigTypeAgree = 0; g_SigTypeDisagree = 0;
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
// The plugin's own pool build against the model's unique data, which knows every kAngelicBases
// row by its key (the game agreeing with the plugin's table), but not before `ready`.
static void useGamePool(bool ready) {
    gamePool = true;
    uniqueDataReady = ready;
    uniqueKeys.clear();
#ifdef HAS_KANGELICBASES
    for (const AngelicBase& base : kAngelicBases) uniqueKeys[{ (double)base.type, (double)base.sub, (double)base.b }] = base.key;
#endif
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
// `angelicprobe inject status`, the research build's line (compiled here for its `list=` token).
static std::string injectStatusLine() {
#ifdef HAS_SIGINJECTSTATUS
    SigInjectStatus();
    return lastLine("angelicprobe inject:");
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
            // The player build before Live 2 (no name), a name the instance lacks, a wrong entry, a
            // short sub-list, no Controller_obj, and the nested layout's own steps (replan 2): a
            // flat array of triples, an outer array without element 5, an element that is no live
            // ds_list. No push, the gate off, one refusal line each.
            struct Case { const char* name; void (*arrange)(); const char* why; };
            const Case cases[] = {
                { "no name", [] { g_SigListName = ""; }, "no list variable named yet" },
                { "absent", [] { g_SigListName = "notTheList"; }, "has no variable notTheList" },
                { "shape", [] { auto v = vanillaTriples(); setList(v); (*listVector())[5] = makeArray({ RValue(1.0), RValue(2.0) }); }, "uniqueLoot[5] entry 5 is not three numbers" },
                { "short", [] { const auto all = vanillaTriples(); setList(std::vector<Triple>(all.end() - 5, all.end())); }, "uniqueLoot[5] has 5 entries, fewer than 10" },
                { "no controller", [] { controllerCount = 0; }, "no live Controller_obj instance" },
                { "flat", [] { setFlatList(vanillaTriples()); }, "uniqueLoot[5] is not a ds_list (kind=array, never a handle)" },
                { "five elements", [] { listVariable()->array->resize(5); }, "uniqueLoot has 5 elements, none at [5]" },
                { "dangling id", [] { (*listVariable()->array)[5] = RValue(7.0); }, "uniqueLoot[5] is not a ds_list (kind=real, ds_exists false)" },
            };
            for (const Case& c : cases) {
                reset();
                c.arrange();
                const auto before = listTriples();
                const std::string shape = variableShape();
                g_HhForced = true;
                installDetection();
                for (int i = 0; i < 5; ++i) roll(monster, { false });
                require(anyLineHas("signature drops: list missing", c.why), std::string(c.name) + ": the switch-on did not refuse naming why: "
                        + lastLine("signature drops: list missing"));
                require(linesStartingWith("signature drops: list missing") == 1, std::string(c.name) + ": not exactly one refusal line");
                require(g_SigInjected == 0 && listTriples() == before && variableShape() == shape,
                        std::string(c.name) + ": an entry was pushed onto a list that did not resolve");
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
        } else if (test == "missing_field_is_created_and_a_refusal_restores") {
            // A record without a field the rewrite writes (here j): the field is created and read
            // back like the others, and the game builds ours.
            setList(vanillaTriples(false));
            paramsLackJ = true;
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(g_SigOurHits == 1, "the hit was not ours");
            for (const auto& line : outLines) require(line.find("refused") == std::string::npos, "a missing field was refused instead of created: " + line);
            require(builds.size() == 1 && builds[0].t == 8 && builds[0].a == 777002 && builds[0].b == 2 && builds[0].c == 0 && builds[0].j == 0,
                    "the created field did not reach the build");
            require(countBuilds(1) == 1 && g_SigBuilt == 1, "a record that lacked j did not build Headhunter");
            // A value that does not read back is still a refusal, and it puts every field back,
            // the one it created removed again: the game builds its own record.
            reset();
            setList(vanillaTriples(false));
            paramsLackJ = true;
            frozenField = "b";
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(g_SigOurHits == 1, "the hit was not ours");
            require(anyLineHas("inject: Headhunter refused", "field b did not read back"), "the refusal does not name the field: " + lastLine("inject:"));
            require(anyLineHas("inject: Headhunter refused", "\"b\":51"), "the refusal does not carry the record's JSON");
            require(builds.size() == 1 && builds[0].t == 8 && builds[0].a == kLootGroundSeed && builds[0].b == 51 && builds[0].c == 1.0,
                    "a refused rewrite did not leave the record vanilla");
            require(builds[0].j == -1, "a refused rewrite left the field it created on the record");
            require(g_SigBuilt == 0 && spawns.empty(), "a refused rewrite counted or spawned an item");
            require(anyLineHas("angelic hit:", "refused (field b did not read back)"), "the hit line does not say it was refused");
        } else if (test == "sigdrop_status_tokens") {
            const std::string line = sigdropStatusLine();
            // Exactly the fresh-session banner the live procedure's `control` step reads.
            const std::string banner = "sigdrop: force off | rolls=0 drops=0 fails=0 | game roll: gameRolls=0 gameHits=0 injected=0 ourHits=0 refused=0 untyped=0 built=0 crown=0 belt=0 anomalies=0 list=none gate=tyrant:off,headhunter:off cdpCalls=0 detect=off";
            require(line == banner, "`sigdrop status` is not the fresh-session banner: " + line);
            g_HhForced = true;
            installDetection();
            require(sigdropStatusLine().find(" list=uniqueLoot[5]:120 gate=tyrant:off,headhunter:on ") != std::string::npos, "`list=` does not name the resolved list");
        // ---- identity (replan 1): every scenario but the controls fails against forgepact-74-replan1-base ----
        } else if (test == "other_type_same_pair_never_attributed") {
            // Supreme Elemelon's shape: type 10 with Liquor Holster's sub 0 / b 51. Its hits name the
            // stand-in's pair to CreateDefaultParams, but the definition the roll read is type 10's,
            // so none of them is ours and none is rewritten - a wrong-type rewrite builds a
            // malformed item.
            const Triple elemelon{ 10, 0, 51 };
            setList(plus(vanillaTriples(), { elemelon }));
            g_HhForced = true;
            installDetection();
            require(switchOn(1), "a stand-in whose pair another type shares no longer refuses: the gate must arm");
            pickPolicy = Pick::Among; pickTriples = { elemelon };
            for (int i = 0; i < 400; ++i) roll(monster, { true });
            require(g_SigGameHits == 400 && builds.size() == 400, "not one item built per hit");
            require(g_SigOurHits == 0, "a hit on another type's same-pair entry was attributed: ourHits=" + std::to_string(g_SigOurHits));
            for (const auto& b : builds)
                require(b.t == 10 && b.b == 51 && b.c == 1.0 && b.a == kLootGroundSeed, "a hit on another type's same-pair entry was rewritten");
            require(countBuilds(1) == 0 && g_SigBuilt == 0, "a Headhunter came from another type's entry");
            require(g_SigUntyped == 0, "a hit whose definition the roll read was counted untyped");
            require(anyLineHas("angelic hit: picked 10/0/51 ", "-> vanilla"), "the hit line does not name the typed triple 10/0/51");
            // Positive control, same session: a hit on our own entry is ours.
            setList(plus(vanillaTriples(false), { elemelon }));
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(g_SigOurHits == 1 && countBuilds(1) == 1, "a hit on the injected stand-in was not ours");
        } else if (test == "no_agreeing_record_is_untyped") {
            // n = 0: the injected entry is the only Liquor Holster, so every typed hit on it is ours.
            setList(vanillaTriples(false));
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Last;
            for (RepoRead read : { RepoRead::Skip, RepoRead::PickedThenOther }) {
                repoRead = read;
                const long untyped = g_SigUntyped;
                const size_t built = builds.size();
                roll(monster, { true });
                const char* how = read == RepoRead::Skip ? "no definition read" : "another definition read last";
                require(g_SigUntyped == untyped + 1, std::string(how) + ": the hit was not counted untyped");
                require(g_SigOurHits == 0, std::string(how) + ": an untyped hit was attributed");
                require(builds.size() == built + 1 && builds.back().t == 8 && builds.back().b == 51 && builds.back().c == 1.0 && builds.back().a == kLootGroundSeed,
                        std::string(how) + ": an untyped hit did not build the game's own Liquor Holster from its own parameters");
                require(lastLine("angelic hit:").find("picked untyped 0/51 ") != std::string::npos, std::string(how) + ": the hit line does not say untyped: " + lastLine("angelic hit:"));
            }
            require(sigdropStatusLine().find(" untyped=2 ") != std::string::npos, "`sigdrop status` does not count untyped=2");
            // Positive control: the roll reads the picked entry's definition last, the hit is typed and ours.
            repoRead = RepoRead::Picked;
            roll(monster, { true });
            require(g_SigOurHits == 1 && countBuilds(1) == 1 && g_SigUntyped == 2, "a typed hit on our entry was not ours");
        } else if (test == "standin_listed_twice_coin_one_in_three") {
            setList(plus(vanillaTriples(), { kBeltStandIn }));   // n = 2
            g_HhForced = true;
            installDetection();
            require(anyLineHas("signature drops:", "Headhunter:Liquor Holster(n=2)"), "n does not count the stand-in triple twice");
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 3000; ++i) roll(monster, { true });
            require(g_SigOurHits >= 850 && g_SigOurHits <= 1150, "a Liquor Holster hit was not ours about 1 in 3: " + std::to_string(g_SigOurHits));
            require(linesEndingWith("angelic hit:", "Headhunter (stand-in Liquor Holster, 1 in 3) built by the game") == (int)g_SigOurHits,
                    "an our-hit line does not say 1 in 3");
        } else if (test == "copies_coin_and_tail") {
            g_SigCopies = 5;
            g_HhForced = true;
            installDetection();
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kBeltStandIn, kBeltStandIn, kBeltStandIn, kBeltStandIn, kBeltStandIn }),
                    "with copies 5 the roll did not see Liquor Holster five more times: " + describe(listDuringCall[0]));
            require(listTriples() == vanilla && g_SigInjected == 5, "the tail check did not take all five copies off, or injected= did not count them");
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 1200; ++i) roll(monster, { true });   // n = 1, k = 5: ours 5 in 6
            require(g_SigOurHits >= 900 && g_SigOurHits <= 1100, "with copies 5 a Liquor Holster hit was not ours about 5 in 6: " + std::to_string(g_SigOurHits));
            require(linesEndingWith("angelic hit:", "Headhunter (stand-in Liquor Holster, 5 in 6) built by the game") == (int)g_SigOurHits, "an our-hit line does not say 5 in 6");
            require(listTriples() == vanilla, "copies were left on the list");
            // Both on with copies 3: both items' copies, in push order, and all of them come off.
            g_SigCopies = 3; g_TyForced = true;
            listDuringCall.clear();
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kCrownStandIn, kCrownStandIn, kCrownStandIn, kBeltStandIn, kBeltStandIn, kBeltStandIn }),
                    "both on with copies 3, the roll did not see three of each: " + describe(listDuringCall[0]));
            require(listTriples() == vanilla && g_SigAnomalies == 0, "both on with copies 3, the list was not restored");
            // A list the game changed mid-roll is still left as found with copies.
            gamePushesDuringRoll = true;
            roll(monster, { false });
            require(g_SigAnomalies == 1 && linesStartingWith("inject: list changed during the roll, left as found") == 1, "a changed list was cut with copies on it");
        } else if (test == "copied_list_held_read_back") {
            // The runtime hands back a copy: the push lands on the copy, the roll reads the list
            // itself. The held read-back sees it, takes the entries off again and the roll carries
            // nothing, so a vanilla Liquor Holster hit is never taken for ours.
            getReturnsCopy = true;
            g_HhForced = true;
            installDetection();
            require(switchOn(1), "a list that resolves by name and shape did not arm the gate");
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 200; ++i) roll(monster, { true });
            for (const auto& seen : listDuringCall) require(seen == vanilla, "the roll saw an entry the read-back did not");
            require(g_SigOurHits == 0 && countBuilds(1) == 0, "a roll whose push was not visible attributed a hit: ourHits=" + std::to_string(g_SigOurHits));
            require(g_SigInjected == 0, "injected= counted entries the list never held");
            require(g_SigAnomalies == 200, "anomalies= did not count every invisible push: " + std::to_string(g_SigAnomalies));
            require(linesStartingWith("inject: push not visible through Controller_obj.uniqueLoot") == 1, "the read-back's refusal was not logged exactly once");
            require(listTriples() == vanilla, "something was left on the list");
            // Positive control: the list itself comes back, the push is visible, the roll carries it.
            getReturnsCopy = false;
            listDuringCall.clear();
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kBeltStandIn }) && g_SigInjected == 1 && g_SigAnomalies == 200, "a visible push was refused");
            require(listTriples() == vanilla, "the visible push was not removed");
            getReturnsCopy = true;   // once per change: a copy again logs again
            roll(monster, { false });
            require(linesStartingWith("inject: push not visible through Controller_obj.uniqueLoot") == 2, "the refusal was not logged again after the outcome changed");
        } else if (test == "value_ref_and_value_object_resolve_alike") {
            for (int kind : { VALUE_REF, VALUE_OBJECT }) {
                reset();
                controllerKind = kind;
                g_HhForced = true;
                installDetection();
                const std::string what = kind == VALUE_REF ? "VALUE_REF" : "VALUE_OBJECT";
                require(switchOn(1), what + ": the gate did not arm");
                roll(monster, { false });
                require(listDuringCall[0] == plus(vanilla, { kBeltStandIn }), what + ": the roll did not see the stand-in");
                require(listTriples() == vanilla && g_SigAnomalies == 0, what + ": the list was not restored, or the read-back refused");
            }
        } else if (test == "wrong_shape_still_refuses") {
            // Negative control for the widened instance kinds: a list of the wrong shape refuses.
            for (int kind : { VALUE_REF, VALUE_OBJECT }) {
                reset();
                controllerKind = kind;
                auto v = vanillaTriples();
                setList(v);
                (*listVector())[5] = makeArray({ RValue(1.0), RValue(2.0), RValue(3.0), RValue(4.0) });
                const auto before = listTriples();
                g_HhForced = true;
                installDetection();
                roll(monster, { false });
                require(anyLineHas("signature drops: list missing", "entry 5 is not three numbers"), "a four-number entry did not refuse");
                require(!switchOn(1) && g_SigInjected == 0 && listTriples() == before, "a list of the wrong shape was injected into");
            }
        } else if (test == "typing_hook_installed_once_by_name") {
            g_OrigAngChance = nullptr;
            installDetection();
            installDetection();
            require(installs["GetUniqueRepoStruct"] == 1, "GetUniqueRepoStruct was not hooked exactly once");
            require(repoEntry != gameGetUniqueRepoStruct, "the roll's direct GetUniqueRepoStruct call does not reach the hook");
            // Outside a roll the hook passes through and records nothing.
            readDefinition(&monster, nullptr, kBeltStandIn);
            require(gameRepoCalls == 1 && !g_SigRepoSeen, "a GetUniqueRepoStruct call outside the roll was recorded or not passed through");
            require(sigdropStatusLine().find(" detect=detoured") != std::string::npos, "three detoured hooks do not read detect=detoured");
        } else if (test == "typing_hook_not_detoured_never_arms") {
            // Positive control: all three detoured, the gate arms.
            g_HhForced = true;
            installDetection();
            require(switchOn(1), "three detoured hooks with the list resolved did not arm the gate");
            struct Case { const char* name; bool table; const char* log; const char* detect; };
            const Case cases[] = {
                { "table-only", true, "GetUniqueRepoStruct TABLE-ONLY", " detect=GetUniqueRepoStruct:TABLE-ONLY" },
                { "not found", false, "GetUniqueRepoStruct not found", " detect=GetUniqueRepoStruct:not found" },
            };
            for (const Case& c : cases) {
                reset();
                g_HhForced = true;
                if (c.table) tableOnlyHook = "GetUniqueRepoStruct"; else missingHook = "GetUniqueRepoStruct";
                installDetection();
                require(anyLineHas("signature drops: game-roll detection NOT installed", c.log), std::string(c.name) + ": the typing hook's route was not logged");
                require(!switchOn(1), std::string(c.name) + ": the gate armed without the typing hook");
                const std::string status = sigdropStatusLine();
                require(status.size() >= std::string(c.detect).size() && status.compare(status.size() - std::string(c.detect).size(), std::string::npos, c.detect) == 0,
                        std::string(c.name) + ": `detect=` does not name the typing hook: " + status);
                pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
                for (int i = 0; i < 20; ++i) roll(monster, { true });
                require(g_SigInjected == 0 && g_SigOurHits == 0, std::string(c.name) + ": entries were pushed or a hit taken without typing");
            }
        // ---- layout (replan 2): every scenario fails against forgepact-74-replan2-base ----
        } else if (test == "layout_push_lands_in_element_five_only") {
            g_HhForced = true;
            installDetection();
            std::vector<const void*> entries;   // element 5's own entries, to see they are the same ones after
            for (const RValue& e : *listVector()) entries.push_back(e.array.get());
            roll(monster, { false });
            require(layoutDuringCall.size() == 1, "the game's roll did not run once");
            require(idsDuringCall[0] == vanillaIds, "while the roll ran, the outer array was not the game's own");
            for (size_t e = 0; e < kModelIndex; ++e)
                require(layoutDuringCall[0][e] == vanillaLayout[e], "while the roll ran, element " + std::to_string(e) + " changed");
            require(layoutDuringCall[0][kModelIndex] == plus(vanilla, { kBeltStandIn }),
                    "while the roll ran, element 5's ds_list was not the game's own plus Liquor Holster once: " + describe(layoutDuringCall[0][kModelIndex]));
            require(layoutNow() == vanillaLayout && idsNow() == vanillaIds, "after the roll the layout is not the game's own again");
            require(listVector()->size() == entries.size(), "after the roll element 5's size is not the size before");
            for (size_t i = 0; i < entries.size(); ++i)
                require((*listVector())[i].array.get() == entries[i], "after the roll element 5 does not hold the same entries in the same order");
            // Both on, three copies each: six entries on element 5 only, all six off again.
            g_TyForced = true; g_SigCopies = 3;
            roll(monster, { false });
            require(layoutDuringCall[1][kModelIndex].size() == vanilla.size() + 6
                    && layoutDuringCall[1][kModelIndex] == plus(vanilla, { kCrownStandIn, kCrownStandIn, kCrownStandIn, kBeltStandIn, kBeltStandIn, kBeltStandIn }),
                    "both on with copies 3, element 5 did not carry three of each: " + describe(layoutDuringCall[1][kModelIndex]));
            for (size_t e = 0; e < kModelIndex; ++e) require(layoutDuringCall[1][e] == vanillaLayout[e], "both on, element " + std::to_string(e) + " changed");
            require(layoutNow() == vanillaLayout && idsNow() == vanillaIds && g_SigAnomalies == 0 && g_SigInjected == 7,
                    "both on with copies 3, the layout was not restored or the pushes not counted");
        } else if (test == "layout_n_counts_element_five_only") {
            // Element 1 holds Liquor Holster as well: n is element 5's count, 1, not 2.
            g_HhForced = true;
            installDetection();
            require(anyLineHas("signature drops: list uniqueLoot[5]:120", "Headhunter:Liquor Holster(n=1)"),
                    "n is not element 5's count of the stand-in: " + lastLine("signature drops: list"));
            reset();
            setList(vanillaTriples(false));   // only element 1 holds it now
            g_HhForced = true;
            installDetection();
            require(anyLineHas("signature drops: list uniqueLoot[5]:119", "Headhunter:Liquor Holster(n=0)"),
                    "a stand-in triple in another element was counted: " + lastLine("signature drops: list"));
            require(switchOn(1), "n = 0 refused the gate");
        } else if (test == "layout_refusals") {
            // Every step of the nested resolution refuses with its own reason, and the variable is
            // left exactly as found: the old fixture (a flat array of triples), an outer array of
            // five elements, an element 5 that is a number with no live ds_list, a string, or a
            // reference ds_exists turns away, a ds_list with a non-triple entry, a variable that is
            // no array, a sub-list shorter than the minimum.  Each ds_list refusal names the kind
            // the element arrived as and the step that refused it.  An array or a string is
            // refused as never a handle, before any conversion; a number or a reference goes on to
            // ds_exists, so no handle kind is ever turned away by its kind.
            struct Case { const char* name; void (*arrange)(); const char* why; };
            const Case cases[] = {
                { "flat array of triples", [] { setFlatList(vanillaTriples()); }, "Controller_obj.uniqueLoot[5] is not a ds_list (kind=array, never a handle)" },
                { "five elements", [] { listVariable()->array->resize(5); }, "Controller_obj.uniqueLoot has 5 elements, none at [5]" },
                { "number with no ds_list", [] { (*listVariable()->array)[5] = RValue(7.0); }, "Controller_obj.uniqueLoot[5] is not a ds_list (kind=real, ds_exists false)" },
                { "string", [] { (*listVariable()->array)[5] = RValue("7"); }, "Controller_obj.uniqueLoot[5] is not a ds_list (kind=string, never a handle)" },
                { "reference with no ds_list", [] { RValue r(7.0); r.m_Kind = VALUE_REF; (*listVariable()->array)[5] = r; },
                  "Controller_obj.uniqueLoot[5] is not a ds_list (kind=ref, ds_exists false)" },
                { "non-triple entry", [] { (*listVector())[3] = makeArray({ RValue(1.0), RValue(2.0) }); }, "Controller_obj.uniqueLoot[5] entry 3 is not three numbers" },
                { "not an array", [] { controllerVars[kModelListName] = RValue(3.0); }, "Controller_obj.uniqueLoot is not an array" },
                { "short", [] { setList({ { 3, 1, 0 }, { 3, 1, 1 }, { 8, 0, 51 } }); }, "Controller_obj.uniqueLoot[5] has 3 entries, fewer than 10" },
            };
            for (const Case& c : cases) {
                reset();
                c.arrange();
                const std::string shape = variableShape();
                g_HhForced = true;
                installDetection();
                for (int i = 0; i < 3; ++i) roll(monster, { false });
                require(anyLineHas("signature drops: list missing", c.why),
                        std::string(c.name) + ": no refusal naming `" + c.why + "`: " + lastLine("signature drops: list missing"));
                require(linesStartingWith("signature drops: list missing") == 1, std::string(c.name) + ": not exactly one refusal line");
                require(g_SigInjected == 0 && !switchOn(1), std::string(c.name) + ": entries were pushed or the gate armed");
                require(variableShape() == shape, std::string(c.name) + ": the variable was not left as found");
                require(sigdropStatusLine().find(" list=missing ") != std::string::npos, std::string(c.name) + ": `sigdrop status` does not say list=missing");
            }
        } else if (test == "kind_gate_refuses_before_converting") {
            // report#2 (#74): converting an array or a string to a number raises a runner error
            // that a C++ catch does not take back, so the gate refuses the kinds that can never be
            // a handle - array, string, struct, undefined, null - before it converts anything, and
            // the stub's count of refused conversions does not move.  A live list whose handle is
            // a number or a reference is still accepted: the refusal is a deny-list, never an
            // allow-list of handle kinds.
            struct Case { const char* name; void (*arrange)(); const char* why; };
            const Case refused[] = {
                { "array", [] { (*listVariable()->array)[5] = makeArray({ RValue(8.0), RValue(0.0), RValue(51.0) }); },
                  "Controller_obj.uniqueLoot[5] is not a ds_list (kind=array, never a handle)" },
                { "string", [] { (*listVariable()->array)[5] = RValue("44"); }, "Controller_obj.uniqueLoot[5] is not a ds_list (kind=string, never a handle)" },
                { "struct", [] { (*listVariable()->array)[5] = makeStruct({ { "id", RValue(44.0) } }); },
                  "Controller_obj.uniqueLoot[5] is not a ds_list (kind=struct, never a handle)" },
                { "undefined", [] { (*listVariable()->array)[5] = RValue(); }, "Controller_obj.uniqueLoot[5] is not a ds_list (kind=undefined, never a handle)" },
                { "null", [] { RValue r; r.m_Kind = VALUE_NULL; (*listVariable()->array)[5] = r; },
                  "Controller_obj.uniqueLoot[5] is not a ds_list (kind=null, never a handle)" },
            };
            for (const Case& c : refused) {
                reset();
                c.arrange();
                const std::string shape = variableShape();
                const long conversions = refusedConversions;
                g_HhForced = true;
                installDetection();
                for (int i = 0; i < 3; ++i) roll(monster, { false });
                require(anyLineHas("signature drops: list missing", c.why),
                        std::string(c.name) + ": no refusal naming `" + c.why + "`: " + lastLine("signature drops: list missing"));
                require(refusedConversions == conversions, std::string(c.name) + ": the gate converted a value that can never be a handle ("
                        + std::to_string(refusedConversions - conversions) + " refused conversions, each a runner error)");
                require(g_SigInjected == 0 && !switchOn(1), std::string(c.name) + ": entries were pushed or the gate armed");
                require(variableShape() == shape, std::string(c.name) + ": the variable was not left as found");
            }
            for (const int kind : { (int)VALUE_REAL, (int)VALUE_REF }) {
                const std::string name = kind == VALUE_REAL ? "real handle" : "ref handle";
                reset();
                dsIdKind = kind;
                setList(vanillaTriples());
                const long conversions = refusedConversions;
                g_HhForced = true;
                installDetection();
                require(switchOn(1), name + ": a live ds_list was refused: " + lastLine("signature drops: list"));
                require(linesStartingWith("signature drops: list missing") == 0, name + ": a live ds_list was reported missing");
                roll(monster, { false });
                require(listDuringCall.size() == 1 && listDuringCall[0] == plus(vanilla, { kBeltStandIn }), name + ": the roll did not see the stand-in");
                require(layoutNow() == vanillaLayout && g_SigInjected == 1, name + ": the layout was not restored or the push not held");
                require(refusedConversions == conversions, name + ": a conversion was refused on a live list ("
                        + std::to_string(refusedConversions - conversions) + ")");
            }
        } else if (test == "layout_held_miss_on_another_id") {
            // A fresh read whose element 5 is another ds_list (a copying runtime): a held miss.
            // The entries come off the ds_list they went onto, nothing is attributed, and the roll
            // reads the game's own list throughout.
            getReturnsCopy = true;
            g_HhForced = true;
            installDetection();
            require(switchOn(1), "a list that resolves by name and layout did not arm the gate");
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 50; ++i) roll(monster, { true });
            for (const auto& seen : listDuringCall) require(seen == vanilla, "the roll saw an entry the read-back did not");
            require(g_SigOurHits == 0 && countBuilds(1) == 0 && g_SigInjected == 0, "a roll whose push was not visible attributed a hit or counted entries");
            require(g_SigAnomalies == 50, "anomalies= did not count every held miss: " + std::to_string(g_SigAnomalies));
            require(linesStartingWith("inject: push not visible through Controller_obj.uniqueLoot[5] (a fresh read holds another value at [5]") == 1,
                    "the held miss was not logged once, naming the index and the other list: " + lastLine("inject: push not visible"));
            require(!copiedIds.empty(), "the model made no copy");
            for (double id : copiedIds)
                require(triplesOf(dsLists[id]) == vanilla, "entries were left on the ds_list " + std::to_string(id) + " they were pushed onto");
            require(layoutNow() == vanillaLayout && idsNow() == vanillaIds, "the game's own layout changed");
            // Positive control: the same list comes back, the push is held and the roll carries it.
            getReturnsCopy = false;
            listDuringCall.clear();
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kBeltStandIn }) && g_SigInjected == 1 && g_SigAnomalies == 50, "a held push was refused");
            require(layoutNow() == vanillaLayout, "the held push was not removed");
        } else if (test == "layout_status_tokens") {
            g_HhForced = true;
            installDetection();
            require(sigdropStatusLine().find(" list=uniqueLoot[5]:120 ") != std::string::npos, "`sigdrop status` does not read list=<name>[5]:<size>: " + sigdropStatusLine());
            const std::string inject = injectStatusLine();
            require(inject.find(" list=uniqueLoot[5]:120 ") != std::string::npos, "`angelicprobe inject status` does not read list=<name>[5]:<size>: " + inject);
            reset();
            require(injectStatusLine().find(" list=none ") != std::string::npos, "`angelicprobe inject status` before any resolution is not list=none");
        } else if (test == "layout_hit_on_pushed_entry_k_in_n_plus_k") {
            // n = 1 on element 5 (element 1's Liquor Holster is not drawn from), k = 3: 3 in 4.
            g_SigCopies = 3;
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 2000; ++i) roll(monster, { true });
            require(g_SigOurHits >= 1350 && g_SigOurHits <= 1650, "a Liquor Holster hit was not ours about 3 in 4: " + std::to_string(g_SigOurHits));
            require(linesEndingWith("angelic hit:", "Headhunter (stand-in Liquor Holster, 3 in 4) built by the game") == (int)g_SigOurHits,
                    "an our-hit line does not say 3 in 4");
            require(countBuilds(1) == (int)g_SigOurHits && layoutNow() == vanillaLayout, "the hits were not built, or the layout not restored");
        } else if (test == "layout_handle_as_reference") {
            // The outer array may hold each ds_list id as a reference: the same list, the same work.
            dsIdKind = VALUE_REF;
            setList(vanillaTriples());
            g_HhForced = true;
            installDetection();
            require(switchOn(1), "ds_list ids held as VALUE_REF did not arm the gate");
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kBeltStandIn }), "ids held as VALUE_REF: the roll did not see the stand-in");
            require(layoutNow() == vanillaLayout && g_SigAnomalies == 0 && g_SigInjected == 1, "ids held as VALUE_REF: the layout was not restored or the push not held");
        } else if (test == "layout_dump_two_levels") {
#ifdef HAS_SIGLISTDUMP
            // The research dump reads the variable two levels down and writes nothing.
            (*listVariable()->array)[2] = RValue(7.0);                                  // a number with no live ds_list
            dsList((*listVariable()->array)[3]).push_back(RValue(9.0));                 // a ds_list with a non-triple entry
            const std::string shape = variableShape();
            SigListDump(kModelListName);
            const std::string h = "angelicprobe list dump: ";
            require(anyLineHas((h + "Controller_obj.uniqueLoot kind=array array_length=6").c_str(), ""), "no first line naming the variable and its length");
            require(anyLineHas((h + "[1] kind=real ds_list=yes:2 triples=2/2 first=[8,0,51][3,2,2] standins=Headhunter:Liquor Holster(n=1),").c_str(), ""),
                    "element 1 was not printed with its own count");
            require(anyLineHas((h + "[2] kind=real ds_list=no").c_str(), ""), "a number with no ds_list was not printed as ds_list=no");
            require(anyLineHas((h + "[3] kind=real ds_list=yes:2 triples=1/2 first=[5,1,1]entry1=real").c_str(), ""), "a non-triple entry was not printed by its kind");
            require(anyLineHas((h + "[5] kind=real ds_list=yes:120 triples=120/120 first=[3,1,0][3,1,1][3,1,2] standins=Headhunter:Liquor Holster(n=1),Tyrant's Crown:Mask of the Celestial(n=1)").c_str(), ""),
                    "element 5 was not printed whole");
            require(lastLine("angelicprobe list dump:") == h + "uniqueLoot elements=6 ds_lists=5 triple-lists=4", "the summary is not the last line: " + lastLine("angelicprobe list dump:"));
            require(variableShape() == shape && g_SigInjected == 0, "the dump wrote to the list");
            SigListDump("");   // lootListUnique by default; the model has none
            require(lastLine("angelicprobe list dump:") == h + "Controller_obj has no variable lootListUnique", "the default name is not lootListUnique");
            controllerVars["notAList"] = RValue(3.0);
            SigListDump("notAList");
            require(lastLine("angelicprobe list dump:") == h + "Controller_obj.notAList kind=real, not an array", "a non-array was not reported and stopped");
#else
            require(false, "no `angelicprobe list dump` to read the layout with");
#endif
        // ---- build id (Session 5): Live 2's struct; the target fails against forgepact-74-live2-base ----
        } else if (test == "measured_params_build_our_item") {
            // The target. CreateDefaultParams hands back {j, b, c}, no field a, which the plugin Live
            // 2 ran refused on (`no field a`) at every one of 65 hits. LootGroundCreate stores its
            // own a; the plugin writes Headhunter's a, b, c, j onto that record at CreateItemNew's
            // entry, and the game builds Headhunter once from it.
            setList(vanillaTriples(false));   // n = 0: every typed hit on the stand-in is ours
            g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(lastParamsFields == std::vector<std::string>{ "b", "c", "j" }, "the model's CreateDefaultParams did not return the measured {j, b, c}");
            require(g_SigOurHits == 1, "the hit on our entry was not ours");
            for (const auto& line : outLines) require(line.find("refused") == std::string::npos, "the rewrite refused: " + line);
            require(builds.size() == 1, "the game did not build exactly one item for one hit");
            const BuildRecord b = builds[0];
            require(b.t == 8 && b.a == 777002 && b.b == 2 && b.c == 0 && b.j == 0,
                    "the game did not build from Headhunter's own definition {t 8, a 777002, b 2, c 0, j 0}: t=" + std::to_string(b.t)
                    + " a=" + std::to_string(b.a) + " b=" + std::to_string(b.b) + " c=" + std::to_string(b.c) + " j=" + std::to_string(b.j));
            require(b.forged == 1 && g_SigBuilt == 1 && g_SigBuiltBelt == 1 && g_SigBuiltCrown == 0, "the forge hook's final pass did not record built=1 belt=1");
            const std::string status = sigdropStatusLine();
            require(status.find(" built=1 ") != std::string::npos && status.find(" belt=1 ") != std::string::npos, "`sigdrop status` does not read built=1 belt=1: " + status);
            require(linesEndingWith("angelic hit:", "Headhunter (stand-in Liquor Holster, 1 in 1) built by the game") == 1, "the hit line does not end `built by the game`: " + lastLine("angelic hit:"));
            require(spawns.empty(), "the roll path spawned an item of its own");
        } else if (test == "measured_params_vanilla_untouched") {
            // The baseline on the same struct: both switches off, the detection installed (as a
            // research lever installs it). The game builds its own Liquor Holster from its own
            // record - LootGroundCreate's a, b 51, c 1, j 0 - and nothing of ours touches it.
            installDetection();
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            roll(monster, { true });
            require(lastParamsFields == std::vector<std::string>{ "b", "c", "j" }, "the model's CreateDefaultParams did not return the measured {j, b, c}");
            require(builds.size() == 1, "the game did not build exactly one item for one hit");
            const BuildRecord b = builds[0];
            require(b.t == 8 && b.a == kLootGroundSeed && b.b == 51 && b.c == 1.0 && b.j == 0 && b.forged == -1,
                    "the game's own Liquor Holster was not built from its own record (the model's a, b 51, c 1, j 0)");
            require(g_SigOurHits == 0 && g_SigBuilt == 0 && g_SigInjected == 0, "with both switches off something of ours was counted");
            require(linesStartingWith("inject:") == 0, "with both switches off an `inject:` line was printed");
            require(listTriples() == vanilla && spawns.empty(), "with both switches off the list changed or an item was spawned");
        } else if (test == "item_hook_route_decides_the_gate") {
            // LootGroundCreate calls CreateItemNew directly, so the rewrite point is reached only
            // through a detour. 1. Nothing holds CreateItemNew (no Custom Forge, no Item Truth): the
            // installer hooks it by its SDK name, once, and our hit is built.
            g_Orig_CreateItemNew = nullptr; citemEntry = gameCreateItemNew;
            setList(vanillaTriples(false));
            g_HhForced = true;
            installDetection();
            installDetection();
            require(installs["CreateItemNew"] == 1, "CreateItemNew was not hooked exactly once");
            require(citemEntry != gameCreateItemNew, "LootGroundCreate's direct call does not reach the hook");
            require(switchOn(1), "four detoured hooks with the list resolved did not arm the gate");
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(countBuilds(1) == 1 && g_SigBuilt == 1, "with the installer's own CreateItemNew hook our hit was not built");
            // 2. A table-only hook holds it already: the direct call never reaches the rewrite, so
            // the gate stays off and `detect=` names it.
            reset();
            gameImage.push_back(reinterpret_cast<const void*>(gameCreateItemNew));
            citemEntry = gameCreateItemNew;
            g_HhForced = true;
            installDetection();
            require(anyLineHas("signature drops: game-roll detection NOT installed", "CreateItemNew TABLE-ONLY"), "a table-only CreateItemNew was not logged");
            require(!switchOn(1), "a table-only CreateItemNew armed the gate");
            std::string status = sigdropStatusLine();
            const std::string tail = " detect=CreateItemNew:TABLE-ONLY";
            require(status.size() >= tail.size() && status.compare(status.size() - tail.size(), std::string::npos, tail) == 0,
                    "`detect=` does not name CreateItemNew: " + status);
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 20; ++i) roll(monster, { true });
            require(g_SigInjected == 0 && g_SigOurHits == 0, "entries were pushed or a hit taken with the rewrite point unreachable");
            // 3. The runtime cannot resolve it by name.
            reset();
            g_Orig_CreateItemNew = nullptr; citemEntry = gameCreateItemNew;
            missingHook = "CreateItemNew";
            g_HhForced = true;
            installDetection();
            require(!switchOn(1), "an unresolved CreateItemNew armed the gate");
            status = sigdropStatusLine();
            const std::string missing = " detect=CreateItemNew:not found";
            require(status.size() >= missing.size() && status.compare(status.size() - missing.size(), std::string::npos, missing) == 0,
                    "`detect=` does not say CreateItemNew was not found: " + status);
        } else if (test == "refusal_latches_item_off") {
            // The first refused rewrite latches that item off for the session: no more copies of it
            // are pushed, the gate reads off with `rewrite refused: <why>`, and `refused=` counts it.
            // The other item, not refused, stays on (the positive control).
            setList(vanillaTriples(false));
            g_HhForced = true; g_TyForced = true;
            installDetection();
            require(switchOn(0) && switchOn(1), "both switches on did not arm both items");
            frozenField = "b";          // the record's b will not take the write
            pickPolicy = Pick::Last;    // the belt's copy, pushed after the crown's
            roll(monster, { true });
            require(g_SigOurHits == 1 && anyLineHas("inject: Headhunter refused", "field b did not read back"),
                    "the refused rewrite was not logged: " + lastLine("inject:"));
            require(builds.size() == 1 && builds[0].t == 8 && builds[0].a == kLootGroundSeed && builds[0].b == 51 && builds[0].c == 1.0 && g_SigBuilt == 0,
                    "a refused rewrite did not leave the game's own Liquor Holster");
            require(anyLineHas("angelic hit:", "refused (field b did not read back), the game placed its own stand-in"), "the hit line does not say it was refused");
#ifdef HAS_SIGNATUREOFFREASON
            require(SignatureOffReason(1) == "rewrite refused: field b did not read back", "the off reason is not the refusal: " + SignatureOffReason(1));
#else
            require(false, "no off reason to name the refusal");
#endif
            require(!switchOn(1), "a refused item stayed on");
            require(switchOn(0), "the latch took the other item off too");
            std::string status = sigdropStatusLine();
            require(status.find(" ourHits=1 refused=1 ") != std::string::npos && status.find(" gate=tyrant:on,headhunter:off ") != std::string::npos,
                    "`sigdrop status` does not count refused=1 with Headhunter off: " + status);
            // For the rest of the session nothing of Headhunter's is pushed, the write now works or not.
            frozenField.clear();
            listDuringCall.clear();
            roll(monster, { false });
            require(listDuringCall.size() == 1 && listDuringCall[0] == plus(vanilla, { kCrownStandIn }),
                    "a latched item was pushed again: " + (listDuringCall.empty() ? std::string("no roll") : describe(listDuringCall.back())));
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn };
            for (int i = 0; i < 50; ++i) roll(monster, { true });
            for (const auto& seen : listDuringCall)
                require(std::find(seen.begin(), seen.end(), kBeltStandIn) == seen.end(), "a latched item's stand-in was on the list during a roll");
            require(g_SigOurHits == 1 && countBuilds(1) == 0, "a latched item took a hit");
            status = sigdropStatusLine();
            require(status.find(" refused=1 ") != std::string::npos, "refused= moved without a refusal: " + status);
            require(listTriples() == vanilla, "the list is not the game's own after the rolls");
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
        // ---- the #74 review: a pool build that comes back empty is not kept ----
        // Both fail against the plugin before the fix, which latched the first build however it
        // came out, so a switch applied at the main menu left both stand-ins unresolved all session.
        } else if (test == "empty_pool_retried_on_a_later_roll") {
#ifdef HAS_KANGELICBASES
            // The panel's auto-apply at the main menu: both switches on (each switch-on installs the
            // detection and resolves the stand-ins) before the game's unique data is there.
            useGamePool(false);
            g_HhForced = true; g_TyForced = true;
            installDetection();
            const int firstTry = poolRepoReads;
            require(firstTry > 0, "the switch-on did not try to build the pool");
            installDetection();
            require(poolRepoReads == firstTry, "the second switch-on, inside 5 s, built the pool again");
            require(!switchOn(0) && !switchOn(1), "a stand-in resolved against a build with no unique data");
            require(linesStartingWith("angelic pool:") == 1 && anyLineHas("angelic pool:", ": 0 candidates"),
                    "the empty build was not logged once: " + lastLine("angelic pool:"));
            // Still not loaded 5 s later: the next roll tries again, and says nothing.
            fakeNowMs += 5000;
            roll(monster, { false });
            const int secondTry = poolRepoReads;
            require(secondTry > firstTry, "an empty pool was never built again");
            require(linesStartingWith("angelic pool:") == 1, "a retry logged the empty pool again");
            // In a zone, the data loaded: a roll inside the throttle builds nothing and pushes nothing.
            uniqueDataReady = true;
            setList(vanillaTriples(false));   // n = 0: the pushed entry is the only Liquor Holster
            pickPolicy = Pick::Last;
            fakeNowMs += 4999;
            roll(monster, { false });
            require(poolRepoReads == secondTry, "the pool was built again sooner than 5 s after the last try");
            require(listDuringCall.back() == vanilla, "a roll pushed an entry before the stand-ins resolved: " + describe(listDuringCall.back()));
            // The first roll 5 s after the last try builds the pool, resolves both stand-ins and pushes
            // them. The build runs inside the game's roll: were InitItemFromJson to call
            // CreateDefaultParams itself (not established), none of its calls may count as a hit.
            initItemAlsoCalls = [] {
                RValue sub(0.0), b(1.0), c(1.0), params;
                RValue* args[] = { &sub, &b, &c };
                cdpEntry(nullptr, nullptr, params, 3, args);
            };
            fakeNowMs += 1;
            roll(monster, { true });
            require(g_SigGameHits == 1 && g_SigUntyped == 0,
                    "the pool build inside the roll was taken for a hit: gameHits=" + std::to_string(g_SigGameHits) + " untyped=" + std::to_string(g_SigUntyped));
            require(switchOn(0) && switchOn(1), "the stand-ins did not resolve on a roll after the unique data loaded");
            require(listDuringCall.back() == plus(vanilla, { kCrownStandIn, kBeltStandIn }),
                    "the roll after the pool built did not carry both stand-ins: " + describe(listDuringCall.back()));
            require(g_SigOurHits == 1 && countBuilds(1) == 1, "the hit on the pushed entry was not built as Headhunter");
            require(linesStartingWith("angelic pool:") == 2 && lastLine("angelic pool:").find(": 0 candidates") == std::string::npos,
                    "the build that found candidates was not logged once: " + lastLine("angelic pool:"));
            // A pool that found candidates is kept: no build after it, however long the session.
            const int kept = poolRepoReads;
            fakeNowMs += 600000;
            roll(monster, { false });
            require(poolRepoReads == kept, "a pool that found candidates was built again");
#else
            require(false, "this source has no kAngelicBases for the model's unique data");
#endif
        } else if (test == "empty_pool_retried_on_a_later_switch_on") {
#ifdef HAS_KANGELICBASES
            useGamePool(false);
            g_HhForced = true;
            installDetection();   // `headhunter force` at the main menu
            require(!switchOn(1), "the stand-in resolved against a build with no unique data");
            uniqueDataReady = true;
            fakeNowMs += 5000;
            installDetection();   // `headhunter force` again, in a zone
            require(switchOn(1), "a later switch-on did not resolve the stand-in once the unique data loaded");
            require(anyLineHas("signature drops:", "Headhunter:Liquor Holster"), "the later switch-on did not name the stand-in");
            setList(vanillaTriples(false));
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(g_SigOurHits == 1 && countBuilds(1) == 1, "the hit on the pushed entry was not built as Headhunter");
            // `angeliclist` still rebuilds at once and verbosely, inside the throttle, and keeps the pool.
            const size_t pool = g_AngelicPool.size();
            const int listed = linesStartingWith("angeliclist:");
            g_AngelicPoolBuilt = false; BuildAngelicPool(true);
            require(g_AngelicPoolBuilt && g_AngelicPool.size() == pool, "`angeliclist` did not rebuild the same pool at once");
            require(linesStartingWith("angeliclist:") - listed == (int)std::size(kAngelicBases), "`angeliclist` did not list every row");
#else
            require(false, "this source has no kAngelicBases for the model's unique data");
#endif
        } else return 2;
        std::cout << "PASS " << test << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << test << ": " << error.what() << '\n';
        return 1;
    }
}
