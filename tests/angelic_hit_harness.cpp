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
#include <cctype>
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
// The flight charms' decision core (#158): the drop adapter's stand-ins, coin and stat line. Game-
// independent by contract, so it compiles here whole.
#include <ForgePact/FlightCharms.hpp>
namespace FcNs = ForgePact::FlightCharms;
// The Mark 1 drops' decision core (#166) and the set's, which it arms the art of: game-independent
// by the same contract. The pieces' icon table is not compiled in; its count is all the drop
// adapter reads of it (the art-ready test), so a stand-in with icons embedded takes its place.
#include <ForgePact/Mark1Drops.hpp>
namespace M1sNs = ForgePact::Mark1Set;
namespace M1dNs = ForgePact::Mark1Drops;
namespace ForgePact::Mark1Icons { inline constexpr size_t kIconCount = 8; }

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
// A VALUE_BOOL, as the runtime's bool constructor makes one (#166 replan 1: DropItem passes its
// builds' class this way). ToDouble refuses it like every other non-numeric kind; ToBoolean reads it.
static RValue makeBool(bool v) { RValue r; r.m_Kind = VALUE_BOOL; r.number = v ? 1.0 : 0.0; return r; }
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
inline constexpr std::string_view gml_Script_DropUniqueItems = "gml_Script_DropUniqueItems";
inline constexpr std::string_view gml_Script_DropItemHeroic = "gml_Script_DropItemHeroic";
inline constexpr std::string_view gml_Script_LoadSatanicDropTier = "gml_Script_LoadSatanicDropTier";
}
// What DropManager.hpp (#166's kill draw: DropManager's one DropItem detour, spliced in below the
// production types) names beside the plugin's own: reward scopes never active here, its research
// macros empty, and the incident monitor's scope and guard as the bare call (the header is absent
// here, as in drop_gold_harness.cpp).
namespace HeroSiege::RewardScope { inline bool Active() { return false; } }
#define BP_ANGELIC_PROBE_SCOPE(name, s, argc, a) ((void)0)
#define BP_DIAG_INCREMENT(counter) ((void)0)
#define BP_LOGDROP(name, res, argc, argv) ((void)0)
#ifndef _MSC_VER
#define sprintf_s(buf, ...) std::snprintf(buf, sizeof(buf), __VA_ARGS__)
#endif
#define FP_GAME_ORIGINAL(call) (call)
namespace IncidentMod { inline constexpr int drops = 0; }
struct IncidentScope { explicit IncidentScope(int) {} };
namespace HeroSiege::Objects {
enum class GameObject { Loot_Ground_obj = 902, Controller_obj = 984 };
inline constexpr std::string_view GetObjectName(GameObject o) { return o == GameObject::Controller_obj ? "Controller_obj" : "Loot_Ground_obj"; }
}
static constexpr const char* SdkShortScriptName(std::string_view sdkConstant) { return sdkConstant.data() + 11; }

// ---- log ---------------------------------------------------------------------------------
static std::vector<std::string> outLines;
static void Out(const std::string& line) { outLines.push_back(line); }
static std::string Lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }
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
// The definition's itemBaseInfoStruct per (type, sub, b), for the triples a flight scenario names:
// its rarity ("27") and whether it is flagged hidden ("40"). The unique roll keeps an entry only
// when the rarity is below 7 and it is not hidden (RDM § 23.4, § 23.8); so does the elf's filter.
// The kill draw's scenarios (#166) also give a triple its tier ("32", none below 0) and field 41,
// which DropItem's own draw reads beside 40 (RDM § 13.12, static reading).
struct RepoInfo { double rarity; bool hidden; double tier = -1.0; bool f41 = false; };
static std::map<Triple, RepoInfo> repoInfo;
static RValue repoInfoStruct(const RepoInfo& i) {
    std::map<std::string, RValue> f{ { "27", RValue(i.rarity) }, { "40", RValue(i.hidden ? 1.0 : 0.0) } };
    if (i.tier >= 0.0) f["32"] = RValue(i.tier);
    if (i.f41) f["41"] = RValue(1.0);
    return makeStruct(std::move(f));
}
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
// ds_list_add throws once, after this many more calls succeed (-1: never), the way a builtin the
// runtime refuses would; then it is -1 again. A partial push is the case it models.
static int dsListAddThrowAfter = -1;
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
        if (n == "ds_list_add") {
            if (dsListAddThrowAfter == 0) { dsListAddThrowAfter = -1; throw std::runtime_error("ds_list_add refused"); }
            if (dsListAddThrowAfter > 0) --dsListAddThrowAfter;
            dsList(a[0]).push_back(a[1]);
            return RValue();
        }
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
        const auto info = repoInfo.find(t);
        if (it == repoBase.end() && info == repoInfo.end()) return RValue();
        std::map<std::string, RValue> f;
        if (it != repoBase.end()) f["droprate"] = makeStruct({ { "base", RValue(it->second) } });
        if (info != repoInfo.end()) f["itemBaseInfoStruct"] = repoInfoStruct(info->second);
        return makeStruct(std::move(f));
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
// raredrop's one LoadSatanicDropTier detour (#166, the kill draw's tier) and the globals beside it in
// ModuleMain.cpp: the lift off (x1, a pass-through), its saved original, and the one install's route.
static PFUNC_YYGMLScript g_OrigSatTier = nullptr;
static double g_SatanicMult = 1.0;
static constexpr double kSatanicTopBracket = 200.0;
static volatile long g_SatTierHits = 0;
static bool g_SatTierTried = false;
static std::string g_SatTierRoute = "not-installed";
// The kill draw's call-outs (#166), which hooks defined before its adapter call, as ModuleMain.cpp
// declares them.
static void KillDrawNoteRead(int argc, RValue** A);
static bool KillDrawTypePick(int argc, RValue** A);
static void KillDrawCheckClass(const RValue& made);
static void KillDrawSettleHit();
static void KillDrawNoteTier(const RValue& result);
static bool KillDrawBeforeCreate(int argc, RValue** A);
static bool KillDrawHoldsDropItem();
static void KillDrawEnterRoll();
static void KillDrawLeaveRoll();
static bool KillDrawNoteBuiltElf();
static std::string KillDrawInstall();
static void KillDrawElfFillStat(FcNs::StatFields& s);
static bool KillDrawNoteBuiltPiece(M1sNs::Piece piece);
static std::string KillDrawRoute();
static void KillDrawMark1FillStat(M1dNs::DropStatFields& f);

// PRODUCTION_TYPES

#ifdef HAS_SIGNATURESTANDIN
static SignatureStandIn g_SigStandIn[2];
#endif
#ifdef HAS_SIGNATURENOTEBUILT
static void SignatureNoteBuilt(const std::map<std::string, double>& selector);
#endif
#ifdef HAS_FLIGHTDROPSTATE
// The flight charms' drop adapter (#158): its state and the globals beside it, as ModuleMain.cpp
// declares them (the copies a plain int here, as the research build's lever leaves it), and the
// functions #74's machinery calls before they are defined.
static FlightDropState g_FlightDrop;
static PFUNC_YYGMLScript g_Orig_FlightDropUnique = nullptr;
static int g_FlightDropCopies = 1;
static void FlightDropHitReset();
static void FlightDropAngelicPush();
static void FlightDropAngelicRemove();
static void FlightDropAttributeHit();
static void FlightDropBeforeCreate(RValue& record, const std::string& missing);
static void FlightDropAfterHit();
static void FlightDropNoteBuilt(FcNs::Charm charm);
#endif
// The shared ground-icon hook a drop switch installs (hidden loot's one LootGroundInit detour),
// counted here: the model has no ground.
static int groundIconInstalls = 0;
static void FlightGroundIconInstall() { ++groundIconInstalls; }

// The Mark 1 set's state the drop adapter (#166) reads: the set's core (whose art flag the drops'
// switch-on arms), whether the pieces' one ground install was tried, setup, and whether the
// local player resolves (false: character select). Mark1GroundIconInstall is the set adapter's,
// as ModuleMain.cpp writes it: tried once a session, only while the art is armed.
static ForgePact::Mark1Set::Bonus g_Mark1;
static bool g_M1sGroundTried = false;
static bool g_Setup = true;
static bool playerResolves = true;
static int mark1GroundInstalls = 0;
static bool Mark1PlayerResolves() { return playerResolves; }
static void Mark1GroundIconInstall() {
    if (!M1sNs::ArtGroundInstallPending(g_Mark1.ArtArmed(), (int)ForgePact::Mark1Icons::kIconCount, g_M1sGroundTried)) return;
    g_M1sGroundTried = true;
    ++mark1GroundInstalls;
    FlightGroundIconInstall();
}
#ifdef HAS_MARK1DROPSTATE
// The Mark 1 drop adapter's state and the globals beside it, as ModuleMain.cpp declares them (the
// copies a plain int here, as the research build's lever leaves it), and the functions #74's
// machinery calls before they are defined.
static Mark1DropState g_M1Drop;
static PFUNC_YYGMLScript g_Orig_Mark1DropHeroic = nullptr;
static int g_Mark1DropCopies = 1;
static void Mark1DropHitReset();
static void Mark1DropAttributeHit();
static bool Mark1DropBeforeCreate(RValue& record, const std::string& missing);
static void Mark1DropNoteBuilt(M1sNs::Piece piece);
#endif
#ifdef HAS_MARK1KILLINJECTGUARD
// The kill route's part of the DropUniqueItems detour Pocket Elf shares, which calls it before the
// Mark 1 drop adapter defines it, as ModuleMain.cpp declares it.
static bool Mark1KillBeginCall(int uniqueDepth);
static void Mark1KillEnter();
static void Mark1KillLeave();
static void Mark1DropAfterHit();
#endif
#ifdef HAS_KILLDRAWSTATE
// The kill draw's state (#166), as ModuleMain.cpp declares it.
static KillDrawState g_KillDraw;
static FcNs::ElfDraw g_ElfDraw;
static FcNs::ElfDrawCache g_ElfDrawCache;
#endif

// ---- the game: CreateDefaultParams, the item builder, the script table, DropItemAngelicChance ---
// CreateDefaultParams(sub, b, c) returns the struct Live 2 measured, {"b":51.0,"j":0.0,"c":1.0}:
// `j` the first argument, `b` the second, and `c` a real 1 or 0, the runner's truth test of the
// third (absent or undefined counts as 0; #166 replan 1's static reading, hub RDM § 13.12), never
// the argument itself; no field a and no w. The built item's `a` is LootGroundCreate's, below.
// The runner's truth test: a real or a bool true above 0.5, an int32 or a reference above 0, an
// int64 above 0, a pointer when non-zero, undefined false. A string or another kind is a runner
// error in the game; the model reads it as false and counts it apart from refusedConversions,
// which counts the plugin's own conversions.
static int gameTruthErrors = 0;
static bool gameTruth(const RValue& v) {
    switch (v.m_Kind) {
    case VALUE_REAL: case VALUE_BOOL: return v.number > 0.5;
    case VALUE_INT32: case VALUE_INT64: case VALUE_REF: return v.number > 0;
    case VALUE_PTR: return v.number != 0;
    case VALUE_UNDEFINED: case VALUE_UNSET: return false;
    default: ++gameTruthErrors; return false;
    }
}
// What a scenario makes the fake game store as `c`: the truth of the argument (the game's own), its
// opposite, or no `c` at all (the class check's disagree and unread cases).
enum class CdpClassStore { Truth, Opposite, Missing };
static CdpClassStore cdpClassStore = CdpClassStore::Truth;
static int gameCdpCalls = 0;
static bool paramsLackJ = false;   // a CreateDefaultParams whose struct has no `j`
static std::vector<std::string> lastParamsFields;   // the field names the last call returned
static RValue& gameCreateDefaultParams(CInstance*, CInstance*, RValue& result, int argc, RValue** A) {
    ++gameCdpCalls;
    std::map<std::string, RValue> f;
    if (argc > 0 && !paramsLackJ) f["j"] = *A[0];
    if (argc > 1) f["b"] = *A[1];
    const bool truth = argc > 2 && A[2] && gameTruth(*A[2]);
    if (cdpClassStore != CdpClassStore::Missing) f["c"] = RValue((truth != (cdpClassStore == CdpClassStore::Opposite)) ? 1.0 : 0.0);
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
        const Triple t{ A[0]->number, A[1]->number, A[2]->number };
        const auto it = repoBase.find(t);
        if (it != repoBase.end()) result = makeStruct({ { "droprate", makeStruct({ { "base", RValue(it->second) } }) } });
        const auto info = repoInfo.find(t);
        if (info != repoInfo.end()) obj(result)["itemBaseInfoStruct"] = repoInfoStruct(info->second);
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
// #166: where the game's own calls to DropItemHeroic land (its function, below, or a detoured hook
// on it), and a scenario's lever that makes the Angelic roll call it once, nested, before its die.
static PFUNC_YYGMLScript heroicEntry = nullptr;
static bool angelicNestsHeroic = false;
// The same lever for the ordinary unique roll (the Mark 1 kill route): the Angelic roll calls
// DropUniqueItems once, nested, through wherever the game's calls to it land (uniqueEntry, below).
static bool angelicNestsUnique = false;
static void nestedUniqueCall(CInstance* self, CInstance* other);
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
    if (angelicNestsHeroic && heroicEntry) {
        RValue hx(640.0), hy(320.0), hr;
        RValue* hargs[] = { &hx, &hy };
        heroicEntry(self, other, hr, 2, hargs);
        if (repoRead != RepoRead::Skip) readDefinition(self, other, picked);   // the roll's own read is the last again
    }
    if (angelicNestsUnique) {
        nestedUniqueCall(self, other);
        if (repoRead != RepoRead::Skip) readDefinition(self, other, picked);   // the roll's own read is the last again
    }
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

// DropUniqueItems(x, y), the ordinary unique roll (RDM § 23.8, static reading): element `uniqueK`
// (0..4, the zone value) of the same Controller_obj variable, one index drawn (the picker above),
// and only when that entry is an array its definition read through GetUniqueRepoStruct by direct
// call; kept when the rarity is below 7 and it is not hidden, then CreateDefaultParams(sub, b, 1)
// by direct call and the placement under the entry's type. A failed pick drops nothing: no second
// draw and no die.
static int uniqueK = 2;
static int uniqueCalls = 0;
static bool uniqueThrows = false;
static std::vector<std::vector<std::vector<Triple>>> uniqueLayoutDuringCall;   // every element, per call
static RValue uniqueReturn;
static RValue& gameDropUniqueItems(CInstance* self, CInstance* other, RValue&, int, RValue**) {
    ++uniqueCalls;
    uniqueLayoutDuringCall.push_back(layoutNow());
    if (uniqueThrows) throw std::runtime_error("the game's unique roll threw");
    uniqueReturn = RValue();
    RValue* v = listVariable();
    if (!v || v->m_Kind != VALUE_ARRAY || v->array->size() <= (size_t)uniqueK) return uniqueReturn;
    std::vector<RValue>* list = dsFind((*v->array)[(size_t)uniqueK]);
    if (!list || list->empty()) return uniqueReturn;
    size_t index = 0;
    if (pickPolicy == Pick::Last) index = list->size() - 1;
    else if (pickPolicy == Pick::Among) {
        std::vector<size_t> candidates;
        for (size_t i = 0; i < list->size(); ++i)
            if (std::find(pickTriples.begin(), pickTriples.end(), tripleOf((*list)[i])) != pickTriples.end()) candidates.push_back(i);
        if (!candidates.empty()) index = candidates[std::uniform_int_distribution<size_t>(0, candidates.size() - 1)(pickRng)];
    }
    if ((*list)[index].m_Kind != VALUE_ARRAY) return uniqueReturn;
    const Triple picked = tripleOf((*list)[index]);
    RValue type(picked[0]), sub(picked[1]), b(picked[2]), definition;
    RValue* readArgs[] = { &type, &sub, &b };
    RValue& def = repoEntry(self, other, definition, 3, readArgs);
    double rarity = 99.0;
    bool hidden = false;
    if (def.m_Kind == VALUE_OBJECT && def.fields->count("itemBaseInfoStruct")) {
        auto& info = obj((*def.fields)["itemBaseInfoStruct"]);
        rarity = info["27"].number;
        hidden = info["40"].number != 0.0;
    }
    if (repoRead == RepoRead::PickedThenOther) readDefinition(self, other, kOtherRead);   // a read the hit cannot be typed from
    if (!(rarity < 7.0) || hidden) return uniqueReturn;
    RValue c(1.0), params;
    RValue* args[] = { &sub, &b, &c };
    RValue& made = cdpEntry(self, other, params, 3, args);
    gameLootGroundCreate(picked[0], made);
    return uniqueReturn;
}
// Where the game's own calls to DropUniqueItems land: its function, or a detoured hook on it.
static PFUNC_YYGMLScript uniqueEntry = gameDropUniqueItems;
static void nestedUniqueCall(CInstance* self, CInstance* other) {
    RValue ux(640.0), uy(320.0), ur;
    RValue* uargs[] = { &ux, &uy };
    uniqueEntry(self, other, ur, 2, uargs);
}

// DropItemHeroic(x, y, k), the Heroic roll (#166; hub RDM § 23.4 and the drops workorder's static
// reading): element k of the same Controller_obj variable, k the third argument and 5 when it is
// absent or undefined. It draws an entry and draws again until it keeps one: an entry that is not
// an array is drawn past; a triple's definition is read through GetUniqueRepoStruct by direct call
// (one read per draw, so the last read before the hit is the kept entry's) and kept when it is not
// hidden and its rarity is neither 7 nor 10. Then CreateDefaultParams(sub, b, 1) once, by direct
// call, and the placement under the entry's own type. A triple the model's repository does not
// know is drawn past too. The draws follow pickPolicy: First walks the list from the front, Last
// from the back, Among draws uniformly among the entries equal to one of pickTriples, and any
// other entry uniformly when none is there.
static int heroicCalls = 0;
static bool heroicThrows = false;
static std::vector<std::vector<std::vector<Triple>>> heroicLayoutDuringCall;   // every element, per call
static std::vector<int> heroicListRead;                                         // the element each call read
static RValue heroicReturn;
static RValue& gameDropItemHeroic(CInstance* self, CInstance* other, RValue&, int argc, RValue** A) {
    ++heroicCalls;
    heroicLayoutDuringCall.push_back(layoutNow());
    if (heroicThrows) throw std::runtime_error("the game's Heroic roll threw");
    heroicReturn = RValue();
    size_t k = 5;
    if (argc > 2 && A && A[2] && A[2]->m_Kind == VALUE_REAL && A[2]->number >= 0) k = (size_t)A[2]->number;
    heroicListRead.push_back((int)k);
    RValue* v = listVariable();
    if (!v || v->m_Kind != VALUE_ARRAY || v->array->size() <= k) return heroicReturn;
    std::vector<RValue>* list = dsFind((*v->array)[k]);
    if (!list || list->empty()) return heroicReturn;
    std::vector<size_t> among;
    if (pickPolicy == Pick::Among)
        for (size_t i = 0; i < list->size(); ++i)
            if (std::find(pickTriples.begin(), pickTriples.end(), tripleOf((*list)[i])) != pickTriples.end()) among.push_back(i);
    for (size_t draw = 0; draw < 100000; ++draw) {
        size_t index = 0;
        if (pickPolicy == Pick::First) index = draw % list->size();
        else if (pickPolicy == Pick::Last) index = list->size() - 1 - draw % list->size();
        else if (!among.empty() && draw == 0) index = among[std::uniform_int_distribution<size_t>(0, among.size() - 1)(pickRng)];
        else index = std::uniform_int_distribution<size_t>(0, list->size() - 1)(pickRng);
        if ((*list)[index].m_Kind != VALUE_ARRAY) continue;
        const Triple picked = tripleOf((*list)[index]);
        RValue type(picked[0]), sub(picked[1]), b(picked[2]), definition;
        RValue* readArgs[] = { &type, &sub, &b };
        RValue& def = repoEntry(self, other, definition, 3, readArgs);
        if (def.m_Kind != VALUE_OBJECT || !def.fields->count("itemBaseInfoStruct")) continue;
        auto& info = obj((*def.fields)["itemBaseInfoStruct"]);
        const double rarity = info["27"].number;
        if (info["40"].number != 0.0 || rarity == 7.0 || rarity == 10.0) continue;
        if (repoRead == RepoRead::PickedThenOther) readDefinition(self, other, kOtherRead);   // a read the hit cannot be typed from
        RValue c(1.0), params;
        RValue* args[] = { &sub, &b, &c };
        RValue& made = cdpEntry(self, other, params, 3, args);
        gameLootGroundCreate(picked[0], made);
        return heroicReturn;
    }
    throw std::runtime_error("the model's Heroic roll kept nothing in 100000 draws");
}

// LoadSatanicDropTier(level): the tier DropItem draws its unique from (a whole number 0 to 5 by the
// monster's level, static reading); here whatever a scenario sets, a number or not.
static RValue satTierAnswer = RValue(3.0);
static int satTierCalls = 0;
static RValue satTierReturn;
static RValue& gameLoadSatanicDropTier(CInstance*, CInstance*, RValue&, int, RValue**) {
    ++satTierCalls;
    satTierReturn = satTierAnswer;
    return satTierReturn;
}
// Where DropItem's direct call lands: the game's function, or a detoured hook on it.
static PFUNC_YYGMLScript satTierEntry = gameLoadSatanicDropTier;

// DropItem(... twelve arguments), the kill's own drop (#166; hub RDM § 13.12, static reading): the
// tier from LoadSatanicDropTier by direct call; an ordinary item when a scenario asks
// (CreateDefaultParams(0, 900, 0), never typed by a unique read); the nested rolls a scenario asks for,
// each through wherever the game's calls to it land (#74's Angelic roll, its outcome the next of
// `outcomes`, and DropUniqueItems); then `dropItemDraws` unique draws from lootListUnique[k], k the
// tier (0 when it is not a number): an index drawn (pickPolicy), drawn again unless the entry is an
// array, its definition read through GetUniqueRepoStruct by direct call, drawn again on a set field
// 40 or 41 or a rarity other than Satanic (6), at most 500 draws; then CreateDefaultParams(sub, b, true)
// and the placement under the entry's type. The layout each draw reads is recorded. A scenario can
// also have a kept pick's own pair built as an ordinary item first (`dropItemNormalAtPick`:
// CreateDefaultParams(sub, b, false) between the pick's read and its unique build), the worst case
// of a normal build that matches the latest unique read. The class is a VALUE_BOOL on every build,
// true on the unique one and false on the normal one (#166 replan 1's static reading, hub RDM §
// 13.12); `dropItemUniqueClass` / `dropItemUniqueArgc` let a scenario pass the unique build's class
// as another kind, or not at all, and `cdpReturnedOther` counts a unique build whose
// CreateDefaultParams handed back anything but the record the game's own call made.
static int dropItemCalls = 0;
static bool dropItemThrows = false;
static int dropItemDraws = 1;
static bool dropItemNormal = false;
static bool dropItemNormalAtPick = false;
static RValue dropItemUniqueClass = makeBool(true);
static RValue dropItemNormalClass = makeBool(false);
static int dropItemUniqueArgc = 3;
static int cdpReturnedOther = 0;
static bool dropItemNestsAngelic = false, dropItemNestsUnique = false;
static PFUNC_YYGMLScript angelicEntry = nullptr;   // the game's calls to DropItemAngelicChance: HookAngelicChance (set in reset)
static int dropItemArgcSeen = -1;
static std::vector<RValue*> dropItemArgsSeen;
static std::vector<int> dropItemTiers;
static std::vector<std::vector<std::vector<Triple>>> dropItemLayoutAtDraw;
static RValue dropItemReturn;
static RValue& gameDropItem(CInstance* self, CInstance* other, RValue&, int argc, RValue** A) {
    ++dropItemCalls;
    dropItemArgcSeen = argc;
    dropItemArgsSeen.assign(A, A + argc);
    if (dropItemThrows) throw std::runtime_error("the game's DropItem threw");
    dropItemReturn = RValue();
    RValue level(60.0), tierResult;
    RValue* tierArgs[] = { &level };
    const RValue& tier = satTierEntry(self, other, tierResult, 1, tierArgs);
    const size_t k = tier.m_Kind == VALUE_REAL && tier.number >= 0 && tier.number < 6 ? (size_t)tier.number : 0;
    dropItemTiers.push_back((int)k);
    if (dropItemNormal) {
        RValue sub(0.0), b(900.0), c = dropItemNormalClass, params;
        RValue* args[] = { &sub, &b, &c };
        RValue& made = cdpEntry(self, other, params, 3, args);
        gameLootGroundCreate(6, made);
    }
    if (dropItemNestsAngelic && angelicEntry) {
        RValue a0(640.0), a1(320.0), a2(1500.0), a3, r;
        RValue* args[] = { &a0, &a1, &a2, &a3 };
        angelicEntry(self, other, r, 4, args);
    }
    if (dropItemNestsUnique) nestedUniqueCall(self, other);
    for (int i = 0; i < dropItemDraws; ++i) {
        dropItemLayoutAtDraw.push_back(layoutNow());
        RValue* v = listVariable();
        if (!v || v->m_Kind != VALUE_ARRAY || v->array->size() <= k) continue;
        std::vector<RValue>* list = dsFind((*v->array)[k]);
        if (!list || list->empty()) continue;
        std::vector<size_t> among;
        if (pickPolicy == Pick::Among)
            for (size_t e = 0; e < list->size(); ++e)
                if (std::find(pickTriples.begin(), pickTriples.end(), tripleOf((*list)[e])) != pickTriples.end()) among.push_back(e);
        for (size_t draw = 0; draw < 500; ++draw) {
            size_t index = 0;
            if (pickPolicy == Pick::First) index = draw % list->size();
            else if (pickPolicy == Pick::Last) index = list->size() - 1 - draw % list->size();
            else if (!among.empty() && draw == 0) index = among[std::uniform_int_distribution<size_t>(0, among.size() - 1)(pickRng)];
            else index = std::uniform_int_distribution<size_t>(0, list->size() - 1)(pickRng);
            if ((*list)[index].m_Kind != VALUE_ARRAY) continue;
            const Triple picked = tripleOf((*list)[index]);
            RValue type(picked[0]), sub(picked[1]), b(picked[2]), definition;
            RValue* readArgs[] = { &type, &sub, &b };
            RValue& def = repoEntry(self, other, definition, 3, readArgs);
            if (def.m_Kind != VALUE_OBJECT || !def.fields->count("itemBaseInfoStruct")) continue;
            auto& info = obj((*def.fields)["itemBaseInfoStruct"]);
            if (info["40"].number != 0.0 || (info.count("41") && info["41"].number != 0.0) || info["27"].number != 6.0) continue;
            if (repoRead == RepoRead::PickedThenOther) readDefinition(self, other, kOtherRead);   // a read the pick cannot be typed from
            if (dropItemNormalAtPick) {
                RValue normalC = dropItemNormalClass, normalParams;
                RValue* normalArgs[] = { &sub, &b, &normalC };
                RValue& normal = cdpEntry(self, other, normalParams, 3, normalArgs);
                gameLootGroundCreate(6, normal);
            }
            RValue c = dropItemUniqueClass, params;
            RValue* args[] = { &sub, &b, &c };
            RValue& made = cdpEntry(self, other, params, dropItemUniqueArgc, args);
            if (&made != &params) ++cdpReturnedOther;
            gameLootGroundCreate(picked[0], made);
            break;
        }
    }
    return dropItemReturn;
}
// Where the game's own calls to DropItem land: its function, or DropManager's detoured hook on it
// (which stays in for the process, as in the plugin, across a scenario's reset()).
static PFUNC_YYGMLScript dropItemEntry = gameDropItem;

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
    if (name == "DropUniqueItems") {
        *origOut = gameDropUniqueItems;
        if (native) uniqueEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest);
        else gameImage.push_back(reinterpret_cast<const void*>(gameDropUniqueItems));
        return true;
    }
    if (name == "DropItemHeroic") {
        *origOut = gameDropItemHeroic;
        if (native) heroicEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest);
        else gameImage.push_back(reinterpret_cast<const void*>(gameDropItemHeroic));
        return true;
    }
    if (name == "DropItem") {
        *origOut = gameDropItem;
        if (native) dropItemEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest);
        else gameImage.push_back(reinterpret_cast<const void*>(gameDropItem));
        return true;
    }
    if (name == "LoadSatanicDropTier") {
        *origOut = gameLoadSatanicDropTier;
        if (native) satTierEntry = reinterpret_cast<PFUNC_YYGMLScript>(dest);
        else gameImage.push_back(reinterpret_cast<const void*>(gameLoadSatanicDropTier));
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

// The real plugin/include/ForgePact/DropManager.hpp (#166: the kill draw's DropItem detour is its
// Hook_DropItem, installed alone by InstallDropItemHook), its #pragma and #include lines removed.
// PRODUCTION_DROPMANAGER

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
#ifdef HAS_FLIGHTDROPNOTEBUILT
        // The Custom Forge's flight-charm row (#158): a built item whose own fields are a flight
        // charm's, reported as TryApplyCustomForge's final pass does.
        const RValue definition = obj(res)["itemDefinitionStruct"];
        FcNs::Rolled rolled;
        if (FcNs::IsFlightCharm(obj(res)["itemType"].number, field(definition, "a"), field(definition, "b"),
                                field(definition, "c"), field(definition, "j"), rolled))
            FlightDropNoteBuilt(rolled.charm);
#endif
#ifdef HAS_MARK1DROPNOTEBUILT
        // The Custom Forge's Mark 1 rows (#166): a built item whose own fields are a piece's
        // (Mark1Set.hpp's IsPiece), reported as TryApplyCustomForge's final pass does.
        const RValue pieceDefinition = obj(res)["itemDefinitionStruct"];
        M1sNs::Piece piece = M1sNs::Piece::Helmet;
        if (M1sNs::IsPiece(obj(res)["itemType"].number, field(pieceDefinition, "a"), field(pieceDefinition, "b"),
                           field(pieceDefinition, "c"), field(pieceDefinition, "j"), piece))
            Mark1DropNoteBuilt(piece);
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
    g_Orig_CreateItemNew = gameCreateItemNew; citemEntry = Hook_CreateItemNew; frozenField.clear(); dsListAddThrowAfter = -1;
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
    repoInfo.clear(); groundIconInstalls = 0;
    uniqueEntry = gameDropUniqueItems; uniqueK = 2; uniqueCalls = 0; uniqueThrows = false; uniqueLayoutDuringCall.clear();
#ifdef HAS_FLIGHTDROPSTATE
    g_FlightDrop = FlightDropState{}; g_Orig_FlightDropUnique = nullptr; g_FlightDropCopies = 1;
#endif
    g_Mark1 = ForgePact::Mark1Set::Bonus{}; g_M1sGroundTried = false; g_Setup = true; playerResolves = true; mark1GroundInstalls = 0;
    heroicEntry = gameDropItemHeroic; angelicNestsHeroic = false; angelicNestsUnique = false;
    heroicCalls = 0; heroicThrows = false; heroicLayoutDuringCall.clear(); heroicListRead.clear();
#ifdef HAS_MARK1DROPSTATE
    g_M1Drop = Mark1DropState{}; g_Orig_Mark1DropHeroic = nullptr; g_Mark1DropCopies = 1;
#endif
    // The kill draw (#166): the game's own DropItem and LoadSatanicDropTier, every lever off, and
    // raredrop's one tier detour not tried.
    satTierAnswer = RValue(3.0); satTierCalls = 0; satTierEntry = gameLoadSatanicDropTier;
    dropItemCalls = 0; dropItemThrows = false; dropItemDraws = 1; dropItemNormal = false; dropItemNormalAtPick = false;
    dropItemUniqueClass = makeBool(true); dropItemNormalClass = makeBool(false); dropItemUniqueArgc = 3; cdpReturnedOther = 0;
    cdpClassStore = CdpClassStore::Truth; gameTruthErrors = 0;
    dropItemNestsAngelic = false; dropItemNestsUnique = false; dropItemArgcSeen = -1; dropItemArgsSeen.clear();
    dropItemTiers.clear(); dropItemLayoutAtDraw.clear();
    g_OrigSatTier = nullptr; g_SatanicMult = 1.0; g_SatTierHits = 0; g_SatTierTried = false; g_SatTierRoute = "not-installed";
#ifdef HAS_HOOKANGELICCHANCE
    angelicEntry = HookAngelicChance;
#endif
#ifdef HAS_KILLDRAWSTATE
    g_KillDraw = KillDrawState{}; g_ElfDraw = FcNs::ElfDraw{}; g_ElfDrawCache = FcNs::ElfDrawCache{};
    // DropManager is a process-wide singleton, as in the plugin: its DropItem hook, once in, stays in
    // (the game's calls keep landing on it); an earlier install's splice is undone, so the next
    // install splices the game's own function again.
    if (PFUNC_YYGMLScript* slot = ForgePact::DropManager::Instance().DropItemOriginal(); slot && *slot) *slot = gameDropItem;
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

// ---- the flight charms' drops (#158) -------------------------------------------------------------
static const Triple kWaterMelon{ 10, 0, 47 };     // Gabriel Wings' stand-in, Angelic (7)
static const Triple kSoulCollector{ 10, 0, 91 };  // Witch's Broomstick's stand-in, Unholy (10)
static const Triple kAimbot{ 10, 0, 76 };         // Pocket Elf's first candidate, Satanic (6)
static const Triple kWitchesClaw{ 10, 0, 41 };
static const Triple kMapJournal{ 10, 0, 24 };
static const Triple kCrowsFeather{ 10, 0, 68 };
static const Triple kSteve{ 10, 0, 12 };
// The validated Angelic pool with the two Angelic stand-ins in it, and every candidate's definition
// as the repository measured it (Aimbot.exe 6, Water Melon 7, Soul Collector 10; the other four
// candidates' rarities are the model's).
static void flightRepo() {
    poolExtras.push_back({ 10, 0, 47, "Water Melon", true });
    poolExtras.push_back({ 10, 0, 91, "Soul Collector", false });
    for (const Triple& t : { kAimbot, kWitchesClaw, kMapJournal, kCrowsFeather, kSteve }) repoInfo[t] = { 6.0, false };
    repoInfo[kWaterMelon] = { 7.0, false };
    repoInfo[kSoulCollector] = { 10.0, false };
}
// lootListUnique[0..4] for the unique roll: twenty ordinary uniques of each sub-list's own
// (rarity 3) plus `extra[k]`, and [5] the Angelic list as setList builds it.
static void setUniqueLayout(const std::array<std::vector<Triple>, 5>& extra, const std::vector<Triple>& angelic = vanillaTriples()) {
    dsLists.clear(); nextDsId = 40;
    std::vector<RValue> ids;
    for (int k = 0; k < 5; ++k) {
        std::vector<Triple> l;
        for (int i = 0; i < 20; ++i) {
            const Triple t{ 4, 0, (double)(100 * k + i) };
            l.push_back(t);
            repoInfo[t] = { 3.0, false };
        }
        l.insert(l.end(), extra[(size_t)k].begin(), extra[(size_t)k].end());
        ids.push_back(dsHandle(dsCreate(entriesOf(l))));
    }
    ids.push_back(dsHandle(dsCreate(entriesOf(angelic))));
    controllerVars[kModelListName] = makeArray(std::move(ids));
    vanilla = angelic;
    vanillaIds = idsNow();
    vanillaLayout = layoutNow();
}
// One call of the unique roll as LoadDrops makes it: x, y.
static void dropUnique(CInstance& monster, double x = 640.5, double y = 320.25) {
    static RValue a0, a1, result;
    a0 = RValue(x);
    a1 = RValue(y);
    RValue* args[] = { &a0, &a1 };
    uniqueEntry(&monster, nullptr, result, 2, args);
}
// The built items that are a flight charm (by their own fields), and of which charm.
static int countFlightBuilds(int charm) {
    int n = 0;
    for (const auto& b : builds) {
        FcNs::Rolled r;
        if (FcNs::IsFlightCharm(b.t, b.a, b.b, b.c, b.j, r) && (charm < 0 || (int)r.charm == charm)) ++n;
    }
    return n;
}
static std::string flightStatLine() {
#ifdef HAS_FLIGHTDROPFILLSTAT
    FcNs::StatFields s;
    FlightDropFillStat(s);
#ifdef HAS_KILLDRAWELFFILLSTAT
    KillDrawElfFillStat(s);   // as FlightCharmStatFields fills the `elf-draw-` fields after the drops'
#endif
    return FcNs::StatLine(s);
#else
    return std::string();
#endif
}
static bool flightSwitch(FcNs::Charm c, bool on) {
#ifdef HAS_FLIGHTDROPSWITCH
    FlightDropSwitch(c, on);
    return true;
#else
    (void)c; (void)on;
    return false;
#endif
}
static FcNs::Gate flightGate(FcNs::Charm c) {
#ifdef HAS_FLIGHTDROPGATE
    return FlightDropGate(c);
#else
    (void)c;
    return FcNs::Gate::Off;
#endif
}
// ---- the Mark 1 drops (#166) ---------------------------------------------------------------------
// The Heroic roll's pool on the sub-list a scenario names (5 unless it says otherwise): the pieces'
// classes are helmet 0, body armor 1, boots 2, gloves 4 (Mark1Set.hpp's kPieces).
static const Triple kHelmetHeroic{ 0, 3, 10 };    // Heroic (9), listed twice: the helmet's stand-in, n 2
static const Triple kHelmetSatanic{ 0, 3, 11 };   // Satanic (6): kept by the roll, not preferred
static const Triple kBodyHeroic{ 1, 4, 20 };      // Heroic, once: the body armor's stand-in
static const Triple kBootsHeroic{ 2, 5, 30 };     // Heroic, once: the boots' stand-in
static const Triple kGlovesAngelic{ 4, 6, 40 };   // Angelic (7): the roll draws past it, so gloves have none
static const Triple kHiddenHeroic{ 5, 0, 99 };    // Heroic but flagged hidden: never kept
// What the Heroic roll keeps of the pool below: the piece rows' five entries and ten ordinary
// Heroic uniques of class 5. The vanilla Angelic list's own entries (rarity 7, Lucifer's Crown and
// Mask of the Celestial among them, class 0) are drawn past.
static std::vector<Triple> heroicOrdinary() {
    std::vector<Triple> v;
    for (int i = 0; i < 10; ++i) v.push_back({ 5, 0, (double)i });
    return v;
}
static std::vector<Triple> heroicPieceRows() {
    return { kHelmetHeroic, kHelmetHeroic, kHelmetSatanic, kBodyHeroic, kBootsHeroic, kGlovesAngelic, kHiddenHeroic };
}
static void heroicRepo() {
    for (const Triple& t : vanillaTriples()) repoInfo[t] = { 7.0, false };
    for (const Triple& t : heroicOrdinary()) repoInfo[t] = { 9.0, false };
    repoInfo[kHelmetHeroic] = { 9.0, false };
    repoInfo[kHelmetSatanic] = { 6.0, false };
    repoInfo[kBodyHeroic] = { 9.0, false };
    repoInfo[kBootsHeroic] = { 9.0, false };
    repoInfo[kGlovesAngelic] = { 7.0, false };
    repoInfo[kHiddenHeroic] = { 9.0, true };
}
// lootListUnique: elements 0..4 otherElements(), element 5 the vanilla Angelic list plus the
// ordinary Heroic uniques, and the piece rows on element `k` (5 by default) with ordinary Heroic
// uniques beside them there too.
static void setHeroicLayout(size_t k = 5, bool pieceRows = true) {
    heroicRepo();
    dsLists.clear(); nextDsId = 40;
    std::vector<std::vector<Triple>> elements = otherElements();
    elements.push_back(plus(vanillaTriples(), {}));
    for (const Triple& t : heroicOrdinary()) elements[5].push_back(t);
    if (k != 5) for (const Triple& t : heroicOrdinary()) elements[k].push_back(t);
    if (pieceRows) for (const Triple& t : heroicPieceRows()) elements[k].push_back(t);
    for (const auto& e : elements) for (const Triple& t : e) if (!repoInfo.count(t)) repoInfo[t] = { 3.0, false };
    std::vector<RValue> ids;
    for (const auto& e : elements) ids.push_back(dsHandle(dsCreate(entriesOf(e))));
    controllerVars[kModelListName] = makeArray(std::move(ids));
    vanilla = elements[5];
    vanillaIds = idsNow();
    vanillaLayout = layoutNow();
}
// One call of the Heroic roll, through wherever the game's calls land: x, y, and a third argument
// absent (charmprobe heroic's call shape), undefined, or the number `k`.
enum class HeroicArg { Absent, Undefined, Number };
static void dropHeroic(CInstance& self, HeroicArg third = HeroicArg::Absent, double k = 0.0) {
    static RValue a0, a1, a2, result;
    a0 = RValue(self.x); a1 = RValue(self.y);
    a2 = third == HeroicArg::Number ? RValue(k) : RValue();
    RValue* args[] = { &a0, &a1, &a2 };
    heroicEntry(&self, &self, result, third == HeroicArg::Absent ? 2 : 3, args);
}
// The built items that are a piece (by their own fields), and of which piece (-1: any).
static int countPieceBuilds(int piece) {
    int n = 0;
    for (const auto& b : builds) {
        M1sNs::Piece p = M1sNs::Piece::Helmet;
        if (M1sNs::IsPiece(b.t, b.a, b.b, b.c, b.j, p) && (piece < 0 || (int)p == piece)) ++n;
    }
    return n;
}
static std::string dropsStatLine() {
#ifdef HAS_MARK1DROPSTATLINE
    return Mark1DropStatLine();
#else
    return std::string();
#endif
}
static bool dropsSwitch(bool on) {
#ifdef HAS_MARK1DROPSSWITCH
    Mark1DropsSwitch(on);
    return true;
#else
    (void)on;
    return false;
#endif
}
static bool lineHas(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }
#ifdef HAS_MARK1DROPSTATE
// Every field of the drops' line, as the core fills it.
static M1dNs::DropStatFields dropsFields() {
    M1dNs::DropStatFields f;
    g_M1Drop.core.FillStat(f, g_SigDetectNative);
    return f;
}
#endif
// ---- the Mark 1 drops' kill route (#166): the ordinary unique roll -------------------------------
// lootListUnique[0..4] (setUniqueLayout's twenty ordinary uniques each, class 4 and each on one
// list only, so never a stand-in) plus the rows below, and [5] the Angelic list plus two triples
// only it holds. The kill stand-in rule (Mark1Drops.hpp's ResolveKillStandIns): a triple of the
// piece's class the ordinary roll keeps (rarity below 7, not hidden) with one count on all five,
// the smallest count first.
static const Triple kHelmetKill{ 0, 7, 10 };      // Satanic (6), once on each of [0..4]: qualifies at n 1, not chosen
static const Triple kHelmetKillZero{ 0, 7, 11 };  // Satanic, on [5] alone: n 0 on all five, the helmet's kill stand-in
static const Triple kBodyKill{ 1, 8, 20 };        // rarity 5, once on each of [0..4]: the body armor's kill stand-in, n 1
static const Triple kBootsKill{ 2, 9, 30 };       // Satanic, on [5] alone: the boots' kill stand-in, n 0
static const Triple kBootsUneven{ 2, 9, 29 };     // Satanic, twice on [0] and once on the rest: never a stand-in
static const Triple kGlovesHeroic{ 4, 10, 40 };   // Heroic (9): the ordinary roll refuses it, so gloves have none
static const char* const kKillStandIns =
    " kill-standin-helmet=7:11@6x0 kill-standin-body=8:20@5x1 kill-standin-boots=9:30@6x0 kill-standin-gloves=none";
static void setKillLayout() {
    std::array<std::vector<Triple>, 5> extra;
    for (auto& e : extra) e = { kHelmetKill, kBodyKill, kBootsUneven, kGlovesHeroic };
    extra[0].push_back(kBootsUneven);
    setUniqueLayout(extra, plus(vanillaTriples(), { kHelmetKillZero, kBootsKill }));
    for (const Triple& t : vanillaTriples()) repoInfo[t] = { 7.0, false };   // the Angelic list's own: refused by the ordinary roll
    repoInfo[kHelmetKill] = { 6.0, false };
    repoInfo[kHelmetKillZero] = { 6.0, false };
    repoInfo[kBodyKill] = { 5.0, false };
    repoInfo[kBootsKill] = { 6.0, false };
    repoInfo[kBootsUneven] = { 6.0, false };
    repoInfo[kGlovesHeroic] = { 9.0, false };
}
// What the kill route pushes onto each of [0..4] at one copy, in the core's plan order.
static const std::vector<Triple> kKillPush{ kHelmetKillZero, kBodyKill, kBootsKill };
static std::vector<Triple> killCarried(size_t k, std::initializer_list<Triple> elf = {}) {
    std::vector<Triple> v = plus(vanillaLayout[k], elf);
    v.insert(v.end(), kKillPush.begin(), kKillPush.end());
    return v;
}

static const char* const kFreshDropsLine =
    "mark1set drops: drops=off install=not-armed gate=off hook=not-installed list=- heroic-calls=0 pushed-calls=0 nested=0"
    " art-wait=0 helmet=0/0 body=0/0 boots=0/0 gloves=0/0 injected=0 refused=0 anomalies=0 standin-helmet=none"
    " standin-body=none standin-boots=none standin-gloves=none list-refused=0 unread=0 kill-gate=off kill-hook=not-installed"
    " kill-calls=0 kill-pushed=0 kill-nested=0 kill-helmet=0/0 kill-body=0/0 kill-boots=0/0 kill-gloves=0/0"
    " kill-standin-helmet=none kill-standin-body=none kill-standin-boots=none kill-standin-gloves=none"
    " draw-gate=off draw-hook=not-installed tier-hook=not-installed draw-calls=0 draw-pushed=0 draw-nested=0 tier-unread=0"
    " draw-helmet=0/0 draw-body=0/0 draw-boots=0/0 draw-gloves=0/0 draw-standin-helmet=none draw-standin-body=none"
    " draw-standin-boots=none draw-standin-gloves=none"
    " draw-typed=0/0 draw-untyped=0 draw-normal=0 draw-outside=busy:0,aside:0,lists:0,roll:0"
    " draw-class=bool:0/0,num:0/0,none:0,other:0 draw-class-check=0/0/0";

static const char* const kFreshFlightLine =
    "flightcharm: switches elf=off wings=off broom=off | flight install=not-armed state=off equipped=none answered=0 excluded=0"
    " outside-room=0 depth=0 | icons=not-loaded ground=0 | guard kept=0 failed=0 | drops gate=off elf=0/0 wings=0/0 broom=0/0"
    " injected=0 refused=0 anomalies=0 standin-elf=none elf-draw-hook=not-installed elf-draw-calls=0 elf-draw-pushed=0"
    " elf-draw=0/0 elf-draw-standin=none"
    " elf-draw-typed=0/0 elf-draw-untyped=0 elf-draw-normal=0 elf-draw-outside=busy:0,aside:0,lists:0,roll:0"
    " elf-draw-class=bool:0/0,num:0/0,none:0,other:0 elf-draw-class-check=0/0/0";
// The typed-build fields (#166's draw control) as each line has them, before the class fields.
static std::string drawTypedTail(int standIn, int natural, int untyped, int normal, int busy, int aside, int lists, int roll,
                                 const char* prefix = "draw-") {
    const std::string p = std::string(" ") + prefix;
    return p + "typed=" + std::to_string(standIn) + "/" + std::to_string(natural) + p + "untyped=" + std::to_string(untyped)
        + p + "normal=" + std::to_string(normal) + p + "outside=busy:" + std::to_string(busy) + ",aside:" + std::to_string(aside)
        + ",lists:" + std::to_string(lists) + ",roll:" + std::to_string(roll);
}
// The class fields (#166 replan 1) as each line ends with them: the in-scope builds by the kind and
// truth of their third argument, and the class check against the `c` the game's call stored.
struct ClassCounts { int boolTrue = 0, boolFalse = 0, numTrue = 0, numFalse = 0, none = 0, other = 0, agree = 0, disagree = 0, unread = 0; };
static std::string drawClassTail(const ClassCounts& c, const char* prefix = "draw-") {
    const std::string p = std::string(" ") + prefix;
    return p + "class=bool:" + std::to_string(c.boolTrue) + "/" + std::to_string(c.boolFalse) + ",num:" + std::to_string(c.numTrue) + "/"
        + std::to_string(c.numFalse) + ",none:" + std::to_string(c.none) + ",other:" + std::to_string(c.other) + p + "class-check="
        + std::to_string(c.agree) + "/" + std::to_string(c.disagree) + "/" + std::to_string(c.unread);
}
static ClassCounts boolAgree(int t, int f = 0) { ClassCounts c; c.boolTrue = t; c.boolFalse = f; c.agree = t + f; return c; }
static bool lineEndsWith(const std::string& line, const std::string& tail) {
    return line.size() >= tail.size() && line.compare(line.size() - tail.size(), tail.size(), tail) == 0;
}
// A field's value on a stat line (" <key>=<value>"), "" when the line has none.
static std::string statField(const std::string& line, const std::string& key) {
    const std::string at = " " + key + "=";
    const size_t i = line.find(at);
    if (i == std::string::npos) return std::string();
    const size_t from = i + at.size();
    return line.substr(from, line.find(' ', from) == std::string::npos ? std::string::npos : line.find(' ', from) - from);
}
// The numbers in a field's value, in order ("bool:3/1,num:0/0,none:2,other:0" -> 3 1 0 0 2 0).
static std::vector<long long> statNumbers(const std::string& value) {
    std::vector<long long> out;
    for (size_t i = 0; i < value.size();) {
        if (!std::isdigit((unsigned char)value[i])) { ++i; continue; }
        size_t end = i;
        while (end < value.size() && std::isdigit((unsigned char)value[end])) ++end;
        out.push_back(std::stoll(value.substr(i, end - i)));
        i = end;
    }
    return out;
}
// The class fields' sums, on a line (prefix "draw-" or "elf-draw-"): the bool, num, none and other
// counts sum to typed (both halves) + untyped + normal, the true halves of bool and num to typed +
// untyped, and every in-scope build is checked once (agree + disagree + unread).
static void requireClassSums(const std::string& line, const std::string& prefix) {
    const std::vector<long long> typed = statNumbers(statField(line, prefix + "typed"));
    const std::vector<long long> untyped = statNumbers(statField(line, prefix + "untyped"));
    const std::vector<long long> normal = statNumbers(statField(line, prefix + "normal"));
    const std::vector<long long> cls = statNumbers(statField(line, prefix + "class"));
    const std::vector<long long> check = statNumbers(statField(line, prefix + "class-check"));
    require(typed.size() == 2 && untyped.size() == 1 && normal.size() == 1 && cls.size() == 6 && check.size() == 3,
            "the line lacks a typed or class field: " + line);
    const long long scope = typed[0] + typed[1] + untyped[0] + normal[0];
    require(cls[0] + cls[1] + cls[2] + cls[3] + cls[4] + cls[5] == scope, "the class counts do not sum to the in-scope builds: " + line);
    require(cls[0] + cls[2] == typed[0] + typed[1] + untyped[0], "the true halves do not sum to typed + untyped: " + line);
    require(check[0] + check[1] + check[2] == scope, "the class check did not see every in-scope build once: " + line);
}

// ---- the kill draw (#166): DropItem's own unique draw ---------------------------------------------
// lootListUnique[0..4]: setUniqueLayout's twenty ordinary uniques each (class 4, rarity 3: the draw
// never keeps one) plus one Satanic unique of the game's own the draw keeps, and the rows below; [5]
// the Angelic list plus the rows only it holds. The draw stand-in rule (Mark1Drops.hpp's
// ResolveDrawStandIns, FlightCharms.hpp's ResolveElfDrawStandIn): a triple of the class the draw keeps
// as read (rarity 6, fields 40 and 41 clear), SS (tier 5) first - Pocket Elf's SS only - then the
// smallest largest count over [0..5]; the coin c / (n_k + c) takes n on the list the tier names.
static const Triple kDrawNatural{ 3, 2, 90 };      // Satanic, tier 3, once on each of [0..4]: the game's own unique
static const Triple kHelmetDrawSs{ 0, 12, 50 };    // Satanic SS, on [5] once: the helmet's draw stand-in, n 0 on [0..4]
static const Triple kHelmetDrawLow{ 0, 12, 51 };   // Satanic, tier 3, on [0]: kept by the draw, below SS, so not chosen
static const Triple kBodyDrawSs{ 1, 13, 60 };      // Satanic SS, twice on [2]: the body armor's, n_2 2 (1 in 3 there)
static const Triple kBootsDrawSs{ 2, 14, 70 };     // Satanic SS, on [5] once: the boots', n 0 on [0..4]
static const Triple kGlovesDrawF41{ 4, 15, 80 };   // Satanic SS with field 41 set: never kept, so gloves have none
static const Triple kElfDrawSs{ 10, 0, 76 };       // Aimbot.exe, Satanic SS, on [5] once: Pocket Elf's draw stand-in
static const Triple kElfDrawLow{ 10, 0, 41 };      // Satanic tier 4, on [1]: kept, below SS, never Pocket Elf's
static void setDrawLayout() {
    std::array<std::vector<Triple>, 5> extra;
    for (auto& e : extra) e = { kDrawNatural };
    extra[0].push_back(kHelmetDrawLow);
    extra[1].push_back(kElfDrawLow);
    extra[2].push_back(kBodyDrawSs);
    extra[2].push_back(kBodyDrawSs);
    setUniqueLayout(extra, plus(vanillaTriples(), { kHelmetDrawSs, kBootsDrawSs, kGlovesDrawF41, kElfDrawSs }));
    for (const Triple& t : vanillaTriples()) repoInfo[t] = { 7.0, false };   // the Angelic list's own: never kept by the draw
    repoInfo[kDrawNatural] = { 6.0, false, 3.0 };
    repoInfo[kHelmetDrawSs] = { 6.0, false, 5.0 };
    repoInfo[kHelmetDrawLow] = { 6.0, false, 3.0 };
    repoInfo[kBodyDrawSs] = { 6.0, false, 5.0 };
    repoInfo[kBootsDrawSs] = { 6.0, false, 5.0 };
    repoInfo[kGlovesDrawF41] = { 6.0, false, 5.0, true };
    repoInfo[kElfDrawSs] = { 6.0, false, 5.0 };
    repoInfo[kElfDrawLow] = { 6.0, false, 4.0 };
}
// The pieces' draw stand-ins in the core's plan order, and the stat line's form of them.
static const std::vector<Triple> kDrawPush{ kHelmetDrawSs, kBodyDrawSs, kBootsDrawSs };
static const char* const kDrawStandIns =
    " draw-standin-helmet=12:50@6/5x0,0,0,0,0,1 draw-standin-body=13:60@6/5x0,0,2,0,0,0"
    " draw-standin-boots=14:70@6/5x0,0,0,0,0,1 draw-standin-gloves=none";
// What list k carries during a pushing DropItem call: Pocket Elf's row first (when it is on), then the
// pieces' (when the drops are on).
static std::vector<Triple> drawCarried(size_t k, bool elf, bool pieces = true) {
    std::vector<Triple> v = vanillaLayout[k];
    if (elf) v.push_back(kElfDrawSs);
    if (pieces) v.insert(v.end(), kDrawPush.begin(), kDrawPush.end());
    return v;
}
// One call of the game's DropItem, through wherever the game's calls land (DropManager's detour once
// it is in), with twelve arguments, as each of its object-event call sites passes.
static RValue dropItemArgs[12];
static RValue* dropItemArgv[12];
static RValue* dropItem(CInstance& monster) {
    static RValue result;
    for (int i = 0; i < 12; ++i) { dropItemArgs[i] = RValue((double)(i + 1)); dropItemArgv[i] = &dropItemArgs[i]; }
    return &dropItemEntry(&monster, nullptr, result, 12, dropItemArgv);
}
static bool layoutsCarry(size_t from, bool elf, bool pieces = true) {
    for (size_t i = from; i < dropItemLayoutAtDraw.size(); ++i)
        for (size_t k = 0; k < 6; ++k)
            if (dropItemLayoutAtDraw[i][k] != drawCarried(k, elf, pieces)) return false;
    return from < dropItemLayoutAtDraw.size();
}

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
        // ---- the flight charms' drops (#158): baseline, holds against the source before them too ----
        } else if (test == "flight_untouched_installs_nothing") {
            // No switch ever touched: both rolls are the game's own and the plugin calls nothing.
            setUniqueLayout({ { { kAimbot }, { kAimbot }, { kAimbot }, { kAimbot }, { kAimbot } } });
            flightRepo();
            pickPolicy = Pick::Last;
            roll(monster, { true });
            dropUnique(monster);
            require(installs["DropUniqueItems"] == 0 && uniqueEntry == gameDropUniqueItems, "DropUniqueItems was hooked with no switch on");
            require(builtinCalls == 0, "with no switch on the plugin called the runtime");
            require(uniqueCalls == 1 && uniqueLayoutDuringCall[0] == vanillaLayout, "the unique roll saw lists that are not the game's own");
            require(builds.size() == 2 && countFlightBuilds(-1) == 0, "a roll with no switch on built a flight charm");
            require(builds[1].t == 10 && builds[1].b == 76 && builds[1].c == 1.0, "the unique roll did not build the game's own Aimbot.exe");
            require(linesStartingWith("flightcharm") == 0, "a flight line was logged with no switch on");
        } else if (test == "flight_switches_off_rows_vanilla") {
            // Every switch on and off again (the hooks stay installed and pass through): the rows
            // the rolls read, and what they build, are the game's own, byte for byte.
            setUniqueLayout({ { { kAimbot }, { kAimbot }, { kAimbot }, { kAimbot }, { kAimbot } } });
            flightRepo();
            for (FcNs::Charm c : { FcNs::Charm::Elf, FcNs::Charm::Wings, FcNs::Charm::Broom }) flightSwitch(c, true);
            for (FcNs::Charm c : { FcNs::Charm::Elf, FcNs::Charm::Wings, FcNs::Charm::Broom }) flightSwitch(c, false);
            installDetection();
            pickPolicy = Pick::Last;
            roll(monster, { false });
            roll(monster, { true });
            for (int k = 0; k < 5; ++k) { uniqueK = k; dropUnique(monster); }
            for (const auto& seen : layoutDuringCall) require(seen == vanillaLayout, "switched off, the Angelic roll saw a changed list");
            for (const auto& seen : uniqueLayoutDuringCall) require(seen == vanillaLayout, "switched off, the unique roll saw a changed list");
            require(layoutNow() == vanillaLayout, "switched off, the lists are not the game's own after the rolls");
            require(builds.size() == 6 && countFlightBuilds(-1) == 0, "switched off, a flight charm was built");
            require(builds[0].t == 0 && builds[0].b == 86 && builds[0].c == 1.0, "switched off, the Angelic hit did not build the game's own pick");
            for (size_t i = 1; i < builds.size(); ++i)
                require(builds[i].t == 10 && builds[i].b == 76 && builds[i].c == 1.0, "switched off, a unique roll built something else");
            require(linesStartingWith("flightcharm hit:") == 0, "switched off, a hit was taken for a flight charm");
            const std::string line = flightStatLine();
            if (!line.empty())
                require(line.find(" drops gate=off elf=0/0 wings=0/0 broom=0/0 injected=0 refused=0 anomalies=0 ") != std::string::npos,
                        "switched off, the stat line moved: " + line);
        // ---- the flight charms' drops (#158): target, fails against the source before them ----
        } else if (test == "flight_stat_line_fresh") {
            require(flightStatLine() == kFreshFlightLine, "the fresh `flightcharm stat` line is not the requirement: " + flightStatLine());
        } else if (test == "flight_wings_angelic_hit_built") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            flightSwitch(FcNs::Charm::Wings, true);
            require(flightGate(FcNs::Charm::Wings) == FcNs::Gate::On, "the wings switch did not arm: " + lastLine("flightcharm:"));
            require(anyLineHas("flightcharm: wings drop", " on"), "the switch-on did not say the drop is on");
            require(installs["DropUniqueItems"] == 0, "the wings switch hooked DropUniqueItems");
            require(groundIconInstalls == 1, "the switch-on did not install the shared ground-icon hook");
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kWaterMelon }), "the Angelic roll did not carry Water Melon once: " + describe(listDuringCall[0]));
            for (int k = 0; k < 5; ++k) require(layoutDuringCall[0][(size_t)k] == vanillaLayout[(size_t)k], "the wings switch pushed onto lootListUnique[0..4]");
            require(listTriples() == vanilla, "after the roll the Angelic list is not the game's own");
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(builds.size() == 1, "not one item for one hit");
            const BuildRecord b = builds[0];
            require(b.t == 10 && b.b == 0 && b.c == 0 && b.j == 0 && b.a >= 777200 && b.a <= 777343,
                    "the game was not handed a Gabriel Wings definition {t 10, a 777200..777343, b 0, c 0, j 0}");
            require(countFlightBuilds((int)FcNs::Charm::Wings) == 1, "the build is not Gabriel Wings");
            require(g_SigOurHits == 0 && g_SigBuilt == 0, "the hit was counted as #74's");
            require(anyLineHas("flightcharm hit:", "angelic roll -> Gabriel Wings (stand-in Water Melon, 1 in 1) built by the game"),
                    "the hit line does not say the game built Gabriel Wings: " + lastLine("flightcharm hit:"));
            require(flightStatLine().find(" drops gate=on elf=0/0 wings=1/1 broom=0/0 injected=2 refused=0 anomalies=0 ") != std::string::npos,
                    "the stat line does not count it: " + flightStatLine());
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_broom_coin_one_in_two") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            setList(plus(vanillaTriples(), { kSoulCollector }));   // n = 1: ours 1 in 2
            flightSwitch(FcNs::Charm::Broom, true);
            pickPolicy = Pick::Among; pickTriples = { kSoulCollector };
            for (int i = 0; i < 2000; ++i) roll(monster, { true });
            const long long ours = (long long)g_FlightDrop.tally[(int)FcNs::Charm::Broom].ours;
            require(ours >= 850 && ours <= 1150, "a Soul Collector hit was not ours about 1 in 2: " + std::to_string(ours));
            require(countFlightBuilds((int)FcNs::Charm::Broom) == ours && (long long)g_FlightDrop.tally[(int)FcNs::Charm::Broom].built == ours,
                    "the game did not build a Witch's Broomstick for every hit that was ours");
            for (const auto& b : builds) {
                FcNs::Rolled r;
                if (FcNs::IsFlightCharm(b.t, b.a, b.b, b.c, b.j, r)) require(b.a >= 777400 && b.a <= 777490, "a broom was built from another seed");
                else require(b.t == 10 && b.b == 91 && b.c == 1.0, "a hit that was not ours did not build the game's own Soul Collector");
            }
            require(linesEndingWith("flightcharm hit:", "Witch's Broomstick (stand-in Soul Collector, 1 in 2) built by the game") == (int)ours,
                    "an our-hit line does not say the game built it");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_angelic_rows_after_74s") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            g_HhForced = true;
            installDetection();
            flightSwitch(FcNs::Charm::Wings, true);
            flightSwitch(FcNs::Charm::Broom, true);
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kBeltStandIn, kWaterMelon, kSoulCollector }),
                    "the roll did not carry #74's row and then both flight rows: " + describe(listDuringCall[0]));
            require(listTriples() == vanilla, "after the roll the list is not the game's own");
            require(g_SigInjected == 1 && g_FlightDrop.injected == 2 && g_SigAnomalies == 0 && g_FlightDrop.anomalies == 0,
                    "the pushes were not counted, or a removal found someone else's tail");
            setList(vanillaTriples(false));   // n = 0 for all three: every stand-in hit is ours
            pickPolicy = Pick::Among; pickTriples = { kBeltStandIn, kWaterMelon, kSoulCollector };
            for (int i = 0; i < 300; ++i) roll(monster, { true });
            require(builds.size() == 300, "not one item per hit");
            require(countBuilds(1) > 0 && countFlightBuilds((int)FcNs::Charm::Wings) > 0 && countFlightBuilds((int)FcNs::Charm::Broom) > 0,
                    "not all three were built");
            require(countBuilds(1) + countFlightBuilds(-1) == 300, "a hit on a pushed entry built something else");
            require(listTriples() == vanilla && g_SigAnomalies == 0 && g_FlightDrop.anomalies == 0, "the rolls left the list changed");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_elf_unique_roll_built") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            setUniqueLayout({});   // no candidate anywhere: every count is 0, so Aimbot.exe is the stand-in
            // An entry that is not an array (the roll drops nothing for it) is no reason to refuse.
            dsList((*listVariable()->array)[0]).push_back(RValue(7.0));
            vanillaLayout = layoutNow();
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropUniqueItems"] == 1 && uniqueEntry != gameDropUniqueItems, "the elf switch did not hook DropUniqueItems, detoured");
            require(flightGate(FcNs::Charm::Elf) == FcNs::Gate::On, "the elf switch did not arm: " + lastLine("flightcharm:"));
            require(flightStatLine().find(" standin-elf=aimbot") != std::string::npos, "the stand-in is not Aimbot.exe: " + flightStatLine());
            const std::string sig = sigdropStatusLine();
            uniqueK = 3;
            pickPolicy = Pick::Last;
            dropUnique(monster);
            require(uniqueCalls == 1, "the game's unique roll did not run once");
            for (int k = 0; k < 5; ++k)
                require(uniqueLayoutDuringCall[0][(size_t)k] == plus(vanillaLayout[(size_t)k], { kAimbot }),
                        "lootListUnique[" + std::to_string(k) + "] did not carry Aimbot.exe once during the call");
            require(uniqueLayoutDuringCall[0][5] == vanillaLayout[5], "the elf pushed onto the Angelic list");
            require(layoutNow() == vanillaLayout, "after the call the lists are not the game's own");
            require(builds.size() == 1, "not one item for one call");
            const BuildRecord b = builds[0];
            require(b.t == 10 && b.a == 777100 && b.b == 0 && b.c == 0 && b.j == 0,
                    "the game was not handed Pocket Elf's definition {t 10, a 777100, b 0, c 0, j 0}");
            require(anyLineHas("flightcharm hit:", "unique roll -> Pocket Elf (stand-in Aimbot.exe, 1 in 1) built by the game"),
                    "the hit line does not say the game built Pocket Elf: " + lastLine("flightcharm hit:"));
            require(flightStatLine().find(" elf=1/1 wings=0/0 broom=0/0 injected=5 refused=0 anomalies=0 standin-elf=aimbot") != std::string::npos,
                    "the stat line does not count it: " + flightStatLine());
            const std::string after = sigdropStatusLine();
            require(after.substr(0, after.find(" cdpCalls=")) == sig.substr(0, sig.find(" cdpCalls=")), "`sigdrop status` moved: " + after);
            require(g_SigRollDepth == 0 && !g_FlightDrop.rollUnique, "the roll state stayed raised");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_elf_coin_one_in_two") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            setUniqueLayout({ { { kAimbot }, { kAimbot }, { kAimbot }, { kAimbot }, { kAimbot } } });   // n = 1 in each
            flightSwitch(FcNs::Charm::Elf, true);
            pickPolicy = Pick::Among; pickTriples = { kAimbot };
            for (int i = 0; i < 2000; ++i) { uniqueK = i % 5; dropUnique(monster); }
            const long long ours = (long long)g_FlightDrop.tally[(int)FcNs::Charm::Elf].ours;
            require(ours >= 850 && ours <= 1150, "an Aimbot.exe pick was not ours about 1 in 2: " + std::to_string(ours));
            require(builds.size() == 2000 && countFlightBuilds((int)FcNs::Charm::Elf) == ours
                    && (long long)g_FlightDrop.tally[(int)FcNs::Charm::Elf].built == ours, "the game did not build a Pocket Elf for every pick that was ours");
            for (const auto& b : builds)
                if (b.a != 777100) require(b.t == 10 && b.b == 76 && b.c == 1.0, "a pick that was not ours did not build the game's own Aimbot.exe");
            require(linesEndingWith("flightcharm hit:", "Pocket Elf (stand-in Aimbot.exe, 1 in 2) built by the game") == (int)ours,
                    "an our-hit line does not say the game built it");
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_elf_even_count_rule") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            // The first candidate, in table order, with the same count in all five sub-lists that
            // the roll's own filter keeps; none, and the switch refuses and pushes nothing.
            struct Case { const char* name; std::array<std::vector<Triple>, 5> extra; void (*arrange)(); const char* want; };
            const Case cases[] = {
                { "uneven first", { { { kAimbot, kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw } } }, [] {}, "witches-claw" },
                { "even but Angelic", { { { kAimbot, kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw } } },
                  [] { repoInfo[kWitchesClaw] = { 7.0, false }; }, "map-journal" },
                { "even but hidden", { { { kAimbot, kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw }, { kWitchesClaw } } },
                  [] { repoInfo[kWitchesClaw] = { 6.0, true }; }, "map-journal" },
            };
            for (const Case& c : cases) {
                reset();
                flightRepo();
                c.arrange();
                setUniqueLayout(c.extra);
                flightSwitch(FcNs::Charm::Elf, true);
                require(flightStatLine().find(std::string(" standin-elf=") + c.want) != std::string::npos,
                        std::string(c.name) + ": the stand-in is not " + c.want + ": " + flightStatLine());
                require(flightGate(FcNs::Charm::Elf) == FcNs::Gate::On, std::string(c.name) + ": the gate is not on");
            }
            reset();
            flightRepo();
            const std::array<std::vector<Triple>, 5> uneven = { { { kAimbot, kWitchesClaw, kMapJournal, kCrowsFeather, kSteve }, {}, {}, {}, {} } };
            setUniqueLayout(uneven);
            flightSwitch(FcNs::Charm::Elf, true);
            require(anyLineHas("flightcharm: refused - ", "elf: no stand-in with an even count across lootListUnique[0..4]"),
                    "the switch did not refuse naming the rule: " + lastLine("flightcharm:"));
            require(flightGate(FcNs::Charm::Elf) == FcNs::Gate::Refused, "the gate is not refused");
            require(flightStatLine().find(" drops gate=refused ") != std::string::npos
                    && flightStatLine().find(" standin-elf=refused(aimbot:1/0/0/0/0,") != std::string::npos,
                    "the stat line does not name the refusal and the counts: " + flightStatLine());
            pickPolicy = Pick::Last;
            dropUnique(monster);
            require(uniqueLayoutDuringCall[0] == vanillaLayout && g_FlightDrop.injected == 0, "refused, the roll carried an entry");
            // A sub-list's length changes: counted again, and now Aimbot.exe is even.
            for (int k = 1; k < 5; ++k) dsList((*listVariable()->array)[(size_t)k]).push_back(makeEntry(kAimbot));
            vanillaLayout = layoutNow();
            dropUnique(monster);
            require(flightStatLine().find(" standin-elf=aimbot") != std::string::npos, "a changed sub-list was not counted again: " + flightStatLine());
            for (int k = 0; k < 5; ++k)
                require(uniqueLayoutDuringCall[1][(size_t)k] == plus(vanillaLayout[(size_t)k], { kAimbot }), "the recounted stand-in was not pushed");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_elf_untyped_and_not_ours_are_vanilla") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            setUniqueLayout({});
            flightSwitch(FcNs::Charm::Elf, true);
            const std::string sig = sigdropStatusLine();
            pickPolicy = Pick::First;   // an ordinary unique: the game's own
            dropUnique(monster);
            repoRead = RepoRead::PickedThenOther;   // a hit no read types: never ours
            pickPolicy = Pick::Last;
            dropUnique(monster);
            require(builds.size() == 2 && countFlightBuilds(-1) == 0, "a pick that was not ours built a flight charm");
            require(builds[0].t == 4 && builds[0].c == 1.0 && builds[1].t == 10 && builds[1].b == 76 && builds[1].c == 1.0,
                    "the game did not build its own picks");
            require(g_FlightDrop.tally[0].ours == 0 && linesStartingWith("flightcharm hit:") == 0, "a pick that was not ours was taken");
            require(g_SigUntyped == 0 && g_SigGameHits == 0, "the unique roll moved #74's counters");
            const std::string after = sigdropStatusLine();
            require(after.substr(0, after.find(" cdpCalls=")) == sig.substr(0, sig.find(" cdpCalls=")), "`sigdrop status` moved: " + after);
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_elf_table_only_never_arms") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            setUniqueLayout({});
            tableOnlyHook = "DropUniqueItems";
            flightSwitch(FcNs::Charm::Elf, true);
            require(flightGate(FcNs::Charm::Elf) == FcNs::Gate::NoDetection, "a table-only DropUniqueItems hook armed the elf");
            require(anyLineHas("flightcharm: refused - ", "TABLE-ONLY"), "the switch-on did not say the hook is table-only");
            pickPolicy = Pick::Last;
            dropUnique(monster);
            require(uniqueLayoutDuringCall[0] == vanillaLayout && g_FlightDrop.injected == 0 && countFlightBuilds(-1) == 0,
                    "a table-only hook changed the roll");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_refused_rewrite_latches") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            flightSwitch(FcNs::Charm::Wings, true);
            frozenField = "a";
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(anyLineHas("flightcharm: Gabriel Wings refused (", "stays off for this session"), "the refusal was not logged: " + lastLine("flightcharm:"));
            require(flightGate(FcNs::Charm::Wings) == FcNs::Gate::Refused, "the refused charm is not latched off");
            require(builds.size() == 1 && countFlightBuilds(-1) == 0 && builds[0].b == 47, "a refused rewrite did not leave the game's own Water Melon");
            require(flightStatLine().find(" drops gate=refused elf=0/0 wings=1/0 broom=0/0 injected=1 refused=1 ") != std::string::npos,
                    "the stat line does not count the refusal: " + flightStatLine());
            roll(monster, { false });
            require(listDuringCall.back() == vanilla, "a latched-off charm was still pushed");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_unique_throw_removes_entries") {
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            setUniqueLayout({});
            flightSwitch(FcNs::Charm::Elf, true);
            uniqueThrows = true;
            bool threw = false;
            try { dropUnique(monster); } catch (const std::runtime_error&) { threw = true; }
            require(threw, "the game's exception did not reach the caller");
            require(uniqueLayoutDuringCall[0][0] == plus(vanillaLayout[0], { kAimbot }), "the entry was not on the list while the roll ran");
            require(layoutNow() == vanillaLayout, "after a throw the entries stayed on the lists");
            require(g_SigRollDepth == 0 && !g_FlightDrop.rollUnique, "the roll state stayed raised after a throw");
            uniqueThrows = false;
            pickPolicy = Pick::Last;
            dropUnique(monster);
            require(countFlightBuilds((int)FcNs::Charm::Elf) == 1, "after a throw, the next pick of ours was not built");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_partial_push_angelic_removes_its_entries") {
            // PR #214 review: ds_list_add refused partway through the flight push. #74's row and
            // Gabriel Wings' went on, Witch's Broomstick's did not; the flight entries that did go
            // on come off again before the roll, so the roll carries #74's row alone, #74's own
            // removal still finds its tail, and the next roll carries all three again.
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            g_HhForced = true;
            installDetection();
            flightSwitch(FcNs::Charm::Wings, true);
            flightSwitch(FcNs::Charm::Broom, true);
            dsListAddThrowAfter = 2;
            roll(monster, { false });
            require(listDuringCall[0] == plus(vanilla, { kBeltStandIn }),
                    "the roll carried a partial flight push: " + describe(listDuringCall[0]));
            require(listTriples() == vanilla, "after the roll the list kept stand-in entries: " + describe(listTriples()));
            require(g_SigAnomalies == 0, "#74's removal no longer found its own tail");
            require(g_FlightDrop.injected == 0 && g_FlightDrop.anomalies == 1, "the refused push was not counted once, or was counted injected");
            require(anyLineHas("flightcharm: pushing", "threw"), "the refused push was not logged");
            roll(monster, { false });
            require(listDuringCall[1] == plus(vanilla, { kBeltStandIn, kWaterMelon, kSoulCollector }),
                    "the next roll did not carry all three rows: " + describe(listDuringCall[1]));
            require(listTriples() == vanilla && g_SigAnomalies == 0 && g_FlightDrop.anomalies == 1, "the next roll left the list changed");
#else
            require(false, "this source has no flight drop switch");
#endif
        } else if (test == "flight_partial_push_unique_removes_its_entries") {
            // The same on the unique roll: Pocket Elf's stand-in went onto lootListUnique[0] and
            // [1] before ds_list_add refused on [2]; both come off, so the roll reads every
            // sub-list as the game's own and the next roll carries the stand-in on all five.
#if defined(HAS_FLIGHTDROPSWITCH) && defined(HAS_FLIGHTDROPSTATE)
            flightRepo();
            setUniqueLayout({});
            flightSwitch(FcNs::Charm::Elf, true);
            dsListAddThrowAfter = 2;
            dropUnique(monster);
            require(uniqueLayoutDuringCall[0] == vanillaLayout, "the roll carried a partial push");
            require(layoutNow() == vanillaLayout, "after the roll the sub-lists kept stand-in entries");
            require(g_FlightDrop.injected == 0 && g_FlightDrop.anomalies == 1, "the refused push was not counted once, or was counted injected");
            dropUnique(monster);
            for (int k = 0; k < 5; ++k)
                require(uniqueLayoutDuringCall[1][(size_t)k] == plus(vanillaLayout[(size_t)k], { kAimbot }),
                        "the next roll did not carry the stand-in on sub-list " + std::to_string(k));
            require(layoutNow() == vanillaLayout && g_FlightDrop.anomalies == 1, "the next roll left the sub-lists changed");
#else
            require(false, "this source has no flight drop switch");
#endif
        // ---- the Mark 1 drops (#166): baseline, holds against the source before the drop adapter ----
        } else if (test == "mark1_heroic_untouched_installs_nothing") {
            // The drops switch never touched: the Heroic roll is the game's own, nothing is hooked
            // and the plugin calls nothing.
            setHeroicLayout();
            pickPolicy = Pick::First;
            for (int i = 0; i < 5; ++i) dropHeroic(monster);
            require(installs["DropItemHeroic"] == 0 && heroicEntry == gameDropItemHeroic, "DropItemHeroic was hooked with no switch on");
            require(builtinCalls == 0, "with no switch on the plugin called the runtime");
            require(heroicCalls == 5, "the game's Heroic roll did not run once per call");
            for (const auto& seen : heroicLayoutDuringCall) require(seen == vanillaLayout, "the Heroic roll saw lists that are not the game's own");
            require(builds.size() == 5 && countPieceBuilds(-1) == 0, "a roll with no switch on built a piece");
            require(builds[0].t == 5 && builds[0].b == 0 && builds[0].j == 0 && builds[0].c == 1.0,
                    "the Heroic roll did not build the first entry it keeps, the game's own");
            require(linesStartingWith("mark1set") == 0, "a drops line was logged with no switch on");
            const std::string line = dropsStatLine();
            require(line.empty() || line == kFreshDropsLine, "the drops line moved with no switch on: " + line);
        } else if (test == "mark1_heroic_switch_off_is_vanilla") {
            // On (the hook goes in, detoured) and off again: the hook stays and passes every call
            // through, counted - the list each call reads and what it builds are the game's own.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            require(dropsSwitch(true), "this source has no drops switch");
            dropsSwitch(false);
            pickPolicy = Pick::Among; pickTriples = { kBootsHeroic, kHelmetHeroic };
            for (int i = 0; i < 40; ++i) dropHeroic(monster);
            require(installs["DropItemHeroic"] == 1 && heroicEntry != gameDropItemHeroic, "the switch-on did not hook DropItemHeroic, detoured");
            for (const auto& seen : heroicLayoutDuringCall) require(seen == vanillaLayout, "switched off, the Heroic roll saw a changed list");
            require(layoutNow() == vanillaLayout, "switched off, the lists are not the game's own after the rolls");
            require(builds.size() == 40 && countPieceBuilds(-1) == 0, "switched off, a piece was built");
            for (const auto& b : builds) require(b.c == 1.0 && b.a == kLootGroundSeed, "switched off, a record was rewritten");
            const std::string line = dropsStatLine();
            require(lineHas(line, "mark1set drops: drops=off install=installed gate=off hook=detoured "), "the drops line: " + line);
            require(lineHas(line, " heroic-calls=40 pushed-calls=0 nested=0 art-wait=0 helmet=0/0 body=0/0 boots=0/0 gloves=0/0"
                                  " injected=0 refused=0 anomalies=0 "), "switched off, the counters moved: " + line);
            require(linesStartingWith("mark1set drops hit:") == 0, "switched off, a hit was taken for a piece");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        // ---- the Mark 1 drops (#166): target, every scenario fails against the source before them ----
        } else if (test == "mark1_heroic_stat_line_fresh") {
            require(!dropsStatLine().empty(), "this source has no Mark 1 drop adapter");
            require(dropsStatLine() == kFreshDropsLine, "the fresh drops line is not the core's: " + dropsStatLine());
        } else if (test == "mark1_heroic_hit_builds_the_piece") {
            // A hit on a pushed stand-in's whole triple is the piece's at c in n + c (boots: 1 in 2);
            // the game builds the piece from the rewritten record - its class, seed and base, c 0,
            // j 0 - and the forge's final pass counts it built; a stand-in hit lost on the coin is
            // the game's own stand-in, its record untouched.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=on install=installed gate=pending kill-gate=pending draw-gate=pending",
                    "the switch-on reply: " + lastLine("mark1set:"));
            require(g_Mark1.ArtArmed() && g_M1sGroundTried && mark1GroundInstalls == 1,
                    "the drops' switch-on did not arm the art and try the pieces' ground install");
            const std::string resolved = dropsStatLine();
            const std::vector<Triple> carried = plus(vanilla, { kHelmetHeroic, kBodyHeroic, kBootsHeroic });
            pickPolicy = Pick::Among; pickTriples = { kBootsHeroic };
            int calls = 0, lost = 0;
            while (calls < 200 && (dropsFields().pieces[2].ours == 0 || lost == 0)) {
                const size_t before = builds.size();
                dropHeroic(monster);
                ++calls;
                require(builds.size() == before + 1, "a call did not build one item");
                if (builds.back().a != 777502.0) ++lost;   // every pick is a boots stand-in: ours or lost on the coin
            }
            require(heroicCalls == calls, "the game's Heroic roll did not run once per call");
            for (const auto& seen : heroicLayoutDuringCall) {
                require(seen[5] == carried, "a call's list did not carry the three stand-ins once, after the game's own entries");
                for (size_t k = 0; k < 5; ++k) require(seen[k] == vanillaLayout[k], "the push reached another sub-list");
            }
            require(layoutNow() == vanillaLayout, "after the calls the lists are not the game's own");
            const M1dNs::DropStatFields f = dropsFields();
            require(f.pieces[2].ours >= 1 && lost >= 1, "200 calls gave no piece, or no lost coin");
            require((int)f.pieces[2].ours == countPieceBuilds(2) && f.pieces[2].built == f.pieces[2].ours && countPieceBuilds(-1) == countPieceBuilds(2),
                    "the boots taken were not each built once, and counted built");
            for (const auto& b : builds) {
                if (b.a == 777502.0) require(b.t == 2 && b.b == 0 && b.c == 0 && b.j == 0, "a piece was not built from Mark 1 Boots' own definition");
                else require(b.t == 2 && b.j == 5 && b.b == 30 && b.c == 1.0 && b.a == kLootGroundSeed, "a lost coin did not build the game's own stand-in");
            }
            require(anyLineHas("mark1set drops hit:", "heroic roll -> Mark 1 Boots (stand-in 2/5/30, 1 in 2) built by the game"),
                    "the hit line does not say the game built the boots: " + lastLine("mark1set drops hit:"));
            const std::string line = dropsStatLine();
            require(lineHas(resolved, " gate=pending hook=detoured ") && lineHas(resolved, " standin-boots=pending "),
                    "the drops line before the first call: " + resolved);
            require(lineHas(line, " gate=on hook=detoured "), "the drops line after the calls: " + line);
            require(lineHas(line, " list=5 heroic-calls=" + std::to_string(calls) + " pushed-calls=" + std::to_string(calls) + " "),
                    "every call was not pushed for: " + line);
            require(lineHas(line, " injected=" + std::to_string(3 * calls) + " refused=0 anomalies=0 "), "the injected count: " + line);
            require(lineHas(line, " standin-helmet=3:10@9x2 standin-body=4:20@9x1 standin-boots=5:30@9x1 standin-gloves=none "),
                    "the stand-ins are not the core's: " + line);
            require(g_SigRollDepth == 0 && !g_M1Drop.rolling, "the roll state stayed raised");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_coin_one_in_n_plus_c") {
            // The helmet's stand-in is listed twice (n 2): one copy makes a helmet-stand-in hit the
            // piece's 1 in 3, two copies 2 in 4; every hit not ours is the game's own stand-in.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            pickPolicy = Pick::Among; pickTriples = { kHelmetHeroic };
            for (int i = 0; i < 3000; ++i) dropHeroic(monster);
            const long long ours = (long long)dropsFields().pieces[0].ours;
            require(ours >= 850 && ours <= 1150, "a helmet stand-in pick was not ours about 1 in 3: " + std::to_string(ours));
            require(builds.size() == 3000 && countPieceBuilds(0) == ours && (long long)dropsFields().pieces[0].built == ours,
                    "the game did not build a helmet for every pick that was ours");
            for (const auto& b : builds)
                if (b.a != 777500.0) require(b.t == 0 && b.j == 3 && b.b == 10 && b.c == 1.0, "a pick not ours was not the game's own stand-in");
                else require(b.t == 0 && b.b == 7 && b.c == 0 && b.j == 0, "a helmet was not built at its base 7");
            require(anyLineHas("mark1set drops hit:", "Mark 1 Helmet (stand-in 0/3/10, 1 in 3) built by the game"), "the hit line's odds");
            g_Mark1DropCopies = 2;
            for (int i = 0; i < 3000; ++i) dropHeroic(monster);
            const long long ours2 = (long long)dropsFields().pieces[0].ours - ours;
            require(ours2 >= 1350 && ours2 <= 1650, "with two copies a pick was not ours about 2 in 4: " + std::to_string(ours2));
            require(heroicLayoutDuringCall.back()[5] == plus(vanilla, { kHelmetHeroic, kHelmetHeroic, kBodyHeroic, kBodyHeroic, kBootsHeroic, kBootsHeroic }),
                    "two copies did not push each stand-in twice, in plan order");
            require(layoutNow() == vanillaLayout, "the copies were not all taken off again");
            require(anyLineHas("mark1set drops hit:", "Mark 1 Helmet (stand-in 0/3/10, 2 in 4) built by the game"), "the hit line's odds at two copies");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_list_index_from_the_argument") {
            // The third argument names the sub-list: a whole number 0..5 is that list, absent or
            // undefined is 5, anything else is refused and counted, and nothing is pushed for it.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout(2);
            dropsSwitch(true);
            pickPolicy = Pick::Among; pickTriples = { kBootsHeroic };
            dropHeroic(monster, HeroicArg::Number, 2.0);
            require(heroicListRead.back() == 2, "the model did not read element 2");
            require(heroicLayoutDuringCall[0][2] == plus(vanillaLayout[2], { kHelmetHeroic, kBodyHeroic, kBootsHeroic }),
                    "lootListUnique[2] did not carry the stand-ins during the call");
            require(heroicLayoutDuringCall[0][5] == vanillaLayout[5], "the push for list 2 went onto lootListUnique[5]");
            require(lineHas(dropsStatLine(), " list=2 "), "list= does not name 2: " + dropsStatLine());
            pickPolicy = Pick::First;
            dropHeroic(monster, HeroicArg::Undefined);
            dropHeroic(monster);
            require(heroicLayoutDuringCall[1] == vanillaLayout && heroicLayoutDuringCall[2] == vanillaLayout,
                    "list 5, which holds no piece class the roll keeps here, carried a push");
            const std::string five = dropsStatLine();
            require(lineHas(five, " gate=not-resolved ") && lineHas(five, " list=5 ") && lineHas(five, " pushed-calls=1 "),
                    "an undefined or absent argument did not read list 5: " + five);
            require(lineHas(five, " standin-helmet=none standin-body=none standin-boots=none standin-gloves=none "),
                    "list 5's stand-ins: " + five);
            dropHeroic(monster, HeroicArg::Number, 2.5);
            dropHeroic(monster, HeroicArg::Number, 7.0);
            require(heroicLayoutDuringCall[3] == vanillaLayout && heroicLayoutDuringCall[4] == vanillaLayout, "a refused argument carried a push");
            require(lineHas(dropsStatLine(), " list-refused=2 "), "the refused arguments were not counted: " + dropsStatLine());
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_nested_in_the_angelic_roll_passes_through") {
            // A Heroic call inside the Angelic roll (a roll already in progress) is the game's own,
            // counted `nested=`; the same hook pushes for a call outside any roll.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            angelicNestsHeroic = true;
            pickPolicy = Pick::First;
            roll(monster, { false });
            require(heroicCalls == 1 && heroicLayoutDuringCall[0] == vanillaLayout, "the nested call saw a pushed list");
            require(countPieceBuilds(-1) == 0, "the nested call built a piece");
            require(lineHas(dropsStatLine(), " heroic-calls=1 pushed-calls=0 nested=1 "), "the nested call was not counted: " + dropsStatLine());
            angelicNestsHeroic = false;
            dropHeroic(monster);
            require(heroicLayoutDuringCall[1][5] == plus(vanilla, { kHelmetHeroic, kBodyHeroic, kBootsHeroic }),
                    "a call outside any roll did not carry the stand-ins");
            require(g_SigRollDepth == 0 && layoutNow() == vanillaLayout, "the roll state or the list was left changed");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_throw_leaves_the_list_as_found") {
            // The game's roll throws: the throw reaches the caller, and the entries come off first.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            heroicThrows = true;
            bool threw = false;
            try { dropHeroic(monster); } catch (const std::exception&) { threw = true; }
            require(threw, "the game's throw did not reach the caller");
            require(heroicLayoutDuringCall[0][5] == plus(vanilla, { kHelmetHeroic, kBodyHeroic, kBootsHeroic }), "the call did not carry the stand-ins");
            require(layoutNow() == vanillaLayout, "after the throw the list kept stand-in entries");
            require(g_SigRollDepth == 0 && !g_M1Drop.rolling, "the roll state stayed raised after the throw");
            require(lineHas(dropsStatLine(), " injected=3 refused=0 anomalies=0 "), "the throw was counted wrong: " + dropsStatLine());
            heroicThrows = false;
            dropHeroic(monster);
            require(heroicLayoutDuringCall[1][5] == plus(vanilla, { kHelmetHeroic, kBodyHeroic, kBootsHeroic }) && layoutNow() == vanillaLayout,
                    "the next call did not push and remove as before");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_class_with_no_standin") {
            // Gloves have no kept triple on the list: the other three are pushed and gloves read
            // none. A list with no piece class kept at all pushes nothing and the gate reads
            // not-resolved, re-resolved when the list changed.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            dropHeroic(monster);
            require(heroicLayoutDuringCall[0][5] == plus(vanilla, { kHelmetHeroic, kBodyHeroic, kBootsHeroic }), "the push was not the three stand-ins");
            const std::string line = dropsStatLine();
            require(lineHas(line, " standin-gloves=none ") && lineHas(line, " injected=3 ") && lineHas(line, " gate=on "),
                    "gloves with no stand-in: " + line);
            setHeroicLayout(5, false);
            dropHeroic(monster);
            require(heroicLayoutDuringCall[1] == vanillaLayout, "a list with no stand-in carried a push");
            const std::string none = dropsStatLine();
            require(lineHas(none, " gate=not-resolved ") && lineHas(none, " pushed-calls=1 ") && lineHas(none, " injected=3 "),
                    "a list with no stand-in: " + none);
            require(lineHas(none, " standin-helmet=none standin-body=none standin-boots=none standin-gloves=none "), "the changed list was not resolved again: " + none);
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_74s_counters_unchanged") {
            // #74's counters belong to the Angelic roll: a Heroic call's typed and untyped hits
            // leave `sigdrop status` as it was; an untyped hit is never a piece.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            const std::string sig = sigdropStatusLine();
            const long untyped = g_SigUntyped;
            pickPolicy = Pick::Among; pickTriples = { kBootsHeroic };
            for (int i = 0; i < 100; ++i) dropHeroic(monster);
            const long long typedOurs = (long long)dropsFields().pieces[2].ours;
            require(typedOurs >= 1, "100 typed boots picks gave no piece");
            repoRead = RepoRead::PickedThenOther;
            for (int i = 0; i < 20; ++i) dropHeroic(monster);
            require((long long)dropsFields().pieces[2].ours == typedOurs, "an untyped hit was taken for a piece");
            for (size_t i = 100; i < builds.size(); ++i) require(builds[i].c == 1.0 && builds[i].a == kLootGroundSeed, "an untyped hit was rewritten");
            const std::string after = sigdropStatusLine();
            require(after.substr(0, after.find(" cdpCalls=")) == sig.substr(0, sig.find(" cdpCalls=")), "`sigdrop status` moved: " + after);
            require(g_SigUntyped == untyped, "the untyped hits reached #74's untyped=");
#ifdef HAS_FLIGHTDROPSTATE
            require(g_FlightDrop.injected == 0 && g_FlightDrop.anomalies == 0, "the flight charms' counters moved");
#endif
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_installs_with_a_character") {
            // At character select the switch only arms (nothing hooked, the art's ground install
            // waiting); the tick installs once the local player resolves.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            playerResolves = false;
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=on install=waiting-for-character gate=off kill-gate=off draw-gate=off",
                    "the armed reply: " + lastLine("mark1set:"));
            require(installs["DropItemHeroic"] == 0 && installs["CreateDefaultParams"] == 0 && heroicEntry == gameDropItemHeroic,
                    "something was hooked at character select");
            require(g_Mark1.ArtArmed() && !g_M1sGroundTried, "the switch-on did not arm the art, or tried its ground install without a character");
            for (int i = 0; i < 200; ++i) Mark1DropsTick();
            require(installs["DropItemHeroic"] == 0, "the tick installed before the local player resolved");
            playerResolves = true;
            for (int i = 0; i < 200 && installs["DropItemHeroic"] == 0; ++i) Mark1DropsTick();
            require(installs["DropItemHeroic"] == 1 && heroicEntry != gameDropItemHeroic, "the tick did not install once the player resolved");
            require(g_M1sGroundTried && mark1GroundInstalls == 1, "the install did not try the pieces' ground install first");
            require(lineHas(dropsStatLine(), " install=installed gate=pending hook=detoured "), "after the tick: " + dropsStatLine());
            dropHeroic(monster);
            require(heroicLayoutDuringCall.back()[5] == plus(vanilla, { kHelmetHeroic, kBodyHeroic, kBootsHeroic }), "the installed hook did not push");
            require(lineHas(dropsStatLine(), " install=installed gate=on hook=detoured "), "after the first call: " + dropsStatLine());
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_table_only_and_no_detection_refuse") {
            // A table-only DropItemHeroic hook refuses the install and turns the switch off; #74's
            // detection short of four inline detours keeps the gate at no-detection, pushing nothing.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            tableOnlyHook = "DropItemHeroic";
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=off install=refused gate=refused kill-gate=refused draw-gate=refused",
                    "the refused reply: " + lastLine("mark1set:"));
            require(anyLineHas("mark1set drops: refused - ", "DropItemHeroic went in TABLE-ONLY"), "the refusal line: " + lastLine("mark1set drops:"));
            dropHeroic(monster);
            require(heroicLayoutDuringCall[0] == vanillaLayout && lineHas(dropsStatLine(), " hook=TABLE-ONLY "), "a table-only hook pushed: " + dropsStatLine());
            reset();
            setHeroicLayout();
            tableOnlyHook = "CreateDefaultParams";
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=on install=installed gate=no-detection kill-gate=no-detection draw-gate=no-detection",
                    "the no-detection reply: " + lastLine("mark1set:"));
            dropHeroic(monster);
            require(heroicLayoutDuringCall[0] == vanillaLayout && lineHas(dropsStatLine(), " heroic-calls=1 pushed-calls=0 "),
                    "with #74's detection not inline a call carried a push: " + dropsStatLine());
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_refused_rewrite_latches") {
            // A rewrite that does not read back is refused: the record is put back (the game builds
            // its own stand-in), `refused=` counts it, and that piece is latched off for the session.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            frozenField = "a";
            pickPolicy = Pick::Among; pickTriples = { kBootsHeroic };
            for (int i = 0; i < 200 && dropsFields().refused == 0; ++i) dropHeroic(monster);
            require(dropsFields().refused == 1, "no rewrite was refused");
            require(anyLineHas("mark1set drops: ", "Mark 1 Boots refused (field a did not read back)")
                    && anyLineHas("mark1set drops: ", "Mark 1 Boots stays off for this session (rewrite refused)"), "the refusal line");
            require(countPieceBuilds(-1) == 0, "a refused rewrite built a piece");
            require(builds.back().t == 2 && builds.back().j == 5 && builds.back().b == 30 && builds.back().c == 1.0, "the refused record was not put back");
            dropHeroic(monster);
            require(heroicLayoutDuringCall.back()[5] == plus(vanilla, { kHelmetHeroic, kBodyHeroic }), "the latched boots were pushed again");
            require(lineHas(dropsStatLine(), " boots=1/0 ") && lineHas(dropsStatLine(), " refused=1 "), "the latch's counts: " + dropsStatLine());
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_heroic_art_not_ready_pushes_nothing") {
            // While the art is armed with icons embedded and the ground consumer's one install was
            // not tried, a call pushes nothing (`art-wait=`): no piece is built to draw Error_spr.
#ifdef HAS_MARK1DROPSTATE
            setHeroicLayout();
            dropsSwitch(true);
            g_M1sGroundTried = false;
            dropHeroic(monster);
            require(heroicLayoutDuringCall[0] == vanillaLayout, "a call that is not art-ready carried a push");
            require(lineHas(dropsStatLine(), " pushed-calls=0 nested=0 art-wait=1 "), "the art wait was not counted: " + dropsStatLine());
            g_M1sGroundTried = true;
            dropHeroic(monster);
            require(heroicLayoutDuringCall[1][5] == plus(vanilla, { kHelmetHeroic, kBodyHeroic, kBootsHeroic }), "an art-ready call did not push");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        // ---- the Mark 1 drops' kill route (#166): baseline, holds against the source before it ----
        } else if (test == "mark1_kill_untouched_installs_nothing") {
            // Neither switch touched: the ordinary unique roll is the game's own, nothing is hooked
            // and the plugin calls nothing.
            setKillLayout();
            pickPolicy = Pick::Among; pickTriples = { kBootsKill, kBodyKill };
            for (int k = 0; k < 5; ++k) { uniqueK = k; dropUnique(monster); }
            require(installs["DropUniqueItems"] == 0 && uniqueEntry == gameDropUniqueItems, "DropUniqueItems was hooked with no switch on");
            require(builtinCalls == 0, "with no switch on the plugin called the runtime");
            require(uniqueCalls == 5, "the game's unique roll did not run once per call");
            for (const auto& seen : uniqueLayoutDuringCall) require(seen == vanillaLayout, "the unique roll saw lists that are not the game's own");
            require(builds.size() == 5 && countPieceBuilds(-1) == 0, "a roll with no switch on built a piece");
            for (const auto& b : builds) require(b.t == 1 && b.j == 8 && b.b == 20 && b.c == 1.0, "the unique roll did not build the game's own pick");
            require(linesStartingWith("mark1set") == 0 && linesStartingWith("flightcharm") == 0, "a line was logged with no switch on");
        } else if (test == "mark1_kill_switch_off_is_vanilla") {
            // On (the shared detour goes in, detoured) and off again: the detour stays and passes every
            // call through, each one counted (`kill-calls=`) - the lists each call reads and what it
            // builds are the game's own.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            dropsSwitch(true);
            dropsSwitch(false);
            pickPolicy = Pick::Among; pickTriples = { kBootsKill, kBodyKill };
            for (int i = 0; i < 20; ++i) { uniqueK = i % 5; dropUnique(monster); }
            require(installs["DropUniqueItems"] == 1 && uniqueEntry != gameDropUniqueItems, "the switch-on did not hook DropUniqueItems, detoured");
            for (const auto& seen : uniqueLayoutDuringCall) require(seen == vanillaLayout, "switched off, the unique roll saw a changed list");
            require(layoutNow() == vanillaLayout, "switched off, the lists are not the game's own after the rolls");
            require(builds.size() == 20 && countPieceBuilds(-1) == 0, "switched off, a piece was built");
            for (const auto& b : builds) require(b.c == 1.0 && b.a == kLootGroundSeed, "switched off, a record was rewritten");
            const std::string line = dropsStatLine();
            require(lineHas(line, " kill-gate=off kill-hook=detoured kill-calls=20 kill-pushed=0 kill-nested=0 kill-helmet=0/0 kill-body=0/0"
                                  " kill-boots=0/0 kill-gloves=0/0 "), "switched off, the kill fields: " + line);
            require(lineHas(line, " injected=0 refused=0 anomalies=0 "), "switched off, the counters moved: " + line);
            require(linesStartingWith("mark1set drops hit:") == 0, "switched off, a hit was taken for a piece");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        // ---- the Mark 1 drops' kill route (#166): target, every scenario fails against the source before it ----
        } else if (test == "mark1_kill_hit_builds_the_piece") {
            // The drops' install hooks DropUniqueItems through the detour Pocket Elf shares; each call
            // carries one entry per kill stand-in on each of lootListUnique[0..4] (none on [5]); a hit on
            // a pushed stand-in's whole triple is the piece's at c in n + c (boots: n 0, so every one),
            // the game builds it from the rewritten record and the forge's final pass counts it built,
            // on the kill route's tallies; gloves (no kept triple of one count) read none.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=on install=installed gate=pending kill-gate=pending draw-gate=pending",
                    "the switch-on reply: " + lastLine("mark1set:"));
            require(installs["DropUniqueItems"] == 1 && uniqueEntry != gameDropUniqueItems, "the drops' install did not hook DropUniqueItems, detoured");
            const std::string fresh = dropsStatLine();
            require(lineHas(fresh, " kill-gate=pending kill-hook=detoured kill-calls=0 kill-pushed=0 kill-nested=0 ")
                        && lineHas(fresh, " kill-standin-helmet=pending kill-standin-body=pending kill-standin-boots=pending kill-standin-gloves=pending"),
                    "the drops line before the first call: " + fresh);
            const std::string sig = sigdropStatusLine();
            pickPolicy = Pick::Among; pickTriples = { kBootsKill };
            for (int i = 0; i < 10; ++i) { uniqueK = i % 5; dropUnique(monster); }
            require(uniqueCalls == 10, "the game's unique roll did not run once per call");
            for (const auto& seen : uniqueLayoutDuringCall) {
                for (size_t k = 0; k < 5; ++k) require(seen[k] == killCarried(k), "lootListUnique[" + std::to_string(k) + "] did not carry the three kill stand-ins once");
                require(seen[5] == vanillaLayout[5], "the kill route pushed onto lootListUnique[5]");
            }
            require(layoutNow() == vanillaLayout, "after the calls the lists are not the game's own");
            require(builds.size() == 10 && countPieceBuilds(2) == 10 && countPieceBuilds(-1) == 10, "every boots pick (n 0) was not built as Mark 1 Boots");
            for (const auto& b : builds) require(b.t == 2 && b.c == 0 && b.j == 0, "a piece was not built from Mark 1 Boots' own definition");
            const M1dNs::DropStatFields f = dropsFields();
            require(f.killPieces[2].ours == 10 && f.killPieces[2].built == 10 && f.pieces[2].ours == 0 && f.pieces[2].built == 0,
                    "the boots were not counted ours and built on the kill route's tallies");
            require(anyLineHas("mark1set drops hit:", "unique roll -> Mark 1 Boots (stand-in 2/9/30, 1 in 1) built by the game"),
                    "the hit line does not say the game built the boots: " + lastLine("mark1set drops hit:"));
            const std::string line = dropsStatLine();
            require(lineHas(line, " kill-gate=on kill-hook=detoured kill-calls=10 kill-pushed=10 kill-nested=0 kill-helmet=0/0 kill-body=0/0"
                                  " kill-boots=10/10 kill-gloves=0/0" + std::string(kKillStandIns)), "the kill fields after the calls: " + line);
            require(lineHas(line, " injected=150 refused=0 anomalies=0 "), "the injected count (3 per list, 5 lists, 10 calls): " + line);
            require(lineHas(line, " gate=pending hook=detoured list=- heroic-calls=0 pushed-calls=0 "), "the Heroic roll's fields moved: " + line);
            const std::string after = sigdropStatusLine();
            require(after.substr(0, after.find(" cdpCalls=")) == sig.substr(0, sig.find(" cdpCalls=")), "`sigdrop status` moved: " + after);
            require(g_FlightDrop.injected == 0 && g_FlightDrop.tally[(int)FcNs::Charm::Elf].ours == 0, "Pocket Elf's counters moved");
#ifdef HAS_MARK1KILLINJECTGUARD
            require(g_SigRollDepth == 0 && !g_M1Drop.killRolling && g_FlightDrop.uniqueDepth == 0, "the roll state stayed raised");
#endif
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_coin_one_in_n_plus_c") {
            // The body armor's kill stand-in is listed once on each of [0..4] (n 1): a pick of it is the
            // piece's 1 in 2 whichever list the call read; every pick not ours is the game's own.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            dropsSwitch(true);
            pickPolicy = Pick::Among; pickTriples = { kBodyKill };
            for (int i = 0; i < 2000; ++i) { uniqueK = i % 5; dropUnique(monster); }
            const long long ours = (long long)dropsFields().killPieces[1].ours;
            require(ours >= 850 && ours <= 1150, "a body armor stand-in pick was not ours about 1 in 2: " + std::to_string(ours));
            require(builds.size() == 2000 && countPieceBuilds(1) == ours && (long long)dropsFields().killPieces[1].built == ours,
                    "the game did not build a body armor for every pick that was ours");
            for (const auto& b : builds)
                if (b.c == 1.0) require(b.t == 1 && b.j == 8 && b.b == 20 && b.a == kLootGroundSeed, "a pick not ours was not the game's own stand-in");
                else require(b.t == 1 && b.c == 0 && b.j == 0, "a body armor was not built from its own definition");
            require(anyLineHas("mark1set drops hit:", "(stand-in 1/8/20, 1 in 2) built by the game"), "the hit line's odds");
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_and_pocket_elf_each_hit_their_own") {
            // Both on, one detour: each call carries Pocket Elf's stand-in and then the pieces' on each of
            // [0..4]; a pick of Aimbot.exe is Pocket Elf's, a pick of the boots' stand-in Mark 1 Boots', and
            // each is counted on its own feature's tallies alone.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            flightRepo();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropUniqueItems"] == 1, "a second DropUniqueItems hook went in");
            require(flightGate(FcNs::Charm::Elf) == FcNs::Gate::On, "the elf did not arm on the shared detour: " + lastLine("flightcharm:"));
            pickPolicy = Pick::Among; pickTriples = { kAimbot, kBootsKill };
            for (int i = 0; i < 400; ++i) { uniqueK = i % 5; dropUnique(monster); }
            for (const auto& seen : uniqueLayoutDuringCall)
                for (size_t k = 0; k < 5; ++k) require(seen[k] == killCarried(k, { kAimbot }), "a list did not carry Pocket Elf's entry and then the pieces'");
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
            const int elf = countFlightBuilds((int)FcNs::Charm::Elf), boots = countPieceBuilds(2);
            require(builds.size() == 400 && elf + boots == 400 && elf > 100 && boots > 100, "the picks were not each built as their own item: elf "
                    + std::to_string(elf) + ", boots " + std::to_string(boots));
            require((int)g_FlightDrop.tally[(int)FcNs::Charm::Elf].ours == elf && (int)g_FlightDrop.tally[(int)FcNs::Charm::Elf].built == elf,
                    "Pocket Elf's tally is not its own builds");
            const M1dNs::DropStatFields f = dropsFields();
            require((int)f.killPieces[2].ours == boots && (int)f.killPieces[2].built == boots && f.pieces[2].ours == 0, "the boots' kill tally is not their builds");
            require(g_FlightDrop.injected == 400 * 5 && f.injected == 400 * 15, "the pushes were not each counted by their own feature");
            require(g_FlightDrop.anomalies == 0 && f.anomalies == 0, "a removal found someone else's tail");
            require(linesEndingWith("flightcharm hit:", "Pocket Elf (stand-in Aimbot.exe, 1 in 1) built by the game") == elf,
                    "an elf hit line is missing");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_each_switch_pushes_only_its_own") {
            // Mark 1 on and Pocket Elf off: nothing of Pocket Elf's is pushed; Pocket Elf on and Mark 1
            // off: nothing of Mark 1's, and still the one hook.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            flightRepo();
            dropsSwitch(true);
            pickPolicy = Pick::First;
            dropUnique(monster);
            for (size_t k = 0; k < 5; ++k) require(uniqueLayoutDuringCall[0][k] == killCarried(k), "Mark 1 alone did not carry its stand-ins alone");
            require(g_FlightDrop.injected == 0 && !g_FlightDrop.rollUnique, "Mark 1 alone pushed for Pocket Elf");
            flightSwitch(FcNs::Charm::Elf, true);
            dropsSwitch(false);
            dropUnique(monster);
            for (size_t k = 0; k < 5; ++k)
                require(uniqueLayoutDuringCall[1][k] == plus(vanillaLayout[k], { kAimbot }), "Pocket Elf alone carried a Mark 1 stand-in");
            require(installs["DropUniqueItems"] == 1, "a second DropUniqueItems hook went in");
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
            require(g_FlightDrop.injected == 5 && dropsFields().injected == 15, "the pushes: flight " + std::to_string(g_FlightDrop.injected));
            require(lineHas(dropsStatLine(), " kill-calls=2 kill-pushed=1 "), "both calls were not counted: " + dropsStatLine());
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_throw_leaves_the_lists_as_found") {
            // The game's unique roll throws with both features' entries on: the throw reaches the caller,
            // and every entry of both comes off all five lists first.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            flightRepo();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            uniqueThrows = true;
            bool threw = false;
            try { dropUnique(monster); } catch (const std::exception&) { threw = true; }
            require(threw, "the game's throw did not reach the caller");
            for (size_t k = 0; k < 5; ++k) require(uniqueLayoutDuringCall[0][k] == killCarried(k, { kAimbot }), "the call did not carry both features' entries");
            require(layoutNow() == vanillaLayout, "after the throw a list kept stand-in entries");
            require(g_SigRollDepth == 0 && !g_FlightDrop.rollUnique, "the roll state stayed raised after the throw");
#ifdef HAS_MARK1KILLINJECTGUARD
            require(!g_M1Drop.killRolling && g_FlightDrop.uniqueDepth == 0, "the kill route's state stayed raised after the throw");
#endif
            require(dropsFields().anomalies == 0 && g_FlightDrop.anomalies == 0, "the throw was counted as an anomaly");
            uniqueThrows = false;
            pickPolicy = Pick::First;
            dropUnique(monster);
            for (size_t k = 0; k < 5; ++k) require(uniqueLayoutDuringCall[1][k] == killCarried(k, { kAimbot }), "the next call did not push as before");
            require(layoutNow() == vanillaLayout, "the next call did not remove as before");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_nested_in_the_angelic_roll_passes_through") {
            // A unique call inside the Angelic roll (a roll already in progress) is the game's own,
            // counted `kill-nested=`; the same detour pushes for a call outside any roll.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            dropsSwitch(true);
            angelicNestsUnique = true;
            pickPolicy = Pick::First;
            roll(monster, { false });
            require(uniqueCalls == 1 && uniqueLayoutDuringCall[0] == vanillaLayout, "the nested call saw a pushed list");
            require(countPieceBuilds(-1) == 0, "the nested call built a piece");
            require(lineHas(dropsStatLine(), " kill-calls=1 kill-pushed=0 kill-nested=1 "), "the nested call was not counted: " + dropsStatLine());
            angelicNestsUnique = false;
            dropUnique(monster);
            for (size_t k = 0; k < 5; ++k) require(uniqueLayoutDuringCall[1][k] == killCarried(k), "a call outside any roll did not carry the stand-ins");
            require(g_SigRollDepth == 0 && layoutNow() == vanillaLayout, "the roll state or the lists were left changed");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_74s_counters_unchanged") {
            // #74's counters belong to the Angelic roll: the kill route's typed and untyped hits leave
            // `sigdrop status` as it was; an untyped hit is never a piece.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            dropsSwitch(true);
            const std::string sig = sigdropStatusLine();
            const long untyped = g_SigUntyped;
            pickPolicy = Pick::Among; pickTriples = { kBootsKill };
            for (int i = 0; i < 50; ++i) { uniqueK = i % 5; dropUnique(monster); }
            require(dropsFields().killPieces[2].ours == 50, "50 typed boots picks (n 0) were not all pieces");
            repoRead = RepoRead::PickedThenOther;
            for (int i = 0; i < 20; ++i) { uniqueK = i % 5; dropUnique(monster); }
            require(dropsFields().killPieces[2].ours == 50, "an untyped hit was taken for a piece");
            for (size_t i = 50; i < builds.size(); ++i) require(builds[i].c == 1.0 && builds[i].a == kLootGroundSeed, "an untyped hit was rewritten");
            const std::string after = sigdropStatusLine();
            require(after.substr(0, after.find(" cdpCalls=")) == sig.substr(0, sig.find(" cdpCalls=")), "`sigdrop status` moved: " + after);
            require(g_SigUntyped == untyped, "the untyped hits reached #74's untyped=");
            require(g_FlightDrop.injected == 0 && g_FlightDrop.anomalies == 0, "the flight charms' counters moved");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_shares_the_elfs_detour") {
            // One detour and one install for both features, whichever switch comes first; a table-only
            // DropUniqueItems hook refuses the drops whole with one line of their own and is never
            // installed twice.
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            flightRepo();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropUniqueItems"] == 1 && uniqueEntry != gameDropUniqueItems, "drops first: not one detoured hook");
            require(flightGate(FcNs::Charm::Elf) == FcNs::Gate::On, "drops first: the elf did not arm on the shared detour");
            reset();
            setKillLayout();
            flightRepo();
            flightSwitch(FcNs::Charm::Elf, true);
            dropsSwitch(true);
            require(installs["DropUniqueItems"] == 1 && uniqueEntry != gameDropUniqueItems, "elf first: not one detoured hook");
            require(lastLine("mark1set:") == "mark1set: drops=on install=installed gate=pending kill-gate=pending draw-gate=pending", "elf first, the reply: " + lastLine("mark1set:"));
            require(lineHas(dropsStatLine(), " kill-hook=detoured "), "elf first, kill-hook: " + dropsStatLine());
            reset();
            setKillLayout();
            tableOnlyHook = "DropUniqueItems";
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=off install=refused gate=refused kill-gate=refused draw-gate=refused", "table-only, the reply: " + lastLine("mark1set:"));
            require(anyLineHas("mark1set drops: refused - ", "DropUniqueItems went in TABLE-ONLY"), "the refusal line: " + lastLine("mark1set drops:"));
            require(linesStartingWith("flightcharm") == 0, "the drops' refusal was logged as Pocket Elf's");
            require(lineHas(dropsStatLine(), " kill-hook=TABLE-ONLY "), "table-only, kill-hook: " + dropsStatLine());
            dropUnique(monster);
            require(uniqueLayoutDuringCall[0] == vanillaLayout && countPieceBuilds(-1) == 0, "a table-only hook changed the roll");
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropUniqueItems"] == 1, "the elf's switch-on installed a second hook");
            require(anyLineHas("flightcharm: refused - ", "TABLE-ONLY"), "the elf's switch-on did not say the shared hook is table-only");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        } else if (test == "mark1_kill_art_not_ready_pushes_nothing") {
            // While the art is armed with icons embedded and the ground consumer's one install was not
            // tried, a kill-route call pushes nothing (`art-wait=`).
#ifdef HAS_MARK1DROPSTATE
            setKillLayout();
            dropsSwitch(true);
            g_M1sGroundTried = false;
            dropUnique(monster);
            require(uniqueLayoutDuringCall[0] == vanillaLayout, "a call that is not art-ready carried a push");
            require(lineHas(dropsStatLine(), " art-wait=1 ") && lineHas(dropsStatLine(), " kill-calls=1 kill-pushed=0 "), "the art wait: " + dropsStatLine());
            g_M1sGroundTried = true;
            dropUnique(monster);
            for (size_t k = 0; k < 5; ++k) require(uniqueLayoutDuringCall[1][k] == killCarried(k), "an art-ready call did not push");
#else
            require(false, "this source has no Mark 1 drop adapter");
#endif
        // ---- the kill draw (#166): baseline, holds against the source before it ----
        } else if (test == "mark1_draw_untouched_installs_nothing") {
            // Neither switch touched: DropItem and its own unique draw are the game's, nothing is
            // hooked and the plugin calls nothing.
            setDrawLayout();
            pickPolicy = Pick::First;
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(installs["DropItem"] == 0 && dropItemEntry == gameDropItem, "DropItem was hooked with no switch on");
            require(installs["LoadSatanicDropTier"] == 0 && satTierEntry == gameLoadSatanicDropTier, "LoadSatanicDropTier was hooked with no switch on");
            require(builtinCalls == 0, "with no switch on the plugin called the runtime");
            require(dropItemCalls == 3 && satTierCalls == 3, "DropItem or its tier did not run once per call");
            for (const auto& seen : dropItemLayoutAtDraw) require(seen == vanillaLayout, "the draw read lists that are not the game's own");
            require(builds.size() == 3, "not one unique per call");
            for (const auto& b : builds) require(b.t == 3 && b.j == 2 && b.b == 90 && b.c == 1.0 && b.a == kLootGroundSeed, "the draw did not build the game's own pick");
            require(linesStartingWith("mark1set") == 0 && linesStartingWith("flightcharm") == 0 && linesStartingWith("kill draw") == 0,
                    "a line was logged with no switch on");
        } else if (test == "mark1_draw_switch_off_is_vanilla") {
            // Both features on (DropManager's DropItem detour and raredrop's tier detour go in, detoured)
            // and off again: Hook_DropItem calls the game's DropItem exactly as before - once per call,
            // with the caller's own twelve arguments, its return handed back - each call counted
            // (` draw-calls=`, ` elf-draw-calls=`), and the lists the draw reads, what it builds and the
            // runtime calls the plugin makes are the game's own.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropItem"] == 1 && dropItemEntry != gameDropItem, "the switch-on did not hook DropItem, detoured");
            require(installs["LoadSatanicDropTier"] == 1 && satTierEntry != gameLoadSatanicDropTier, "the switch-on did not hook LoadSatanicDropTier, detoured");
            dropsSwitch(false);
            flightSwitch(FcNs::Charm::Elf, false);
            const int runtime = builtinCalls;
            pickPolicy = Pick::First;
            for (int i = 0; i < 4; ++i) {
                RValue* r = dropItem(monster);
                require(r == &dropItemReturn, "Hook_DropItem did not hand back the game's own return");
                require(dropItemArgcSeen == 12 && dropItemArgsSeen == std::vector<RValue*>(dropItemArgv, dropItemArgv + 12),
                        "the game's DropItem was not handed the caller's own arguments");
            }
            require(dropItemCalls == 4 && satTierCalls == 4, "switched off, DropItem or its tier did not run once per call");
            for (const auto& seen : dropItemLayoutAtDraw) require(seen == vanillaLayout, "switched off, the draw read a changed list");
            require(layoutNow() == vanillaLayout, "switched off, the lists are not the game's own after the calls");
            require(builtinCalls == runtime, "switched off, the plugin called the runtime during a DropItem call");
            require(builds.size() == 4 && countPieceBuilds(-1) == 0 && countFlightBuilds(-1) == 0, "switched off, a piece or a charm was built");
            for (const auto& b : builds) require(b.c == 1.0 && b.a == kLootGroundSeed, "switched off, a record was rewritten");
            const std::string line = dropsStatLine();
            require(lineHas(line, " draw-gate=off draw-hook=detoured tier-hook=detoured draw-calls=4 draw-pushed=0 draw-nested=0 tier-unread=0"
                                  " draw-helmet=0/0 draw-body=0/0 draw-boots=0/0 draw-gloves=0/0 "), "switched off, the draw fields: " + line);
            require(lineHas(flightStatLine(), " elf-draw-hook=detoured elf-draw-calls=4 elf-draw-pushed=0 elf-draw=0/0 "),
                    "switched off, the elf's draw fields: " + flightStatLine());
            require(linesStartingWith("kill draw") == 0 && linesStartingWith("mark1set drops hit:") == 0 && linesStartingWith("flightcharm hit:") == 0,
                    "switched off, a hit or an anomaly was logged");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "flight_draw_switch_off_is_vanilla") {
            // Pocket Elf alone, on and off again: the draw's hooks stay in and pass every call through.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropItem"] == 1 && installs["LoadSatanicDropTier"] == 1, "Pocket Elf's switch-on did not hook the draw");
            flightSwitch(FcNs::Charm::Elf, false);
            pickPolicy = Pick::Last;
            for (int i = 0; i < 3; ++i) dropItem(monster);
            for (const auto& seen : dropItemLayoutAtDraw) require(seen == vanillaLayout, "switched off, the draw read a changed list");
            require(builds.size() == 3 && countFlightBuilds(-1) == 0, "switched off, a charm was built");
            require(lineHas(flightStatLine(), " elf-draw-hook=detoured elf-draw-calls=3 elf-draw-pushed=0 elf-draw=0/0 elf-draw-standin=none"),
                    "switched off, the elf's draw fields: " + flightStatLine());
#else
            require(false, "this source has no kill draw");
#endif
        // ---- the kill draw (#166): target, every scenario fails against the source before it ----
        } else if (test == "mark1_draw_hit_builds_the_piece") {
            // The drops' install hooks DropItem (DropManager's one detour) and LoadSatanicDropTier
            // (raredrop's); each DropItem call carries one entry per draw stand-in on each of
            // lootListUnique[0..5] for its length; a typed pick of a pushed stand-in's whole triple is
            // the piece's at c in n_k + c, k the tier the call recorded (boots: n_3 0, so every one); the
            // game builds it from the rewritten record, the forge's final pass counts it built on the
            // draw's own tally, and the hit's line says so. #74's `sigdrop status` does not move.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=on install=installed gate=pending kill-gate=pending draw-gate=pending",
                    "the switch-on reply: " + lastLine("mark1set:"));
            require(installs["DropItem"] == 1 && dropItemEntry != gameDropItem, "the drops' install did not hook DropItem, detoured");
            require(installs["LoadSatanicDropTier"] == 1 && satTierEntry != gameLoadSatanicDropTier, "the drops' install did not hook the tier, detoured");
            const std::string fresh = dropsStatLine();
            require(lineHas(fresh, " draw-gate=pending draw-hook=detoured tier-hook=detoured draw-calls=0 draw-pushed=0 draw-nested=0 tier-unread=0 ")
                        && lineHas(fresh, " draw-standin-helmet=pending draw-standin-body=pending draw-standin-boots=pending draw-standin-gloves=pending"),
                    "the drops line before the first call: " + fresh);
            const std::string sig = sigdropStatusLine();
            pickPolicy = Pick::Last;
            satTierAnswer = RValue(3.0);
            for (int i = 0; i < 5; ++i) dropItem(monster);
            require(dropItemCalls == 5 && dropItemTiers.size() == 5 && dropItemTiers[0] == 3, "DropItem did not run once per call at tier 3");
            require(layoutsCarry(0, false), "a draw did not read every list carrying the three draw stand-ins once");
            require(layoutNow() == vanillaLayout, "after the calls the lists are not the game's own");
            require(builds.size() == 5 && countPieceBuilds(2) == 5, "every boots pick (n_3 0) was not built as Mark 1 Boots");
            for (const auto& b : builds) require(b.t == 2 && b.c == 0 && b.j == 0, "a piece was not built from Mark 1 Boots' own definition");
            const M1dNs::DropStatFields f = dropsFields();
            require(f.drawPieces[2].ours == 5 && f.drawPieces[2].built == 5 && f.pieces[2].ours == 0 && f.killPieces[2].ours == 0,
                    "the boots were not counted ours and built on the draw's own tally");
            require(linesEndingWith("mark1set drops hit:", "kill draw -> Mark 1 Boots (stand-in 2/14/70, 1 in 1, k 3) built by the game") == 5,
                    "a hit line does not say the game built the boots: " + lastLine("mark1set drops hit:"));
            const std::string line = dropsStatLine();
            require(lineHas(line, " draw-gate=on draw-hook=detoured tier-hook=detoured draw-calls=5 draw-pushed=5 draw-nested=0 tier-unread=0"
                                  " draw-helmet=0/0 draw-body=0/0 draw-boots=5/5 draw-gloves=0/0" + std::string(kDrawStandIns)),
                    "the draw fields after the calls: " + line);
            require(lineHas(line, " injected=90 refused=0 anomalies=0 "), "the injected count (3 per list, 6 lists, 5 calls): " + line);
            const std::string after = sigdropStatusLine();
            require(after.substr(0, after.find(" cdpCalls=")) == sig.substr(0, sig.find(" cdpCalls=")), "`sigdrop status` moved: " + after);
            require(g_FlightDrop.injected == 0, "Pocket Elf's counters moved");
            require(g_SigRollDepth == 0 && g_KillDraw.depth == 0 && g_KillDraw.pushDepth == 0 && g_KillDraw.aside == 0, "the draw's state stayed raised");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_coin_one_in_n_plus_c_at_k") {
            // The body armor's draw stand-in is listed twice on [2] and nowhere else: a pick of it at
            // tier 2 is the piece's 1 in 3, at tier 4 every time; a pick not ours is the game's own.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            pickPolicy = Pick::Among; pickTriples = { kBodyDrawSs };
            satTierAnswer = RValue(2.0);
            for (int i = 0; i < 1500; ++i) dropItem(monster);
            const long long ours = (long long)dropsFields().drawPieces[1].ours;
            require(ours >= 400 && ours <= 600, "a body armor stand-in pick at k 2 was not ours about 1 in 3: " + std::to_string(ours));
            require(builds.size() == 1500 && countPieceBuilds(1) == ours && (long long)dropsFields().drawPieces[1].built == ours,
                    "the game did not build a body armor for every pick that was ours");
            for (const auto& b : builds)
                if (b.c == 1.0) require(b.t == 1 && b.j == 13 && b.b == 60 && b.a == kLootGroundSeed, "a pick not ours was not the game's own stand-in");
                else require(b.t == 1 && b.c == 0 && b.j == 0, "a body armor was not built from its own definition");
            require(anyLineHas("mark1set drops hit:", "kill draw -> Mark 1 Body Armor (stand-in 1/13/60, 1 in 3, k 2) built by the game"),
                    "the hit line's odds at k 2");
            satTierAnswer = RValue(4.0);
            for (int i = 0; i < 50; ++i) dropItem(monster);
            require((long long)dropsFields().drawPieces[1].ours == ours + 50, "a pick at k 4 (n_4 0) was not ours every time");
            require(anyLineHas("mark1set drops hit:", "(stand-in 1/13/60, 1 in 1, k 4) built by the game"), "the hit line's odds at k 4");
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_no_tier_is_refused") {
            // LoadSatanicDropTier answers no number: a pick of a stand-in is refused (` tier-unread=`), the
            // game's own stand-in is built, untouched, and no line names a piece.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            satTierAnswer = RValue();
            pickPolicy = Pick::Last;
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(countPieceBuilds(-1) == 0, "a pick with no tier recorded was built as a piece");
            require(builds.size() == 3, "not one unique per call");
            for (const auto& b : builds) require(b.t == 2 && b.j == 14 && b.b == 70 && b.c == 1.0 && b.a == kLootGroundSeed, "the game's own stand-in was rewritten");
            const std::string line = dropsStatLine();
            require(lineHas(line, " tier-unread=3 draw-helmet=0/0 draw-body=0/0 draw-boots=0/0 "), "the refusals: " + line);
            require(linesStartingWith("mark1set drops hit:") == 0, "a refused pick logged a hit");
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_two_hits_in_one_call") {
            // A call that draws twice and picks a stand-in both times: each hit is settled before the
            // next starts, each rewritten, built and counted, each with its own line.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            dropItemDraws = 2;
            pickPolicy = Pick::Last;
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(builds.size() == 6 && countPieceBuilds(2) == 6, "two hits per call were not each built as Mark 1 Boots");
            require(dropsFields().drawPieces[2].ours == 6 && dropsFields().drawPieces[2].built == 6, "the two hits per call were not each counted");
            require(linesEndingWith("mark1set drops hit:", "kill draw -> Mark 1 Boots (stand-in 2/14/70, 1 in 1, k 3) built by the game") == 6,
                    "a hit's line is missing or wrong: " + lastLine("mark1set drops hit:"));
            require(lineHas(dropsStatLine(), " draw-calls=3 draw-pushed=3 "), "one push per call: " + dropsStatLine());
            require(layoutsCarry(0, false) && layoutNow() == vanillaLayout, "the second draw did not read the same push, or the lists were left changed");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_nested_angelic_sees_the_lists_as_before") {
            // #74's Angelic roll inside a pushing DropItem call sees the lists exactly as on its own -
            // #74's stand-in, no draw stand-in - and types its hit the same way; the draw after it reads
            // the stand-ins back on, and its hit is the piece's (` draw-nested=1`).
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            g_HhEnabled = true; g_HhForced = true;
            installDetection();
            pickPolicy = Pick::Last;
            roll(monster, { true });
            require(originalCalls == 1 && linesStartingWith("angelic hit:") == 1 && anyLineHas("angelic hit:", "picked 8/0/51"),
                    "the Angelic roll on its own did not hit #74's stand-in: " + lastLine("angelic hit:"));
            const std::vector<std::vector<Triple>> alone = layoutDuringCall[0];
            const long hits = g_SigGameHits;
            dropItemNestsAngelic = true;
            outcomes.assign({ true });
            dropItem(monster);
            require(originalCalls == 2 && layoutDuringCall[1] == alone, "the Angelic roll nested in DropItem did not see the lists as on its own");
            require(g_SigGameHits == hits + 1 && linesStartingWith("angelic hit:") == 2 && lastLine("angelic hit:").find("picked 8/0/51") != std::string::npos,
                    "the nested Angelic hit was not typed as on its own: " + lastLine("angelic hit:"));
            require(layoutsCarry(0, false), "the draw after the nested roll did not read the stand-ins back on");
            require(countPieceBuilds(2) == 1 && dropsFields().drawPieces[2].built == 1, "the draw's hit after the nested roll was not the piece's");
            require(lineHas(dropsStatLine(), " draw-calls=1 draw-pushed=1 draw-nested=1 "), "the nested roll was not counted: " + dropsStatLine());
            require(layoutNow() == vanillaLayout && g_SigRollDepth == 0 && g_KillDraw.aside == 0, "the lists or the state were left changed");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_nested_unique_pushes_and_hits_as_today") {
            // The shared DropUniqueItems detour inside a pushing DropItem call sees the lists without the
            // draw's entries and pushes and hits as it does on its own (the kill stand-ins on [0..4]);
            // the draw after it reads its own entries back on.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            dropItemNestsUnique = true;
            uniqueK = 3;
            pickPolicy = Pick::Last;
            dropItem(monster);
            require(uniqueCalls == 1, "the nested unique roll did not run once");
            for (size_t k = 0; k < 5; ++k)
                require(uniqueLayoutDuringCall[0][k] == plus(vanillaLayout[k], { kHelmetDrawSs, kBootsDrawSs, kGlovesDrawF41 }),
                        "lootListUnique[" + std::to_string(k) + "] during the nested unique roll was not the kill route's own push");
            require(uniqueLayoutDuringCall[0][5] == vanillaLayout[5], "the nested unique roll saw a draw entry on [5]");
            const M1dNs::DropStatFields f = dropsFields();
            require(f.killPieces[3].ours == 1 && f.killPieces[3].built == 1 && countPieceBuilds(3) == 1, "the nested unique roll's hit was not the gloves' as on its own");
            require(anyLineHas("mark1set drops hit:", "unique roll -> Mark 1 Gloves"), "the nested unique roll's hit line");
            require(layoutsCarry(0, false) && f.drawPieces[2].built == 1 && countPieceBuilds(2) == 1, "the draw after the nested roll did not hit as before");
            require(lineHas(dropsStatLine(), " kill-calls=1 kill-pushed=1 kill-nested=0 ") && lineHas(dropsStatLine(), " draw-nested=1 "),
                    "the counts: " + dropsStatLine());
            require(layoutNow() == vanillaLayout && g_FlightDrop.uniqueDepth == 0 && g_KillDraw.aside == 0, "the lists or the state were left changed");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_throw_leaves_the_lists_as_found") {
            // The game's DropItem throws with the entries on: the throw reaches the caller and every entry
            // comes off all six lists first; the next call pushes and removes as before.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            dropItemThrows = true;
            bool threw = false;
            try { dropItem(monster); } catch (const std::exception&) { threw = true; }
            require(threw, "the game's throw did not reach the caller");
            require(layoutNow() == vanillaLayout, "after the throw a list kept stand-in entries");
            require(g_KillDraw.depth == 0 && g_KillDraw.pushDepth == 0 && !g_KillDraw.onLists, "the draw's state stayed raised after the throw");
            require(dropsFields().anomalies == 0, "the throw was counted as an anomaly");
            dropItemThrows = false;
            pickPolicy = Pick::Last;
            dropItem(monster);
            require(layoutsCarry(0, false) && countPieceBuilds(2) == 1 && layoutNow() == vanillaLayout, "the next call did not push, hit and remove as before");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_dropmult_copies_each_carry_the_draw") {
            // `dropmult item 3`: Hook_DropItem calls the game's DropItem three times, and each original call
            // is one draw call with its own push and removal.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            ForgePact::DropManager::Instance().SetMultiplier("item", 3);
            pickPolicy = Pick::Last;
            dropItem(monster);
            require(dropItemCalls == 3 && dropItemLayoutAtDraw.size() == 3 && layoutsCarry(0, false), "each dropmult copy did not carry the draw's entries");
            require(countPieceBuilds(2) == 3, "each copy's hit was not the piece's");
            require(lineHas(dropsStatLine(), " draw-calls=3 draw-pushed=3 "), "each copy was not one draw call: " + dropsStatLine());
            require(layoutNow() == vanillaLayout, "the lists were left changed");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_table_only_dropitem_refuses") {
            // A DropItem hook that went in table-only refuses the drops whole, on a line of their own; the
            // game's calls pass it by and nothing is pushed.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            tableOnlyHook = "DropItem";
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=off install=refused gate=refused kill-gate=refused draw-gate=refused",
                    "the reply: " + lastLine("mark1set:"));
            require(anyLineHas("mark1set drops: refused - ", "DropItem went in TABLE-ONLY"), "the refusal line: " + lastLine("mark1set drops:"));
            require(lineHas(dropsStatLine(), " draw-hook=TABLE-ONLY "), "draw-hook: " + dropsStatLine());
            pickPolicy = Pick::Last;
            dropItem(monster);
            require(dropItemLayoutAtDraw[0] == vanillaLayout && countPieceBuilds(-1) == 0, "a table-only hook changed the draw");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "mark1_draw_table_only_tier_refuses") {
            // A LoadSatanicDropTier hook that went in table-only refuses the drops whole too: the draw's
            // tier would go unrecorded.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            tableOnlyHook = "LoadSatanicDropTier";
            dropsSwitch(true);
            require(lastLine("mark1set:") == "mark1set: drops=off install=refused gate=refused kill-gate=refused draw-gate=refused",
                    "the reply: " + lastLine("mark1set:"));
            require(anyLineHas("mark1set drops: refused - ", "LoadSatanicDropTier went in TABLE-ONLY"), "the refusal line: " + lastLine("mark1set drops:"));
            require(lineHas(dropsStatLine(), " draw-hook=detoured tier-hook=TABLE-ONLY "), "the hooks: " + dropsStatLine());
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "flight_draw_hit_builds_pocket_elf") {
            // Pocket Elf's switch-on hooks the draw too; each DropItem call carries its SS stand-in
            // (Aimbot.exe) once on each of [0..5]; a typed pick of it is Pocket Elf's at c in n_k + c
            // (n_3 0, so every one), rewritten to Pocket Elf's own definition, built and counted on the
            // `elf-draw=` tally, the DropUniqueItems route's own tally untouched.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropItem"] == 1 && dropItemEntry != gameDropItem && installs["LoadSatanicDropTier"] == 1,
                    "Pocket Elf's switch-on did not hook the draw, detoured");
            require(lineHas(flightStatLine(), " elf-draw-hook=detoured elf-draw-calls=0 elf-draw-pushed=0 elf-draw=0/0 elf-draw-standin=pending"),
                    "the elf's draw fields before the first call: " + flightStatLine());
            const std::string sig = sigdropStatusLine();
            pickPolicy = Pick::Last;
            for (int i = 0; i < 4; ++i) dropItem(monster);
            require(layoutsCarry(0, true, false), "a draw did not read every list carrying Aimbot.exe once");
            require(layoutNow() == vanillaLayout, "after the calls the lists are not the game's own");
            require(builds.size() == 4 && countFlightBuilds((int)FcNs::Charm::Elf) == 4, "every Aimbot.exe pick (n_3 0) was not built as Pocket Elf");
            for (const auto& b : builds) require(b.t == 10 && b.b == 0 && b.c == 0 && b.j == 0, "a Pocket Elf was not built from its own definition");
            require(linesEndingWith("flightcharm hit:", "kill draw -> Pocket Elf (stand-in 10/0/76, 1 in 1, k 3) built by the game") == 4,
                    "a hit line does not say the game built Pocket Elf: " + lastLine("flightcharm hit:"));
            require(lineHas(flightStatLine(), " elf-draw-hook=detoured elf-draw-calls=4 elf-draw-pushed=4 elf-draw=4/4 elf-draw-standin=0:76@6/5x0,0,0,0,0,1"),
                    "the elf's draw fields after the calls: " + flightStatLine());
            require(g_FlightDrop.tally[(int)FcNs::Charm::Elf].ours == 0 && g_FlightDrop.injected == 24 && g_FlightDrop.anomalies == 0,
                    "the DropUniqueItems route's tally moved, or the draw's pushes were not counted");
            const std::string after = sigdropStatusLine();
            require(after.substr(0, after.find(" cdpCalls=")) == sig.substr(0, sig.find(" cdpCalls=")), "`sigdrop status` moved: " + after);
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "flight_draw_and_mark1_each_hit_their_own") {
            // Both on, one DropItem detour and one tier detour: each call carries Pocket Elf's stand-in and
            // then the pieces'; a pick of Aimbot.exe is Pocket Elf's, a pick of the boots' stand-in Mark 1
            // Boots', each counted on its own feature's draw tally alone.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropItem"] == 1 && installs["LoadSatanicDropTier"] == 1, "a second DropItem or tier hook went in");
            pickPolicy = Pick::Among; pickTriples = { kElfDrawSs, kBootsDrawSs };
            for (int i = 0; i < 400; ++i) dropItem(monster);
            require(layoutsCarry(0, true, true), "a list did not carry Pocket Elf's entry and then the pieces'");
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
            const int elf = countFlightBuilds((int)FcNs::Charm::Elf), boots = countPieceBuilds(2);
            require(builds.size() == 400 && elf + boots == 400 && elf > 100 && boots > 100,
                    "the picks were not each built as their own item: elf " + std::to_string(elf) + ", boots " + std::to_string(boots));
            require(lineHas(flightStatLine(), " elf-draw=" + std::to_string(elf) + "/" + std::to_string(elf) + " "), "Pocket Elf's draw tally: " + flightStatLine());
            const M1dNs::DropStatFields f = dropsFields();
            require((int)f.drawPieces[2].ours == boots && (int)f.drawPieces[2].built == boots, "the boots' draw tally is not their builds");
            require(g_FlightDrop.anomalies == 0 && f.anomalies == 0, "a removal found someone else's tail");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "flight_draw_nested_angelic_sees_the_lists_as_before") {
            // Pocket Elf's draw entries are off the lists while #74's Angelic roll runs inside the call,
            // and back on for the draw after it.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            flightSwitch(FcNs::Charm::Elf, true);
            dropItemNestsAngelic = true;
            outcomes.assign({ false });
            pickPolicy = Pick::Last;
            dropItem(monster);
            require(originalCalls == 1 && layoutDuringCall[0] == vanillaLayout, "the nested Angelic roll saw Pocket Elf's draw entry");
            require(layoutsCarry(0, true, false) && countFlightBuilds((int)FcNs::Charm::Elf) == 1, "the draw after the nested roll did not hit as before");
            require(layoutNow() == vanillaLayout && g_KillDraw.aside == 0, "the lists or the state were left changed");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "flight_draw_refuses_whole_when_not_inline") {
            // A table-only DropItem hook: Pocket Elf's switch-on refuses whole on one line and turns its
            // switch off; the game's calls pass the hook by.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            tableOnlyHook = "DropItem";
            flightSwitch(FcNs::Charm::Elf, true);
            require(!g_FlightDrop.switches[(int)FcNs::Charm::Elf], "the refused switch stayed on");
            require(anyLineHas("flightcharm: refused - ", "elf: DropItem went in TABLE-ONLY") && anyLineHas("flightcharm: refused - ", "Pocket Elf stays off"),
                    "the refusal line: " + lastLine("flightcharm: refused"));
            require(lineHas(flightStatLine(), " elf-draw-hook=TABLE-ONLY "), "elf-draw-hook: " + flightStatLine());
            pickPolicy = Pick::Last;
            dropItem(monster);
            require(dropItemLayoutAtDraw[0] == vanillaLayout && countFlightBuilds(-1) == 0, "a table-only hook changed the draw");
#else
            require(false, "this source has no kill draw");
#endif
        // ---- the kill draw's typed-build counters (#166, the draw's positive control): baseline ----
        } else if (test == "draw_typed_fresh_lines_end_zeroed") {
            // A fresh session: `mark1set stat`'s drops line and `flightcharm stat` each end with their
            // typed-build fields, every one 0, after the line's last field before them.
            const std::string drops = dropsStatLine(), flight = flightStatLine();
            require(lineEndsWith(drops, " draw-standin-gloves=none" + drawTypedTail(0, 0, 0, 0, 0, 0, 0, 0) + drawClassTail({})),
                    "the fresh drops line does not end with the zeroed typed fields: " + drops);
            require(lineEndsWith(flight, " elf-draw-standin=none" + drawTypedTail(0, 0, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail({}, "elf-draw-")),
                    "the fresh flight line does not end with the zeroed typed fields: " + flight);
        } else if (test == "draw_typed_switches_off_counts_nothing") {
            // Both features on and off again (the draw's hooks stay in): DropItem calls that build the
            // game's own unique and a normal item run as the game's own - each call, its tier and both
            // CreateDefaultParams once, the lists unchanged, nothing rewritten - and no typed field moves.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            require(installs["DropItem"] == 1 && installs["LoadSatanicDropTier"] == 1, "the switch-on did not hook the draw");
            dropsSwitch(false);
            flightSwitch(FcNs::Charm::Elf, false);
            dropItemNormal = true;
            pickPolicy = Pick::First;
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(dropItemCalls == 3 && satTierCalls == 3 && gameCdpCalls == 6, "switched off, DropItem, its tier or its two builds did not run once each");
            for (const auto& seen : dropItemLayoutAtDraw) require(seen == vanillaLayout, "switched off, the draw read a changed list");
            require(builds.size() == 6 && countPieceBuilds(-1) == 0 && countFlightBuilds(-1) == 0, "switched off, a piece or a charm was built");
            int normal = 0, unique = 0;
            for (const auto& b : builds) {
                if (b.c == 1.0) { ++unique; require(b.t == 3 && b.j == 2 && b.b == 90 && b.a == kLootGroundSeed, "switched off, the game's own unique was changed"); }
                else { ++normal; require(b.c == 0.0 && b.j == 0 && b.b == 900 && b.a == kLootGroundSeed, "switched off, the normal item was changed"); }
            }
            require(normal == 3 && unique == 3, "switched off, not one unique and one normal item per call");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 0, 0, 0, 0, 0, 0, 0) + drawClassTail({})), "switched off, the drops line's typed fields moved: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(0, 0, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail({}, "elf-draw-")),
                    "switched off, the flight line's typed fields moved: " + flightStatLine());
#else
            require(false, "this source has no kill draw");
#endif
        // ---- the kill draw's typed-build counters (#166): target, every scenario fails before them ----
        } else if (test == "draw_typed_natural_counts_on_both_lines") {
            // Both on: the draw reads the twenty ordinary uniques on [3], refuses each and keeps the
            // game's own Satanic unique, a typed natural pick on both lines (`draw-typed=0/1`,
            // `elf-draw-typed=0/1`), built as the game's own.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            pickPolicy = Pick::First;
            dropItem(monster);
            require(layoutsCarry(0, true, true), "the draw did not read the lists carrying both features' entries");
            require(builds.size() == 1 && builds[0].t == 3 && builds[0].j == 2 && builds[0].b == 90 && builds[0].c == 1.0,
                    "the draw did not build the game's own unique");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 1, 0, 0, 0, 0, 0, 0) + drawClassTail(boolAgree(1))), "the drops line's typed fields: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(0, 1, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail(boolAgree(1), "elf-draw-")),
                    "the flight line's typed fields: " + flightStatLine());
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_typed_standin_counts_on_its_own_line") {
            // A typed pick of a piece's stand-in is Mark 1's <standin> and Pocket Elf's <natural>, every
            // pick, whatever Mark 1's coin says (the body armor at k 2 is the piece's 1 in 3); a typed
            // pick of Pocket Elf's stand-in is the reverse, Mark 1's coin taking no hit.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            pickPolicy = Pick::Among; pickTriples = { kBodyDrawSs };
            satTierAnswer = RValue(2.0);
            for (int i = 0; i < 300; ++i) dropItem(monster);
            const long long ours = (long long)dropsFields().drawPieces[1].ours;
            require(ours >= 50 && ours <= 150, "a body armor pick at k 2 was not ours about 1 in 3: " + std::to_string(ours));
            require(countFlightBuilds(-1) == 0, "a piece's stand-in was built as a charm");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(300, 0, 0, 0, 0, 0, 0, 0) + drawClassTail(boolAgree(300))),
                    "every typed body armor pick is not Mark 1's stand-in: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(0, 300, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail(boolAgree(300), "elf-draw-")),
                    "every typed body armor pick is not Pocket Elf's natural: " + flightStatLine());
            pickTriples = { kElfDrawSs };
            satTierAnswer = RValue(3.0);
            for (int i = 0; i < 5; ++i) dropItem(monster);
            require(countFlightBuilds((int)FcNs::Charm::Elf) == 5 && (long long)dropsFields().drawPieces[1].ours == ours,
                    "Aimbot.exe picks were not Pocket Elf's alone");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(300, 5, 0, 0, 0, 0, 0, 0) + drawClassTail(boolAgree(305))), "Pocket Elf's stand-in is not Mark 1's natural: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(5, 300, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail(boolAgree(305), "elf-draw-")),
                    "Pocket Elf's stand-in is not its own line's stand-in: " + flightStatLine());
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_typed_nested_angelic_counts_aside") {
            // #74's Angelic roll nested in a pushing DropItem call builds its hit while the call's entries
            // are set aside: counted `outside` under `aside` on both lines, and no typed field moves.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            dropItemNestsAngelic = true;
            outcomes.assign({ true });
            dropItemDraws = 0;
            dropItem(monster);
            require(originalCalls == 1 && gameCdpCalls == 1 && builds.size() == 1, "the nested Angelic roll did not build its hit once");
            require(countPieceBuilds(-1) == 0 && countFlightBuilds(-1) == 0, "the nested roll's hit was taken for ours");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 0, 0, 0, 0, 1, 0, 0) + drawClassTail({})), "the drops line's typed fields: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(0, 0, 0, 0, 0, 1, 0, 0, "elf-draw-") + drawClassTail({}, "elf-draw-")),
                    "the flight line's typed fields: " + flightStatLine());
            require(layoutNow() == vanillaLayout && g_KillDraw.aside == 0, "the lists or the state were left changed");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_typed_normal_build_is_not_typed") {
            // The draw reads the boots' stand-in, then the call builds a normal item (third argument 0)
            // from that same pair before the unique build: the normal build is `draw-normal=`, gives no
            // hit and is never rewritten into a piece, and the read it left types the unique build after
            // it (the boots at n_3 0, every one ours). Pocket Elf is off, so its line does not move.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            dropItemNormalAtPick = true;
            pickPolicy = Pick::Last;
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(builds.size() == 6, "not one normal and one unique build per call");
            for (size_t i = 0; i + 1 < builds.size(); i += 2) {
                const BuildRecord& normal = builds[i];
                const BuildRecord& unique = builds[i + 1];
                require(normal.t == 6 && normal.c == 0.0 && normal.j == 14 && normal.b == 70 && normal.a == kLootGroundSeed,
                        "a normal build matching the read was rewritten into a piece");
                require(unique.t == 2 && unique.c == 0.0 && unique.j == 0, "the unique build after it was not Mark 1 Boots: the normal build spent the read");
            }
            require(countPieceBuilds(2) == 3 && dropsFields().drawPieces[2].ours == 3 && dropsFields().drawPieces[2].built == 3,
                    "the unique builds were not each the boots'");
            require(linesStartingWith("mark1set drops hit:") == 3, "a normal build raised a hit: " + lastLine("mark1set drops hit:"));
            require(lineEndsWith(dropsStatLine(), drawTypedTail(3, 0, 0, 3, 0, 0, 0, 0) + drawClassTail(boolAgree(3, 3))), "the drops line's typed fields: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(0, 0, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail({}, "elf-draw-")),
                    "Pocket Elf off, its typed fields moved: " + flightStatLine());
            require(layoutNow() == vanillaLayout, "the lists are not the game's own after the calls");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_typed_untyped_unique_build") {
            // The draw reads its pick and then another definition before the unique build: no read
            // matches the build, which is `draw-untyped=`, no hit, and the game's own stand-in.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            repoRead = RepoRead::PickedThenOther;
            pickPolicy = Pick::Last;
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(builds.size() == 3 && countPieceBuilds(-1) == 0, "an untyped build was taken for a piece");
            for (const auto& b : builds) require(b.t == 2 && b.j == 14 && b.b == 70 && b.c == 1.0 && b.a == kLootGroundSeed, "the game's own stand-in was rewritten");
            require(linesStartingWith("mark1set drops hit:") == 0, "an untyped build raised a hit");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 0, 3, 0, 0, 0, 0, 0) + drawClassTail(boolAgree(3))), "the drops line's typed fields: " + dropsStatLine());
#else
            require(false, "this source has no kill draw");
#endif
        // ---- the kill draw's class read (#166 replan 1): baseline ----
        } else if (test == "draw_class_fresh_lines_end_zeroed") {
            // A fresh session: each stat line ends with its typed-build fields and then its two class
            // fields, every one 0, exactly as the procedure and the guide quote them.
            const std::string drops = dropsStatLine(), flight = flightStatLine();
            require(lineEndsWith(drops, " draw-outside=busy:0,aside:0,lists:0,roll:0 draw-class=bool:0/0,num:0/0,none:0,other:0"
                                        " draw-class-check=0/0/0"),
                    "the fresh drops line does not end with the zeroed class fields: " + drops);
            require(lineEndsWith(flight, " elf-draw-outside=busy:0,aside:0,lists:0,roll:0 elf-draw-class=bool:0/0,num:0/0,none:0,other:0"
                                         " elf-draw-class-check=0/0/0"),
                    "the fresh flight line does not end with the zeroed class fields: " + flight);
        } else if (test == "draw_class_switches_off_bool_class_counts_nothing") {
            // The game as the static reading has it: DropItem's unique build passes a bool true and its
            // normal build a bool false, and the record CreateDefaultParams returns reads `c` 1.0 and 0.0.
            // Neither switch touched (nothing hooked), every field of both lines stays the fresh one;
            // both on and off again (the draw's hooks stay in), no typed or class field moves.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropItemNormal = true;
            pickPolicy = Pick::First;
            const long conversions = refusedConversions;
            auto twoBuildsEach = [&](size_t from, const char* when) {
                require(builds.size() == from + 6, std::string(when) + ", not two builds per call");
                int normal = 0, unique = 0;
                for (size_t i = from; i < builds.size(); ++i) {
                    const BuildRecord& b = builds[i];
                    if (b.c == 1.0) { ++unique; require(b.t == 3 && b.j == 2 && b.b == 90 && b.a == kLootGroundSeed, std::string(when) + ", the game's own unique was changed"); }
                    else { ++normal; require(b.c == 0.0 && b.j == 0 && b.b == 900 && b.a == kLootGroundSeed, std::string(when) + ", the normal item was changed"); }
                }
                require(normal == 3 && unique == 3, std::string(when) + ", the records did not read c 1.0 and 0.0, one each per call");
            };
            for (int i = 0; i < 3; ++i) dropItem(monster);
            twoBuildsEach(0, "untouched");
            require(dropsStatLine() == kFreshDropsLine, "untouched, a drops field moved: " + dropsStatLine());
            require(flightStatLine() == kFreshFlightLine, "untouched, a flight field moved: " + flightStatLine());
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            dropsSwitch(false);
            flightSwitch(FcNs::Charm::Elf, false);
            for (int i = 0; i < 3; ++i) dropItem(monster);
            twoBuildsEach(6, "switched off");
            require(countPieceBuilds(-1) == 0 && countFlightBuilds(-1) == 0, "switched off, a piece or a charm was built");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 0, 0, 0, 0, 0, 0, 0) + drawClassTail({})),
                    "switched off, the drops line's typed or class fields moved: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(0, 0, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail({}, "elf-draw-")),
                    "switched off, the flight line's typed or class fields moved: " + flightStatLine());
            require(refusedConversions == conversions, "a bool class was converted (" + std::to_string(refusedConversions - conversions) + " refused)");
#else
            require(false, "this source has no kill draw");
#endif
        // ---- the kill draw's class read (#166 replan 1): target; the bool-true builds fail before it ----
        } else if (test == "draw_class_bool_true_is_typed") {
            // Both on: the game's own Satanic unique built with a bool true class is typed, a natural
            // pick on both lines, its class `bool:1/0` and the record's `c` agreeing with the verdict.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            pickPolicy = Pick::First;
            const long conversions = refusedConversions;
            dropItem(monster);
            require(builds.size() == 1 && builds[0].t == 3 && builds[0].j == 2 && builds[0].b == 90 && builds[0].c == 1.0,
                    "the draw did not build the game's own unique");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 1, 0, 0, 0, 0, 0, 0) + drawClassTail(boolAgree(1))),
                    "a bool true build was not typed on the drops line: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(0, 1, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail(boolAgree(1), "elf-draw-")),
                    "a bool true build was not typed on the flight line: " + flightStatLine());
            requireClassSums(dropsStatLine(), "draw-");
            requireClassSums(flightStatLine(), "elf-draw-");
            require(refusedConversions == conversions, "the bool class was converted");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_class_bool_true_standin_hits_at_copies_400") {
            // Both on at copies 400: a bool true pick of the boots' stand-in (n_3 0, every one ours) is
            // the piece's hit, rewritten and built by the game; a bool true pick of Pocket Elf's stand-in
            // is Pocket Elf's, built as the charm. Each is its own line's stand-in, the other's natural.
#ifdef HAS_KILLDRAWSTATE
            flightRepo();
            setDrawLayout();
            g_Mark1DropCopies = 400;
            g_FlightDropCopies = 400;
            dropsSwitch(true);
            flightSwitch(FcNs::Charm::Elf, true);
            pickPolicy = Pick::Among; pickTriples = { kBootsDrawSs };
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(countPieceBuilds(2) == 3 && dropsFields().drawPieces[2].ours == 3 && dropsFields().drawPieces[2].built == 3,
                    "a bool true pick of the boots' stand-in was not the piece's, built");
            require(linesStartingWith("mark1set drops hit:") == 3 && lineHas(lastLine("mark1set drops hit:"), "kill draw -> Mark 1 Boots (stand-in 2/14/70, ")
                        && lineHas(lastLine("mark1set drops hit:"), ") built by the game"),
                    "the boots' hit line: " + lastLine("mark1set drops hit:"));
            pickTriples = { kElfDrawSs };
            for (int i = 0; i < 3; ++i) dropItem(monster);
            require(countFlightBuilds((int)FcNs::Charm::Elf) == 3 && countPieceBuilds(-1) == 3, "a bool true pick of Aimbot.exe was not Pocket Elf's, built");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(3, 3, 0, 0, 0, 0, 0, 0) + drawClassTail(boolAgree(6))), "the drops line: " + dropsStatLine());
            require(lineEndsWith(flightStatLine(), drawTypedTail(3, 3, 0, 0, 0, 0, 0, 0, "elf-draw-") + drawClassTail(boolAgree(6), "elf-draw-")),
                    "the flight line: " + flightStatLine());
            requireClassSums(dropsStatLine(), "draw-");
            requireClassSums(flightStatLine(), "elf-draw-");
            require(layoutNow() == vanillaLayout && g_KillDraw.pushDepth == 0, "the lists or the state were left changed");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_class_real_numbers_by_the_truth_rule") {
            // A real class is read by the runner's truth rule: 1.0 and 0.6 are unique (typed), 0.4 and
            // 0 are not (normal), counted under `num`, and the record's `c` agrees each time.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            pickPolicy = Pick::First;
            const long conversions = refusedConversions;
            const double classes[] = { 1.0, 0.6, 0.4, 0.0 };
            for (double c : classes) { dropItemUniqueClass = RValue(c); dropItem(monster); }
            require(builds.size() == 4 && builds[0].c == 1.0 && builds[1].c == 1.0 && builds[2].c == 0.0 && builds[3].c == 0.0,
                    "the game's records did not read c by the truth rule");
            ClassCounts want; want.numTrue = 2; want.numFalse = 2; want.agree = 4;
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 2, 0, 2, 0, 0, 0, 0) + drawClassTail(want)), "the drops line: " + dropsStatLine());
            requireClassSums(dropsStatLine(), "draw-");
            requireClassSums(flightStatLine(), "elf-draw-");
            require(countPieceBuilds(-1) == 0 && refusedConversions == conversions, "a piece was built, or a class was refused a conversion");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_class_bool_false_at_the_read_is_normal") {
            // The negative control: a normal build with a bool false class whose sub and b are the
            // boots' stand-in, the latest read, is `normal` (`bool:0/1`), raises no hit and is never
            // rewritten into a piece; the read it left types the bool true build after it.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            dropItemNormalAtPick = true;
            pickPolicy = Pick::Last;
            dropItem(monster);
            require(builds.size() == 2, "not one normal and one unique build");
            require(builds[0].t == 6 && builds[0].c == 0.0 && builds[0].j == 14 && builds[0].b == 70 && builds[0].a == kLootGroundSeed,
                    "the bool false build matching the read was rewritten");
            require(countPieceBuilds(2) == 1 && builds[1].t == 2 && builds[1].j == 0, "the bool true build after it was not Mark 1 Boots");
            require(linesStartingWith("mark1set drops hit:") == 1, "the bool false build raised a hit");
            require(lineEndsWith(dropsStatLine(), drawTypedTail(1, 0, 0, 1, 0, 0, 0, 0) + drawClassTail(boolAgree(1, 1))), "the drops line: " + dropsStatLine());
            requireClassSums(dropsStatLine(), "draw-");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_class_absent_undefined_string_are_not_unique") {
            // The boots' stand-in picked each time, so a class misread as unique would hit: a real 0 is
            // `num:0/1`, an absent (argc 2) and an undefined third argument are `none`, a string is
            // `other`; each is normal, no hit, the game's own stand-in, and none is converted.
#ifdef HAS_KILLDRAWSTATE
            setDrawLayout();
            dropsSwitch(true);
            pickPolicy = Pick::Last;
            const long conversions = refusedConversions;
            dropItemUniqueClass = RValue(0.0); dropItem(monster);
            dropItemUniqueClass = makeBool(true); dropItemUniqueArgc = 2; dropItem(monster);
            dropItemUniqueArgc = 3; dropItemUniqueClass = RValue(); dropItem(monster);
            dropItemUniqueClass = RValue("1"); dropItem(monster);
            require(refusedConversions == conversions, "the gate converted a class (" + std::to_string(refusedConversions - conversions) + " refused)");
            require(builds.size() == 4 && countPieceBuilds(-1) == 0 && linesStartingWith("mark1set drops hit:") == 0, "a build that is not unique raised a hit");
            for (const auto& b : builds) require(b.t == 2 && b.j == 14 && b.b == 70 && b.c == 0.0 && b.a == kLootGroundSeed, "the game's own stand-in was rewritten");
            ClassCounts want; want.numFalse = 1; want.none = 2; want.other = 1; want.agree = 4;
            require(lineEndsWith(dropsStatLine(), drawTypedTail(0, 0, 0, 4, 0, 0, 0, 0) + drawClassTail(want)), "the drops line: " + dropsStatLine());
            requireClassSums(dropsStatLine(), "draw-");
#else
            require(false, "this source has no kill draw");
#endif
        } else if (test == "draw_class_check_disagrees_with_an_opposite_c" || test == "draw_class_check_unread_without_c") {
            // The fake game stores a `c` opposite to the argument's truth, or none: the check reads
            // `0/1/0` or `0/0/1`, and the typing, the hit, the built piece and the record handed back
            // are as without the check.
#ifdef HAS_KILLDRAWSTATE
            const bool opposite = test == "draw_class_check_disagrees_with_an_opposite_c";
            setDrawLayout();
            dropsSwitch(true);
            cdpClassStore = opposite ? CdpClassStore::Opposite : CdpClassStore::Missing;
            pickPolicy = Pick::Last;
            for (int i = 0; i < 2; ++i) dropItem(monster);
            require(cdpReturnedOther == 0, "CreateDefaultParams handed back something other than the game's record");
            require(countPieceBuilds(2) == 2 && dropsFields().drawPieces[2].ours == 2 && dropsFields().drawPieces[2].built == 2,
                    "the boots' picks were not typed, hit and built as without the check");
            require(linesEndingWith("mark1set drops hit:", "kill draw -> Mark 1 Boots (stand-in 2/14/70, 1 in 1, k 3) built by the game") == 2,
                    "the hit line: " + lastLine("mark1set drops hit:"));
            ClassCounts want; want.boolTrue = 2;
            (opposite ? want.disagree : want.unread) = 2;
            require(lineEndsWith(dropsStatLine(), drawTypedTail(2, 0, 0, 0, 0, 0, 0, 0) + drawClassTail(want)), "the drops line: " + dropsStatLine());
            requireClassSums(dropsStatLine(), "draw-");
#else
            require(false, "this source has no kill draw");
#endif
        } else return 2;
        std::cout << "PASS " << test << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL " << test << ": " << error.what() << '\n';
        return 1;
    }
}
