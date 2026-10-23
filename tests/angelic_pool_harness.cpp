// The Python test (test_angelic_pool_behavior.py) inserts the real plugin types and functions
// below, compiled as the player build (FORGEPACT_RELEASE): the Angelic / Unholy Drops slider's
// table (kAngelicBases), its pool build (BuildAngelicPool), the die on a kill (AngelicDropOnKill),
// the spawn (SpawnAngelicItem) and the status line (AngelicDropStatus). Only the game is
// modelled: per (type, sub, b) the answers the pool build validates a row by - the definition's
// name key (itemBaseInfoStruct "28", or no definition at all), its hidden flag ("40") and the
// rarity InitItemFromJson builds the item at (itemInfoStruct "27") - plus a recording
// InitItemFromJson and LootGroundCreateFromItem. No game process, character, installed DLL or
// release asset is touched.
//
// The model answers a potion (item type 18) exactly as it answers every other row: nothing in it
// depends on the type, so a row the pool build refuses for its type is refused by the plugin,
// never by the model (#229).
#define FORGEPACT_RELEASE
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
// A struct is a reference, as in GameMaker: a copy of the RValue shares it.
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    CInstance* instance = nullptr;
    std::string text;
    std::shared_ptr<std::map<std::string, RValue>> fields;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    double ToDouble() const {
        if (m_Kind != VALUE_REAL && m_Kind != VALUE_INT32 && m_Kind != VALUE_INT64 && m_Kind != VALUE_BOOL) throw std::runtime_error("not a number");
        return number;
    }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};
// A monster: its enemyRarity and position, the fields AngelicDropOnKill reads through HhReadNumber.
struct CInstance {
    double enemyRarity = 1.0, x = 0, y = 0;
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};
static RValue makeStruct(std::map<std::string, RValue> values) {
    RValue r; r.m_Kind = VALUE_OBJECT; r.fields = std::make_shared<std::map<std::string, RValue>>(std::move(values)); return r;
}

enum AurieStatus { AURIE_SUCCESS = 0, AURIE_OBJECT_NOT_FOUND = 7 };
static bool AurieSuccess(AurieStatus s) { return s == AURIE_SUCCESS; }
static long InterlockedIncrement(volatile long* value) { return ++*const_cast<long*>(value); }
// The clock the pool build's retry throttle reads; a fresh process per scenario starts it here.
static unsigned long long GetTickCount64() { return 1000; }

// ---- log ---------------------------------------------------------------------------------
static std::vector<std::string> outLines;
static void Out(const std::string& line) { outLines.push_back(line); }
static bool hasLine(const std::string& line) {
    for (const auto& l : outLines) if (l == line) return true;
    return false;
}
static std::string lineStarting(const std::string& prefix) {
    for (const auto& l : outLines) if (l.rfind(prefix, 0) == 0) return l;
    return std::string();
}

// ---- the game's answers, per (type, sub, b) ---------------------------------------------------
// A triple the model has no answer for gives no definition (GetUniqueRepoStruct returns undefined),
// which the pool build refuses as `no def`; a scenario names the triples it answers.
using Triple = std::array<int, 3>;
struct Answer {
    std::string key;        // itemBaseInfoStruct "28" and the built item's itemInfoStruct "28"
    bool hidden = false;    // itemBaseInfoStruct "40"
    double built = 7.0;     // the rarity InitItemFromJson builds the item at, itemInfoStruct "27"
};
static std::map<Triple, Answer> answers;
static std::map<Triple, int> repoReads;   // GetUniqueRepoStruct calls per triple
struct BuildCall { std::string key; double w, a, j, b, c; };
static std::vector<BuildCall> builds;     // InitItemFromJson calls, in order
struct PlaceCall { double x, y; CInstance* self; std::string itemKey; double itemRarity; };
static std::vector<PlaceCall> placements; // LootGroundCreateFromItem calls, in order
static std::vector<std::string> otherScripts;   // any other script the plugin tried to call

static std::map<std::string, RValue>& obj(const RValue& v) {
    if (v.m_Kind != VALUE_OBJECT || !v.fields) throw std::runtime_error("not a struct");
    return *v.fields;
}
static double numberOf(const RValue& s, const char* name) {
    auto& f = obj(s);
    auto it = f.find(name);
    return it == f.end() ? -1.0 : it->second.number;
}

static CInstance globalInstance;
struct FakeYytk {
    RValue CallBuiltin(const char* functionName, std::vector<RValue> a) {
        const std::string n(functionName);
        if (n == "variable_struct_exists") return RValue(obj(a[0]).count(a[1].text) ? 1.0 : 0.0);
        if (n == "variable_struct_get") { auto& f = obj(a[0]); auto it = f.find(a[1].text); return it == f.end() ? RValue() : it->second; }
        if (n == "json_parse") {   // {"w":1,"a":<seed>,"j":<sub>,"b":<b>,"c":1}
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
    // GetUniqueRepoStruct(type, sub, b): the definition, or undefined for a triple the model does not know.
    RValue CallGameScript(std::string_view name, const std::vector<RValue>& a) {
        if (name != "gml_Script_GetUniqueRepoStruct") { otherScripts.emplace_back(name); throw std::runtime_error("unmodelled script"); }
        const Triple t{ (int)a[0].ToDouble(), (int)a[1].ToDouble(), (int)a[2].ToDouble() };
        ++repoReads[t];
        const auto it = answers.find(t);
        if (it == answers.end()) return RValue();
        std::map<std::string, RValue> info{ { "28", RValue(it->second.key) } };
        if (it->second.hidden) info["40"] = RValue(1.0);
        return makeStruct({ { "itemBaseInfoStruct", makeStruct(std::move(info)) } });
    }
    // InitItemFromJson(parsed, "0-0-<n>-<type>") builds the item at the answered rarity;
    // LootGroundCreateFromItem(x, y, item) places it, with the caller's self.
    AurieStatus CallGameScriptEx(RValue& out, std::string_view name, CInstance* self, CInstance*, std::vector<RValue> a) {
        out = RValue();
        if (name == "gml_Script_InitItemFromJson") {
            const std::string& key = a[1].text;
            BuildCall call{ key, numberOf(a[0], "w"), numberOf(a[0], "a"), numberOf(a[0], "j"), numberOf(a[0], "b"), numberOf(a[0], "c") };
            builds.push_back(call);
            const Triple t{ std::atoi(key.c_str() + key.rfind('-') + 1), (int)call.j, (int)call.b };
            const auto it = answers.find(t);
            if (it != answers.end())
                out = makeStruct({ { "itemInfoStruct", makeStruct({ { "27", RValue(it->second.built) }, { "28", RValue(it->second.key) } }) } });
            return AURIE_SUCCESS;
        }
        if (name == "gml_Script_LootGroundCreateFromItem") {
            const RValue info = obj(a[2])["itemInfoStruct"];
            placements.push_back({ a[0].ToDouble(), a[1].ToDouble(), self, obj(info)["28"].text, obj(info)["27"].number });
            return AURIE_SUCCESS;
        }
        otherScripts.emplace_back(name);
        return AURIE_OBJECT_NOT_FOUND;
    }
    void GetGlobalInstance(CInstance** out) { *out = &globalInstance; }
};
static FakeYytk fakeYytk;
static FakeYytk* g_Yytk = &fakeYytk;

// ---- what the production functions read ------------------------------------------------------
static std::mt19937& TyRng() { static std::mt19937 rng{ 7 }; return rng; }
static double HhReadNumber(const RValue& value, const char* field, double fallback) {
    CInstance* instance = value.instance;
    if (!instance) return fallback;
    const std::string name(field);
    if (name == "enemyRarity") return instance->enemyRarity;
    if (name == "x") return instance->x;
    if (name == "y") return instance->y;
    return fallback;
}

// PRODUCTION_TYPES

static std::vector<AngelicCandidate> g_AngelicPool;
static bool g_AngelicPoolBuilt = false;
static double g_AngelicDropOneIn = 0.0;
static volatile long g_AngelicDropRolls = 0, g_AngelicDropHits = 0, g_AngelicDropFails = 0;
static volatile long g_KillsSeen = 0;

// PRODUCTION_FUNCTIONS

// ---- scenarios ---------------------------------------------------------------------------------
static void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}
static const Triple kPotion{ 18, 0, 8 }, kElixir{ 18, 0, 5 }, kCrown{ 0, 0, 85 }, kPegLeg{ 2, 0, 79 }, kDevCharm{ 10, 0, 78 };
static const AngelicBase& row(const Triple& t) {
    for (const AngelicBase& b : kAngelicBases) if (b.type == t[0] && b.sub == t[1] && b.b == t[2]) return b;
    throw std::runtime_error("kAngelicBases has no row " + std::to_string(t[0]) + "/" + std::to_string(t[1]) + "/" + std::to_string(t[2]));
}
// The game agreeing with the table on every row: its key, no hidden flag, built Angelic.
static void answerEveryRowAsPassing() {
    for (const AngelicBase& b : kAngelicBases) answers[{ b.type, b.sub, b.b }] = Answer{ b.key, false, 7.0, };
}
static void answerOnly(const Triple& t, Answer a) { answers.clear(); answers[t] = std::move(a); }
static Answer passing(const Triple& t) { return Answer{ row(t).key, false, 7.0 }; }
static std::string listLine(const Triple& t, const std::string& verdict) {
    const AngelicBase& b = row(t);
    return std::string("angeliclist: ") + b.name + " (" + std::to_string(b.type) + "/" + std::to_string(b.sub) + "/" + std::to_string(b.b) + ") -> " + verdict;
}
static bool inPool(const Triple& t) {
    for (const AngelicCandidate& c : g_AngelicPool) if (c.type == t[0] && c.sub == t[1] && c.b == t[2]) return true;
    return false;
}
static std::string dumpLog() {
    std::string s;
    for (const auto& l : outLines) s += "\n  " + l;
    return s;
}
// One verbose build, as `angelicdrop list` runs it.
static void buildVerbose() { g_AngelicPoolBuilt = false; BuildAngelicPool(true); }
static std::string fmt(double v) { char b[32]; std::snprintf(b, sizeof b, "%g", v); return b; }
// One kill with the slider at `angelicdrop 1` (every kill rolls a hit), and what it must leave:
// one build of `t` through InitItemFromJson with a key ending -<type> and the row's j and b, one
// placement at the enemy's position with the enemy as self, and the status counting one drop.
static void killDropsExactly(const Triple& t, double builtRarity) {
    g_AngelicDropOneIn = 1.0;
    CInstance enemy; enemy.enemyRarity = 2.0; enemy.x = 812.0; enemy.y = 404.0;
    AngelicDropOnKill(&enemy);
    const std::string suffix = "-" + std::to_string(t[0]);
    require(builds.size() == 1, "InitItemFromJson ran " + std::to_string(builds.size()) + " times on the hit" + dumpLog());
    const BuildCall& b = builds[0];
    require(b.key.rfind("0-0-", 0) == 0 && b.key.size() > suffix.size() && b.key.compare(b.key.size() - suffix.size(), suffix.size(), suffix) == 0,
            "the build key does not end " + suffix + ": " + b.key);
    require(b.w == 1.0 && b.j == (double)t[1] && b.b == (double)t[2] && b.c == 1.0,
            "the build json is not {\"w\":1,...,\"j\":" + std::to_string(t[1]) + ",\"b\":" + std::to_string(t[2]) + ",\"c\":1}: j=" + fmt(b.j) + " b=" + fmt(b.b) + " c=" + fmt(b.c));
    require(placements.size() == 1, "LootGroundCreateFromItem ran " + std::to_string(placements.size()) + " times" + dumpLog());
    const PlaceCall& p = placements[0];
    require(p.x == 812.0 && p.y == 404.0 && p.self == &enemy, "the item was not placed at the enemy's position with the enemy as self");
    require(p.itemKey == row(t).key && p.itemRarity == builtRarity, "the placed item is not the row's: " + p.itemKey + " rarity " + fmt(p.itemRarity));
    require(hasLine(std::string("angelicdrop: ") + row(t).name + " -> " + row(t).key + " rarity " + std::to_string((int)builtRarity) + " at 812,404"),
            "no spawn line for " + std::string(row(t).name) + dumpLog());
    outLines.clear();
    AngelicDropStatus();
    const std::string status = lineStarting("angelicdrop: 1 in 1 kills");
    require(status.find(" | rolls=1 drops=1 fails=0 | ") != std::string::npos, "the status does not read drops=1 fails=0: " + status);
    require(otherScripts.empty(), "the plugin called a script the slider never calls: " + (otherScripts.empty() ? std::string() : otherScripts.front()));
}

static int run(const std::string& scenario) {
    // ---- baseline: holds on the source before #229 and after it ----
    if (scenario == "flagged_row_rejected") {
        answerEveryRowAsPassing();
        answers[kDevCharm].hidden = true;
        buildVerbose();
        require(hasLine(listLine(kDevCharm, "hidden/dev item (info flag 40 set)")), "a flagged row was not rejected for its flag" + dumpLog());
        require(!inPool(kDevCharm), "a flagged row is in the pool");
        require(inPool(kCrown), "an ordinary Angelic row left the pool beside the flagged one");
    } else if (scenario == "wrong_rarity_rejected") {
        answerEveryRowAsPassing();
        answers[kPegLeg].built = 9.0;
        buildVerbose();
        require(hasLine(listLine(kPegLeg, "game makes it rarity 9")), "a row the game builds at rarity 9 was not rejected for it" + dumpLog());
        require(!inPool(kPegLeg), "a row the game builds at rarity 9 is in the pool");
    } else if (scenario == "key_mismatch_rejected") {
        answerEveryRowAsPassing();
        answers[kCrown].key = "helmet_something_else";
        buildVerbose();
        require(hasLine(listLine(kCrown, "key mismatch: helmet_something_else")), "a row whose key the game does not agree with was kept" + dumpLog());
        require(!inPool(kCrown), "a row with a mismatched key is in the pool");
    } else if (scenario == "ordinary_angelic_validates_and_spawns") {
        answerOnly(kCrown, passing(kCrown));
        buildVerbose();
        require(hasLine(listLine(kCrown, "ok")), "an ordinary Angelic row did not validate" + dumpLog());
        require(g_AngelicPool.size() == 1 && inPool(kCrown), "the pool is not the one validated row: " + std::to_string(g_AngelicPool.size()));
        builds.clear();   // the validation's own probe build
        killDropsExactly(kCrown, 7.0);
    } else if (scenario == "slider_off_nothing_rolls") {
        answerEveryRowAsPassing();
        g_AngelicDropOneIn = 0.0;
        CInstance enemy; enemy.enemyRarity = 2.0;
        for (int i = 0; i < 50; ++i) AngelicDropOnKill(&enemy);
        require(g_AngelicDropRolls == 0 && builds.empty() && placements.empty() && repoReads.empty(), "the slider off still rolled, built or validated");
        AngelicDropStatus();
        require(lineStarting("angelicdrop: off | rolls=0 drops=0 fails=0") != "", "the status does not read off" + dumpLog());
    }
    // ---- target: #229, fails on the source before it ----
    else if (scenario == "potion_validates_as_angelic") {
        answerEveryRowAsPassing();
        buildVerbose();
        require(repoReads[kPotion] >= 1, "the pool build never asked the game about the potion" + dumpLog());
        require(hasLine(listLine(kPotion, "ok")), "the potion, answered as a real Angelic, did not validate" + dumpLog());
        require(inPool(kPotion), "the potion is not in the pool");
    } else if (scenario == "elixir_rejected_by_its_rarity_not_its_type") {
        answerEveryRowAsPassing();
        answers[kElixir].built = 9.0;   // Heroic, as HS Offline Tracker's item table lists it
        buildVerbose();
        require(hasLine(listLine(kElixir, "game makes it rarity 9")), "the Elixir built as Heroic was not rejected for its rarity" + dumpLog());
        require(!inPool(kElixir), "the Elixir built as Heroic is in the pool");
        require(inPool(kPotion), "the potion left the pool beside the Elixir");
    } else if (scenario == "potion_still_validated") {
        // Negative control: a potion goes through the same three tests, not around them.
        answerEveryRowAsPassing();
        answers[kPotion].hidden = true;
        buildVerbose();
        require(hasLine(listLine(kPotion, "hidden/dev item (info flag 40 set)")), "a flagged potion was not rejected for its flag" + dumpLog());
        require(!inPool(kPotion), "a flagged potion is in the pool");
        answers[kPotion] = passing(kPotion); answers[kPotion].built = 9.0; outLines.clear();
        buildVerbose();
        require(hasLine(listLine(kPotion, "game makes it rarity 9")), "a potion the game builds at rarity 9 was not rejected for it" + dumpLog());
        require(!inPool(kPotion), "a potion the game builds at rarity 9 is in the pool");
        answers[kPotion] = passing(kPotion); answers[kPotion].key = "consumable_other"; outLines.clear();
        buildVerbose();
        require(hasLine(listLine(kPotion, "key mismatch: consumable_other")), "a potion with a mismatched key was not rejected for it" + dumpLog());
        answers.erase(kPotion); outLines.clear();
        buildVerbose();
        require(hasLine(listLine(kPotion, "no def")), "a potion the game has no definition for was not rejected for it" + dumpLog());
    } else if (scenario == "potion_hit_builds_and_places") {
        answerOnly(kPotion, passing(kPotion));
        buildVerbose();
        require(g_AngelicPool.size() == 1 && inPool(kPotion), "the pool is not the potion alone: " + std::to_string(g_AngelicPool.size()) + dumpLog());
        builds.clear();
        killDropsExactly(kPotion, 7.0);
    } else if (scenario == "every_row_passing_fills_the_pool") {
        answerEveryRowAsPassing();
        buildVerbose();
        const size_t rows = std::size(kAngelicBases);
        require(g_AngelicPool.size() == rows, "the pool holds " + std::to_string(g_AngelicPool.size()) + " of " + std::to_string(rows) + " rows the game passes");
        require(hasLine("angelic pool: " + std::to_string(rows) + " candidates, 0 rejected"), "the pool line does not count every row" + dumpLog());
    } else {
        std::cerr << "unknown scenario " << scenario << "\n";
        return 2;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: angelic_pool <scenario>\n"; return 2; }
    try {
        return run(argv[1]);
    } catch (const std::exception& e) {
        std::cout << "FAILED " << argv[1] << ": " << e.what() << "\n";
        return 1;
    }
}
