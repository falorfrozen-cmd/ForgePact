// Behavioral regression harness for the relic drop pool filter's diagnostic.
//
// The Python runner injects the REAL ForgePact::RelicFilterMod class and the
// REAL Hook_DropRelic body below. Only the game API is replaced; no game
// process, character, installed DLL or release asset is touched.
//
// Why this exists rather than more source-string assertions: origin's review
// of PR #4 showed the log line could report the scan's INPUT count before the
// guards and writes that decide whether anything is held back had run, so it
// announced a working filter on paths where nothing was applied. Only a test
// that runs the hook end to end and compares what was logged against what was
// actually written to the repository can tell those apart.
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_BOOL };

struct FakeStruct;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    FakeStruct* m_Object = nullptr;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(int n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(FakeStruct* p) : m_Kind(VALUE_OBJECT), m_Object(p) {}
    double ToDouble() const { return number; }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};
struct FakeStruct { std::map<std::string, RValue> fields; };
struct CInstance { int id = 0; };

// ---- the controlled world -------------------------------------------------
struct World {
    std::unordered_set<int> maxed;      // what the SDK scan returns
    bool playerResolves = true;         // HhResolveLocalPlayer
    bool repoLookupWorks = true;        // RepoStruct / RepoIndexValid
    bool scanThrows = false;
    // The two ways a repository WRITE fails after a successful lookup, both
    // reported 2026-09-15. A throw is swallowed by the hook's own catch; an
    // unset return is CallBuiltin's documented failure signal and writes
    // nothing while looking like an ordinary call.
    bool setThrows = false;
    bool setFailsSilently = false;
    int failWriteForRelic = -1;         // silent write failure for ONE relic only

    long baseSuppressions = 0;          // droprate.base <- 1e18
    long baseRestores = 0;              // droprate.base <- original
    long origCalls = 0;                 // the game's own DropRelic
    std::vector<std::string> log;
};
static World world;

static std::vector<std::unique_ptr<FakeStruct>> g_arena;
static std::unordered_map<int, FakeStruct*> g_repo;   // relic id -> repo struct

static FakeStruct* newStruct() {
    g_arena.push_back(std::make_unique<FakeStruct>());
    return g_arena.back().get();
}

// A repo entry is { droprate: { base: <n> } }, the shape the hook walks.
static FakeStruct* makeRepoEntry(double base, int relicId) {
    FakeStruct* dr = newStruct();
    dr->fields["base"] = RValue(base);
    dr->fields["__relicid"] = RValue(relicId);   // harness tag, so a write can fail per relic
    FakeStruct* item = newStruct();
    item->fields["droprate"] = RValue(dr);
    return item;
}

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        const std::string fn = name;
        if (fn == "variable_struct_get") {
            if (args.empty() || !args[0].m_Object) return RValue();
            auto it = args[0].m_Object->fields.find(args[1].ToString());
            return it == args[0].m_Object->fields.end() ? RValue() : it->second;
        }
        if (fn == "variable_struct_set") {
            if (args.empty() || !args[0].m_Object) return RValue();
            const std::string key = args[1].ToString();
            const bool suppressing = key == "base" && args[2].ToDouble() >= 1e17;
            if (suppressing && world.setThrows) throw std::runtime_error("struct set failed");
            int writingFor = -1;
            auto tag = args[0].m_Object->fields.find("__relicid");
            if (tag != args[0].m_Object->fields.end()) writingFor = static_cast<int>(tag->second.ToDouble());
            if (suppressing && (world.setFailsSilently || writingFor == world.failWriteForRelic))
                return RValue();  // unset, writes nothing
            if (key == "base") {
                if (suppressing) ++world.baseSuppressions;
                else ++world.baseRestores;
            }
            args[0].m_Object->fields[key] = args[2];
            return RValue();
        }
        if (fn == "variable_struct_exists") {
            if (args.empty() || !args[0].m_Object) return RValue(0.0);
            return RValue(args[0].m_Object->fields.count(args[1].ToString()) ? 1.0 : 0.0);
        }
        return RValue();
    }

    // The status-returning variant. `setFailsSilently` deliberately reports
    // SUCCESS while writing nothing, so the only thing that can catch it is
    // reading the value back - which is the point of the case.
    int CallBuiltinEx(RValue& result, const char* name, CInstance*, CInstance*, std::vector<RValue> args) {
        result = CallBuiltin(name, args);
        return 0;
    }

    void GetGlobalInstance(CInstance** out) {
        static CInstance instance;
        if (out) *out = &instance;
    }
};
using AurieStatus = int;
static bool AurieSuccess(AurieStatus status) { return status == 0; }
static FakeRunner g_Runner;
static FakeRunner* g_Yytk = &g_Runner;

static void Out(const std::string& s) { world.log.push_back(s); }

// ---- the pieces of ModuleMain.cpp the hook leans on ----------------------
static constexpr int kSeason10RelicRepoCount = 156;
static bool RepoIndexValid(int category, int index) {
    return category == 16 && index >= 0 && index < kSeason10RelicRepoCount;
}
static bool RepoStruct(int category, int index, RValue& out) {
    if (!world.repoLookupWorks || !RepoIndexValid(category, index)) return false;
    auto it = g_repo.find(index);
    if (it == g_repo.end()) return false;
    out = RValue(it->second);
    return true;
}

static bool HhResolveLocalPlayer(RValue& out) {
    if (!world.playerResolves) return false;
    out = RValue(newStruct());
    out.m_Kind = VALUE_REF;          // what this runner really hands back
    return true;
}

namespace HeroSiege { namespace Player {
inline std::unordered_set<int> GetMaxedRelicIds(FakeRunner*, const RValue&) {
    if (world.scanThrows) throw std::runtime_error("read failed");
    return world.maxed;
}
}}

#define BP_DIAG_INCREMENT(counter) ((void)0)
#define BP_LOGDROP(name, res, argc, argv) ((void)0)
static volatile long g_cnt_DropRelic = 0;
static int g_mult_DropRelic = 1;

using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);
static RValue& FakeOriginal(CInstance*, CInstance*, RValue& result, int, RValue**) {
    ++world.origCalls;
    return result;
}
static PFUNC_YYGMLScript g_Orig_DropRelic = &FakeOriginal;

// PRODUCTION_RELICFILTER

// PRODUCTION_FUNCTIONS

// ---- scenarios ------------------------------------------------------------
static void reset() {
    world = World();
    g_arena.clear();
    g_repo.clear();
    for (int i = 0; i < kSeason10RelicRepoCount; ++i) g_repo[i] = makeRepoEntry(1000.0 + i, i);
}

static void runHook() {
    RValue result;
    Hook_DropRelic(nullptr, nullptr, result, 0, nullptr);
}

static void report(const char* label) {
    std::cout << "SCENARIO " << label
              << " suppressed=" << world.baseSuppressions
              << " restored=" << world.baseRestores
              << " origcalls=" << world.origCalls << "\n";
    for (const std::string& line : world.log) std::cout << "LOG " << label << " :: " << line << "\n";
}

int main() {
    // The report line dedupes on its own text, so each scenario runs in a
    // fresh process-level state only for the world; the static in the hook
    // persists. Every scenario therefore emits a DIFFERENT line, which is
    // also the property we want: identical repeats stay quiet.

    // 1. Positive control: one maxed relic, everything available.
    reset();
    world.maxed = { 42 };
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, true);
    runHook();
    report("positive_control");

    // 2. Every relic maxed: the bypass deliberately applies nothing.
    reset();
    for (int i = 0; i < kSeason10RelicRepoCount; ++i) world.maxed.insert(i);
    runHook();
    report("all_maxed");

    // 3. Repository lookup fails: scan found one, nothing can be written.
    reset();
    world.maxed = { 42 };
    world.repoLookupWorks = false;
    runHook();
    report("repo_lookup_fails");

    // 4. No player instance yet: the scan never ran at all.
    reset();
    world.maxed = { 42 };
    world.playerResolves = false;
    runHook();
    report("no_player");

    // 5. Scan ran and the player simply owns no maxed relics.
    reset();
    runHook();
    report("scanned_none_maxed");

    // 6. The lookup succeeds and the suppression write THROWS. The hook's own
    //    catch swallows it, so nothing but a confirmed read-back can tell.
    reset();
    world.maxed = { 42 };
    world.setThrows = true;
    runHook();
    report("write_throws");

    // 7. The write reports success and changes nothing - CallBuiltin's
    //    documented unset-on-failure shape. A status check alone passes here.
    reset();
    // Two relics, so this scenario's line differs from case 6's: the report
    // dedupes on its own text, which is correct in a session and means the
    // harness must not run two scenarios that would word themselves alike.
    world.maxed = { 7, 8 };
    world.setFailsSilently = true;
    runHook();
    report("write_fails_silently");

    // 8. Two maxed relics, one write lands and one does not: the count must be
    //    the confirmed one, and the shortfall must be named.
    reset();
    world.maxed = { 11, 12 };
    world.failWriteForRelic = 12;
    runHook();
    report("partial_write");

    std::cout << "HARNESS DONE\n";
    return 0;
}
