// Behavioral regression harness for the relic drop pool filter (#125, #93).
//
// The Python runner injects the REAL ForgePact::RelicFilterMod class and the
// REAL Hook_GetRelicQuest, Hook_DropRelic, RelicFilterHookState,
// RelicFilterStatus and RelicFilterReportArmScan bodies below. Only the game
// API is replaced. No game process, character, installed DLL or release asset
// is touched.
//
// The game's relic pick is played by GameDraw below, as the hub's
// docs/models/relic-pick-spec.md reads it:
// - every routine that places a relic draws an id and draws again while
//   GetRelicQuest answers true;
// - GetRelicQuest is true only for the quest relics 141..155.
// A scenario scripts the draws. The relic that drops is the first one the
// hook answers false for, so each scenario shows what actually dropped, and
// never only what the filter said.
//
// Why a harness rather than source-string assertions:
// - ForgePact 2.0.1's filter logged "holding back 1 of 1" while the relic it
//   named kept dropping (#125). Its lever, a `droprate.base` write, is read by
//   no relic pick.
// - Origin's review of PR #4 had already shown that a log line can claim an
//   effect that did not happen.
// Only running the hook through the game's own loop tells those apart.
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
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

struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(int n) : m_Kind(VALUE_REAL), number(n) {}
    explicit RValue(bool b) : m_Kind(VALUE_BOOL), number(b ? 1.0 : 0.0) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const { return number; }
    bool ToBoolean() const { return m_Kind != VALUE_UNDEFINED && number != 0; }
    std::string ToString() const { return text; }
};
struct CInstance { int id = 0; };

struct FakeRunner {};
static FakeRunner g_Runner;
static FakeRunner* g_Yytk = &g_Runner;

// ---- the controlled world -------------------------------------------------
struct World {
    std::unordered_set<int> maxed;      // what the SDK scan returns
    bool playerResolves = true;         // HhResolveLocalPlayer
    bool scanThrows = false;
    bool rewardScopeActive = false;     // inside AFK FARM's reward delivery
    long scans = 0;                     // SDK scans actually made
    long reportRequests = 0;            // scans that asked for a report
    long questCalls = 0;                // the game's own GetRelicQuest ran
    long dropCalls = 0;                 // the game's own DropRelic ran
    // What the equipped-slot and relic-tab reads report when asked (#93, #125).
    const char* equippedStopped = nullptr;
    int equippedRelics = 0;
    const char* tabStopped = nullptr;
    int tabRelics = 0;
    std::vector<std::string> log;
};
static World world;

static void Out(const std::string& s) { world.log.push_back(s); }

static uint64_t g_RuntimeFrame = 1;

static bool HhResolveLocalPlayer(RValue& out) {
    if (!world.playerResolves) return false;
    out = RValue(1.0);
    out.m_Kind = VALUE_REF;          // what this runner really hands back
    return true;
}

namespace HeroSiege { namespace Player {
// Stand-ins for hs_game_sdk/player.hpp's two reports, reduced to what a
// scenario drives. The real reports and their formatters are tested in the
// hub (tests/cpp/test_sdk_player_hooks.cpp). These only show which report
// each arm line prints.
struct EquippedSlotScanReport {
    const char* stopped = "not-run";
    int relicInstances = 0;
};
struct RelicTabScanReport {
    const char* stopped = "not-run";
    int relicInstances = 0;
};
inline std::string FormatEquippedSlotScanReport(const EquippedSlotScanReport& r) {
    return "relic=" + std::to_string(r.relicInstances) + " stopped=" + (r.stopped ? r.stopped : "none");
}
inline std::string FormatRelicTabScanReport(const RelicTabScanReport& r) {
    return "relic=" + std::to_string(r.relicInstances) + " stopped=" + (r.stopped ? r.stopped : "none");
}
inline std::unordered_set<int> GetMaxedRelicIds(FakeRunner*, const RValue&,
                                                EquippedSlotScanReport* equipped = nullptr,
                                                RelicTabScanReport* tab = nullptr) {
    ++world.scans;
    if (equipped || tab) ++world.reportRequests;
    if (world.scanThrows) throw std::runtime_error("read failed");
    if (equipped) {
        equipped->stopped = world.equippedStopped;
        equipped->relicInstances = world.equippedRelics;
    }
    if (tab) {
        tab->stopped = world.tabStopped;
        tab->relicInstances = world.tabRelics;
    }
    return world.maxed;
}
}}

// The one hs_game_sdk/reward_scope.hpp query the hooks make: during AFK FARM's
// own reward delivery the game's drop runs untouched.
namespace HeroSiege { namespace RewardScope {
inline bool Active() { return world.rewardScopeActive; }
}}

#define BP_DIAG_INCREMENT(counter) ((void)0)
#define BP_LOGDROP(name, res, argc, argv) ((void)0)

// The incident monitor's per-mod timer (ForgePact #76, IncidentMonitor.hpp)
// opens Hook_DropRelic, and its guard wraps each call into the original. They
// only measure, so a no-op scope and the bare call stand in for them here.
enum class IncidentMod { drops };
struct IncidentScope { explicit IncidentScope(IncidentMod) noexcept {} };
#define FP_GAME_ORIGINAL(call) (call)

using PFUNC_YYGMLScript = RValue& (*)(CInstance*, CInstance*, RValue&, int, RValue**);

// The game's own GetRelicQuest: true for the quest relics 141..155 only.
static RValue& FakeGetRelicQuest(CInstance*, CInstance*, RValue& result, int argc, RValue** args) {
    ++world.questCalls;
    const int id = (argc >= 1 && args && args[0]) ? static_cast<int>(args[0]->ToDouble()) : -1;
    result = RValue(id >= 141 && id <= 155);
    return result;
}
static RValue& FakeDropRelic(CInstance*, CInstance*, RValue& result, int, RValue**) {
    ++world.dropCalls;
    result = RValue(true);
    return result;
}

static PFUNC_YYGMLScript g_Orig_DropRelic = &FakeDropRelic;
static volatile long g_cnt_DropRelic = 0;
static int g_mult_DropRelic = 1;

// PRODUCTION_RELICFILTER

// PRODUCTION_STATICS

// PRODUCTION_FUNCTIONS

// ---- the game's relic pick ------------------------------------------------
// The draws a scenario scripts as the pick's `irandom(155)` results, in order.
// The relic that drops is the first one GetRelicQuest answers false for. -1
// means every scripted draw was answered true.
static int GameDraw(const std::vector<int>& draws) {
    for (int id : draws) {
        RValue result;
        RValue arg(static_cast<double>(id));
        RValue* args[1] = { &arg };
        if (!Hook_GetRelicQuest(nullptr, nullptr, result, 1, args).ToBoolean()) return id;
    }
    return -1;
}

// ---- scenarios ------------------------------------------------------------
static void reset() {
    world = World();
    g_Orig_GetRelicQuest = &FakeGetRelicQuest;
    g_GetRelicQuestNative = true;
    g_mult_DropRelic = 1;
    ++g_RuntimeFrame;                    // every scenario starts on a new frame
    ForgePact::RelicFilterMod::Instance().SetTestMaxed({});
    ForgePact::RelicFilterMod::Instance().SetEnabled(false, true);
    world.log.clear();
}

static void report(const char* label, int dropped) {
    std::cout << "SCENARIO " << label
              << " dropped=" << dropped
              << " skips=" << ForgePact::RelicFilterMod::Instance().Skips()
              << " scans=" << world.scans
              << " questcalls=" << world.questCalls
              << " dropcalls=" << world.dropCalls
              << " reports=" << world.reportRequests << "\n";
    for (const std::string& line : world.log) std::cout << "LOG " << label << " :: " << line << "\n";
}

// The arm-time report: whether a report was due before and after it ran, so a
// scenario can show the line is emitted once per arm and not again.
static void runArmReport(const char* label) {
    const bool dueBefore = ForgePact::RelicFilterMod::Instance().IsArmScanDue();
    RelicFilterReportArmScan();
    const bool dueAfter = ForgePact::RelicFilterMod::Instance().IsArmScanDue();
    std::cout << "SCENARIO " << label
              << " due_before=" << (dueBefore ? 1 : 0)
              << " due_after=" << (dueAfter ? 1 : 0)
              << " questcalls=" << world.questCalls
              << " reports=" << world.reportRequests << "\n";
    for (const std::string& line : world.log) std::cout << "LOG " << label << " :: " << line << "\n";
}

static void arm() {
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, true);
    world.log.clear();
}

int main() {
    auto& rf = ForgePact::RelicFilterMod::Instance();

    // 1. Baseline: the filter is off, so the maxed relic drops as the game picked it.
    reset();
    world.maxed = { 140 };
    report("baseline_off", GameDraw({ 140, 5 }));

    // 2. Positive control: relic 140 is maxed; the game draws again and 5 drops.
    reset();
    world.maxed = { 140 };
    arm();
    report("positive_control", GameDraw({ 140, 5 }));

    // 3. A quest relic is the game's own reroll: nothing is counted as a skip.
    reset();
    world.maxed = { 140 };
    arm();
    report("quest_is_the_games", GameDraw({ 150, 3 }));

    // 4. A relic the player has not maxed drops untouched.
    reset();
    world.maxed = { 140 };
    arm();
    report("not_maxed", GameDraw({ 9 }));

    // 5. Every droppable relic maxed: nothing is left to drop instead, so the
    //    filter stands down rather than leave the game's loop without an end.
    reset();
    for (int id = 0; id <= 140; ++id) world.maxed.insert(id);
    arm();
    const int first = GameDraw({ 140 });
    const int second = GameDraw({ 7 });
    report("all_maxed", first == 140 && second == 7 ? 140 : -2);

    // 6. One relic left: every draw that is not it goes again.
    reset();
    for (int id = 0; id <= 140; ++id) if (id != 77) world.maxed.insert(id);
    arm();
    report("one_left", GameDraw({ 3, 140, 12, 77 }));

    // 7. Inside AFK FARM's reward scope: the game's answer, untouched.
    reset();
    world.maxed = { 140 };
    arm();
    world.rewardScopeActive = true;
    report("reward_scope", GameDraw({ 140, 5 }));

    // 8. No player yet: the scan did not run, so nothing is held back.
    reset();
    world.maxed = { 140 };
    world.playerResolves = false;
    arm();
    report("no_player", GameDraw({ 140, 5 }));

    // 9. The scan throws: it did not run.
    reset();
    world.maxed = { 140 };
    world.scanThrows = true;
    arm();
    report("scan_throws", GameDraw({ 140, 5 }));

    // 10. One scan per frame, however many draws the frame's rolls make.
    reset();
    world.maxed = { 140, 7 };
    arm();
    const int sameFrame = GameDraw({ 140, 7, 140, 9 });
    report("frame_cache_one_frame", sameFrame);
    ++g_RuntimeFrame;
    report("frame_cache_next_frame", GameDraw({ 140, 4 }));

    // 11. The skip log thins out: 20 lines one by one, then every 100th.
    reset();
    world.maxed = { 140 };
    arm();
    for (int roll = 0; roll < 100; ++roll) GameDraw({ 140, 5 });
    report("log_thinning", 5);

    // 12. Research: `relicfilter testmaxed` ids join the scan's set.
    reset();
    arm();
    rf.SetTestMaxed({ 7 });
    report("testmaxed_joins", GameDraw({ 7, 3 }));

    // 13. ...but only when the scan itself ran (no player: no filtering).
    reset();
    world.playerResolves = false;
    arm();
    rf.SetTestMaxed({ 7 });
    report("testmaxed_needs_a_scan", GameDraw({ 7, 3 }));

    // 14. Status: native hook, one skip.
    reset();
    world.maxed = { 140 };
    arm();
    GameDraw({ 140, 5 });
    world.log.clear();
    RelicFilterStatus();
    report("status_on", 5);

    // 15. Status: a table-only install is the failure it is.
    reset();
    arm();
    g_GetRelicQuestNative = false;
    RelicFilterStatus();
    report("status_table_only", -1);

    // 16. Status: off.
    reset();
    RelicFilterStatus();
    report("status_off", -1);

    // 17. Arming again starts a fresh count.
    reset();
    world.maxed = { 140 };
    arm();
    GameDraw({ 140, 5 });
    GameDraw({ 140, 6 });
    const long before = rf.Skips();
    arm();
    std::cout << "SCENARIO rearm_resets before=" << before << " after=" << rf.Skips() << "\n";

    // 18. DropRelic is `dropmult relic`'s hook only: one game call per call at
    //     x1, three at x3, and one inside AFK FARM's reward scope.
    reset();
    world.maxed = { 140 };
    arm();
    {
        RValue result;
        Hook_DropRelic(nullptr, nullptr, result, 0, nullptr);
    }
    report("drop_x1", -1);
    reset();
    g_mult_DropRelic = 3;
    {
        RValue result;
        Hook_DropRelic(nullptr, nullptr, result, 0, nullptr);
    }
    report("drop_x3", -1);
    reset();
    g_mult_DropRelic = 3;
    world.rewardScopeActive = true;
    {
        RValue result;
        Hook_DropRelic(nullptr, nullptr, result, 0, nullptr);
    }
    report("drop_reward_scope", -1);

    // ---- #93/#125: the lines the filter logs when it arms ------------------

    // 19. Arm-time line: the count and the ids from the scan's own set, then
    //     the equipped-slot and relic-tab reports.
    reset();
    world.maxed = { 42, 7 };
    world.equippedRelics = 2;
    world.tabRelics = 14;
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, false);
    world.log.clear();
    runArmReport("arm_scan_two");

    // 20. Ids are listed in numeric order, not text order (9 before 124).
    reset();
    world.maxed = { 135, 9, 124 };
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, false);
    world.log.clear();
    runArmReport("arm_scan_sorted");

    // 21. The scan ran and found nothing: an empty set is named as such.
    reset();
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, false);
    world.log.clear();
    runArmReport("arm_scan_empty");

    // 22. No player: the scan did not run, which must not read as "0 maxed".
    reset();
    world.maxed = { 42 };
    world.playerResolves = false;
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, false);
    world.log.clear();
    runArmReport("arm_scan_no_player");

    // 23. Armed and then switched off before the report: nothing is due.
    reset();
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, false);
    ForgePact::RelicFilterMod::Instance().SetEnabled(false, false);
    std::cout << "SCENARIO arm_then_off due="
              << (ForgePact::RelicFilterMod::Instance().IsArmScanDue() ? 1 : 0) << "\n";

    // 24. Re-armed while the GetRelicQuest hook is already in: the install is
    //     not pending, the report still is.
    reset();
    world.maxed = { 42 };
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, true);
    std::cout << "SCENARIO rearm_hooked pending="
              << (ForgePact::RelicFilterMod::Instance().IsPending() ? 1 : 0)
              << " due=" << (ForgePact::RelicFilterMod::Instance().IsArmScanDue() ? 1 : 0) << "\n";

    // 25. The equipped-slot read stopped at the owner lookup, and so did the
    //     relic tab's: each line names its stage beside the zero.
    reset();
    world.equippedStopped = "owner";
    world.tabStopped = "controller";
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, false);
    world.log.clear();
    runArmReport("arm_scan_stopped");

    // 26. The scan threw before any place was read: it did not run, and no
    //     report line claims a stage for a read that never happened.
    reset();
    world.maxed = { 42 };
    world.scanThrows = true;
    ForgePact::RelicFilterMod::Instance().SetEnabled(true, false);
    world.log.clear();
    runArmReport("arm_scan_throws");

    std::cout << "HARNESS DONE\n";
    return 0;
}
