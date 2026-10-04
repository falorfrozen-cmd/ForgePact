// Behavioral harness for Dungeon chest opens early's adapter in ModuleMain.cpp
// (issue #31): the kill consumer's enemy check and the planned total's census.
//
// tests/dungeon_chest_harness.cpp runs DungeonChestMod.hpp's decisions; this
// one runs the REAL adapter functions around them, which the Python runner
// (test_dungeon_chest_adapter.py) cuts out of ModuleMain.cpp and splices in
// below, together with the real header. Only the game is replaced: a fake
// object table answers asset_get_index, object_is_ancestor,
// variable_instance_get, instance_number, instance_find and is_array.
//
// Two binaries come from this file. The kill binary (ADAPTER_KILL) runs
// DungeonChestOnKill through the real enemy check (IsEnemyObject and
// CallerIsEnemyInstance) in both start-up states the plugin can be in: the
// research build's, where InstallCreateHooks resolved Enemy_Parent_obj into
// g_EnemyParentIdx at load, and the player build's, where nothing did unless
// another mod installed the create hooks (Live 2, 2026-10-04: kills=0 on every
// read while the owner killed). It must compile from the base tag's
// ModuleMain.cpp too, so the globals those functions use are defined here
// under their own names. The census binary (ADAPTER_CENSUS) runs
// DungeonChestEstimateTotal as the header's total source.
//
// The kill binary also runs the latch (D15): Live 2 latched at 333 against a
// threshold of 321, because a kill only counted and the decision waited for
// the next once-a-second poll. The draw binary (ADAPTER_DRAW) runs the real
// DungeonChestDraw and HhDrawOutlinedWorld (D14): Live 2 drew the head label
// at about 72 % of its size on single frames, because the label drew in
// whatever font the game had left current. Its fake runtime keeps a current
// font that a frame can start in (font A, or the smaller font B) and records
// the font each draw_text runs in. Both binaries must also compile from the
// Live 2 build's sources (tag forgepact-issue-31-dungeon-chest-c-live2), so
// they read what the fix adds only through the status line.
//
// No game process is touched.
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_ARRAY };
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    CInstance* instance = nullptr;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT), instance(p) {}
    // The runner refuses a number from anything that is not one.
    double ToDouble() const {
        if (m_Kind != VALUE_REAL && m_Kind != VALUE_INT32 && m_Kind != VALUE_INT64 && m_Kind != VALUE_REF)
            throw std::runtime_error("not a number");
        return number;
    }
    bool ToBoolean() const { return number != 0; }
};

// ---- the fake object table ----------------------------------------------------
// Player_obj 10; Enemy_Parent_obj 500 and its child Skeleton 510; the creator
// family's first two names at 600 and 601.
constexpr int kPlayerObj = 10;
constexpr int kEnemyParentObj = 500;
constexpr int kSkeletonObj = 510;
constexpr int kCreatorObj = 600;
constexpr int kCreatorAmbushObj = 601;
static const std::map<int, int> kParentOf = { { kSkeletonObj, kEnemyParentObj } };

struct CInstance {
    int id;
    int object;
    bool fired = false;        // a creator: its enemyArray is an array once it has spawned
    bool readable = true;      // a creator: whether instance_find hands back a live instance
    double x = 0, y = 0, top = 0;   // the player, for the head draw: origin and bounding box top
    RValue ToRValue() const { return RValue(const_cast<CInstance*>(this)); }
};

// ---- the draw state the head label reads and must put back -------------------
// Font A is what the game usually leaves current, font B the smaller one Live 2
// caught on single frames; __newfont6 and fntCustom resolve by name.
constexpr double kFontA = 1, kFontB = 2, kNewFont6 = 30, kFontCustom = 31;
static double LineHeight(double font) {
    return font == kFontA ? 18 : font == kFontB ? 14 : font == kNewFont6 ? 20 : font == kFontCustom ? 24 : 10;
}
static bool newFont6Resolves = true;
static double currentFont = kFontA, currentHalign = 0, currentValign = 0, currentColour = 0xFFFFFF, currentAlpha = 1;
// The kind draw_get_font answers in: a number, an asset reference (as the
// pack markers' draw found it can, tests/test_pack_markers_behavior.py), or
// unset (VALUE_UNDEFINED), which CallBuiltin returns both for a real "no font"
// state and for a missing builtin (the draw_get_font trap).
static int fontReadKind = VALUE_REAL;
static double guiWidth = 1920, guiHeight = 1080;
struct DrawCall { double x, y; std::string text; double font, colour; };
static std::vector<DrawCall> draws;

// Whether asset_get_index can resolve Enemy_Parent_obj yet (an ask made
// before the runtime answers it is the early ask the cache must not keep).
static bool parentResolvable = true;
static int parentAsks = 0;
// Whether the creator family's names resolve; which creator object's
// instance_number throws (-1: none).
static bool creatorFamilyResolves = true;
static int countThrowsFor = -1;
static std::vector<CInstance*> instances;

static bool Descends(int object, int parent) {
    for (auto it = kParentOf.find(object); it != kParentOf.end(); it = kParentOf.find(it->second))
        if (it->second == parent) return true;
    return false;
}

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        const std::string key(name);
        if (key == "asset_get_index") {
            const std::string& asset = args.at(0).text;
            if (asset == "Enemy_Parent_obj") { ++parentAsks; return RValue(parentResolvable ? (double)kEnemyParentObj : -1.0); }
            if (asset == "Player_obj") return RValue((double)kPlayerObj);
            if (asset == "Enemy_Creator_obj") return RValue(creatorFamilyResolves ? (double)kCreatorObj : -1.0);
            if (asset == "Enemy_Creator_Ambush_obj") return RValue(creatorFamilyResolves ? (double)kCreatorAmbushObj : -1.0);
            if (asset == "__newfont6") return RValue(newFont6Resolves ? kNewFont6 : -1.0);
            if (asset == "fntCustom") return RValue(kFontCustom);
            return RValue(-1.0);
        }
        // The head draw's builtins.
        if (key == "draw_get_font") {
            if (fontReadKind == VALUE_UNDEFINED) return RValue();
            RValue f(currentFont);
            f.m_Kind = fontReadKind;
            return f;
        }
        if (key == "draw_get_halign") return RValue(currentHalign);
        if (key == "draw_get_valign") return RValue(currentValign);
        if (key == "draw_get_colour") return RValue(currentColour);
        if (key == "draw_get_alpha") return RValue(currentAlpha);
        if (key == "draw_set_font") { currentFont = args.at(0).ToDouble(); return RValue(); }
        if (key == "draw_set_halign") { currentHalign = args.at(0).ToDouble(); return RValue(); }
        if (key == "draw_set_valign") { currentValign = args.at(0).ToDouble(); return RValue(); }
        if (key == "draw_set_colour") { currentColour = args.at(0).ToDouble(); return RValue(); }
        if (key == "draw_set_alpha") { currentAlpha = args.at(0).ToDouble(); return RValue(); }
        if (key == "string_height") return RValue(LineHeight(currentFont));
        if (key == "make_colour_rgb")
            return RValue(args.at(0).ToDouble() + args.at(1).ToDouble() * 256 + args.at(2).ToDouble() * 65536);
        if (key == "draw_text") {
            draws.push_back({ args.at(0).ToDouble(), args.at(1).ToDouble(), args.at(2).text, currentFont, currentColour });
            return RValue();
        }
        if (key == "view_get_camera") return RValue(0.0);
        if (key == "camera_get_view_x" || key == "camera_get_view_y") return RValue(0.0);
        if (key == "camera_get_view_width") return RValue(1920.0);
        if (key == "camera_get_view_height") return RValue(1080.0);
        if (key == "display_get_gui_width") return RValue(guiWidth);
        if (key == "display_get_gui_height") return RValue(guiHeight);
        if (key == "object_is_ancestor")
            return RValue(Descends((int)args.at(0).ToDouble(), (int)args.at(1).ToDouble()) ? 1.0 : 0.0);
        if (key == "variable_instance_get") {
            CInstance* inst = args.at(0).instance;
            if (!inst) throw std::runtime_error("variable_instance_get on no instance");
            const std::string& var = args.at(1).text;
            if (var == "object_index") return RValue((double)inst->object);
            if (var == "id") return RValue((double)inst->id);
            if (var == "enemyArray") { RValue v; if (inst->fired) v.m_Kind = VALUE_ARRAY; return v; }
            if (var == "x") return RValue(inst->x);
            if (var == "y") return RValue(inst->y);
            if (var == "bbox_top") return RValue(inst->top);
            return RValue();
        }
        if (key == "is_array") return RValue(args.at(0).m_Kind == VALUE_ARRAY ? 1.0 : 0.0);
        if (key == "instance_number") {
            const int obj = (int)args.at(0).ToDouble();
            if (obj == countThrowsFor) throw std::runtime_error("instance_number failed");
            double n = 0;
            for (auto* i : instances) if (i->object == obj || Descends(i->object, obj)) ++n;
            return RValue(n);
        }
        if (key == "instance_find") {
            const int obj = (int)args.at(0).ToDouble();
            int k = (int)args.at(1).ToDouble();
            for (auto* i : instances)
                if ((i->object == obj || Descends(i->object, obj)) && k-- == 0)
                    return i->readable ? i->ToRValue() : RValue(-4.0);
            return RValue(-4.0);
        }
        throw std::runtime_error("builtin not faked: " + key);
    }
};
static FakeRunner runner;
static FakeRunner* g_Yytk = &runner;

static std::vector<std::string> outLines;
static void Out(const std::string& line) { outLines.push_back(line); }

namespace HeroSiege::Objects {
inline bool IsDescendantOf(int32_t object, int32_t parent) { return Descends(object, parent); }
}

// ModuleMain's globals the extracted functions read, under their own names.
static int g_EnemyParentIdx = -1;
static std::unordered_map<int, bool> g_IsEnemyCache;
static long g_DcKillNotEnemySelf = 0;
static constexpr const char* kKnownDensityCreatorObjects[] = { "Enemy_Creator_obj", "Enemy_Creator_Ambush_obj" };
static std::vector<int> g_DcCreatorObjects;
// The census resolves a creator through the plugin's own lookup; here an
// instance is what instance_find handed back, or nothing.
static CInstance* HhResolveInstance(const RValue& value) { return value.m_Kind == VALUE_OBJECT ? value.instance : nullptr; }

#ifdef ADAPTER_DRAW
// The head draw's other inputs: the local player (standing still), the label
// lift and font override the Headhunter labels share, and the frame profiler's
// scope, which times nothing here.
static CInstance drawPlayer{ 1, kPlayerObj, false, true, 960, 540, 500 };
static bool HhResolveLocalPlayer(RValue& out, std::string* how = nullptr) {
    out = drawPlayer.ToRValue();
    if (how) *how = "stub";
    return true;
}
static std::string g_HhLabelFont;
static double g_HhLabelOffsetPx = 150.0;
enum class IncidentMod { dungeonchest, hudlabels };
struct IncidentScope { explicit IncidentScope(IncidentMod) {} };
#endif

// PRODUCTION_DUNGEONCHEST

// PRODUCTION_FUNCTIONS

namespace DC = ForgePact::DungeonChest;

static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}

static int chestDummy = 0;
static long FixedSource(long, DC::Census& census) { census = DC::Census{}; return 100; }
static std::string Status() { return DC::StatusLine(DC::state, "ok", "ok"); }
// One ` name=value` field of the status line, or "(absent)" on a tree whose
// status does not print it.
static std::string Field(const std::string& name) {
    const std::string line = Status();
    const size_t at = line.find(" " + name + "=");
    if (at == std::string::npos) return "(absent)";
    const size_t from = at + name.size() + 2;
    return line.substr(from, line.find(' ', from) - from);
}

#ifdef ADAPTER_KILL
// ---- the kill path ------------------------------------------------------------
static CInstance player{ 1, kPlayerObj };
static CInstance skeletonA{ 1001, kSkeletonObj };
static CInstance skeletonB{ 1002, kSkeletonObj };

// The research build: InstallCreateHooks resolved the parent at load. The name
// lookup is refused, so only the preset can make a kill count.
static void ResearchSetup() { g_EnemyParentIdx = kEnemyParentObj; parentResolvable = false; }
// The player build with no create-hook feature on (Live 2): nothing wrote the
// global, and the name lookup works.
static void ReleaseSetup() { g_EnemyParentIdx = -1; parentResolvable = true; }

// The mode on at 50 % and the room's chest seen: a tally is running.
static void Track() {
    DC::state.totalSource = &FixedSource;
    DC::SetMode(DC::state, 50);
    DC::Poll(DC::state, true, 1, 1, 10, &chestDummy);
}

// Kill-hook calls refused as not an enemy `self`: on the status line once the
// adapter reports them there, else the research probe's counter (the base tag).
static long NotEnemy() {
#ifdef HAS_STATUS_NOT_ENEMY
    const std::string line = Status();
    const size_t at = line.find(" notEnemy=");
    if (at == std::string::npos) return -1;
    return std::stol(line.substr(at + 10));
#else
    return g_DcKillNotEnemySelf;
#endif
}
static std::string Detail() {
    return "kills=" + std::to_string(DC::state.tally.kills) + " notEnemy=" + std::to_string(NotEnemy())
        + " g_EnemyParentIdx=" + std::to_string(g_EnemyParentIdx) + " parentAsks=" + std::to_string(parentAsks)
        + " | " + Status();
}

// The latch (D15). A planned total of 40 at 50 %: the first poll sets the
// threshold to 20. The unlock action answers that the chest can open, and the
// adapter's poll prints the latch line when Poll answers that it latched, as
// DungeonChestTick does.
static int unlockCalls = 0;
static bool UnlockOk() { ++unlockCalls; return true; }
static long Source40(long, DC::Census& census) { census = DC::Census{}; return 40; }
static void TrackForLatch() {
    DC::state.totalSource = &Source40;
    DC::state.unlock = &UnlockOk;
    DC::SetMode(DC::state, 50);
    DC::Poll(DC::state, true, 1, 1, 10, &chestDummy);
}
static void Tick() {
    if (DC::Poll(DC::state, true, 1, 1, 10, &chestDummy)) Out(DC::UnlockedLine(DC::state));
}
// A pack of distinct enemies, each counted once.
static std::vector<CInstance>& Pack() {
    static std::vector<CInstance> pack;
    if (pack.empty()) for (int i = 0; i < 30; ++i) pack.push_back({ 3000 + i, kSkeletonObj });
    return pack;
}
static long UnlockedLines() {
    long n = 0;
    for (const auto& l : outLines) if (l.rfind("dungeonchest: unlocked early at 20/40 alive=", 0) == 0) ++n;
    return n;
}
static std::string LatchDetail(long latchedAfter) {
    std::string lines;
    for (const auto& l : outLines) lines += " [" + l + "]";
    return "latchedAfter=" + std::to_string(latchedAfter) + " unlockCalls=" + std::to_string(unlockCalls)
        + " | " + Status() + " | out:" + lines;
}

static int Run(const std::string& scenario) {
    if (scenario == "kill-research-setup") {
        ResearchSetup(); Track();
        DungeonChestOnKill(&skeletonA);
        check(scenario, DC::state.tally.kills == 1 && NotEnemy() == 0, Detail());
    } else if (scenario == "kill-player-self") {
        // The player-`self` call of a kill counts nothing, also once the
        // parent resolves: the enemy check did not become "anything".
        ReleaseSetup(); Track();
        DungeonChestOnKill(&player);
        check(scenario, DC::state.tally.kills == 0 && NotEnemy() == 1, Detail());
    } else if (scenario == "kill-not-tracking") {
        // The mode on with no chest in the room, then the mode off: nothing.
        ResearchSetup();
        DC::state.totalSource = &FixedSource;
        DC::SetMode(DC::state, 50);
        DC::Poll(DC::state, true, 1, 0, 10, nullptr);
        DungeonChestOnKill(&skeletonA);
        const long noChest = DC::state.tally.kills;
        DC::SetMode(DC::state, 0);
        DungeonChestOnKill(&skeletonB);
        check(scenario, noChest == 0 && DC::state.tally.kills == 0 && NotEnemy() == 0 && DC::state.counters.kills == 0, Detail());
    } else if (scenario == "kill-same-id-twice") {
        ResearchSetup(); Track();
        DungeonChestOnKill(&skeletonA);
        DungeonChestOnKill(&skeletonA);
        check(scenario, DC::state.tally.kills == 1 && DC::state.counters.duplicates == 1, Detail());
    } else if (scenario == "kill-release-setup") {
        // Live 2's start-up state: an enemy-`self` call counts, by name, and
        // the global the create hooks own is left as it was.
        ReleaseSetup(); Track();
        DungeonChestOnKill(&skeletonA);
        check(scenario, DC::state.tally.kills == 1 && NotEnemy() == 0 && g_EnemyParentIdx == -1, Detail());
    } else if (scenario == "enemy-check-not-poisoned") {
        // An ask made while the parent cannot be resolved yet is refused, and
        // that refusal is not kept: the same object counts once it resolves.
        g_EnemyParentIdx = -1; parentResolvable = false; Track();
        DungeonChestOnKill(&skeletonA);
        const long early = DC::state.tally.kills;
        const long earlyRefused = NotEnemy();
        parentResolvable = true;
        DungeonChestOnKill(&skeletonB);
        check(scenario, early == 0 && earlyRefused == 1 && DC::state.tally.kills == 1 && NotEnemy() == 1
                && IsEnemyObject(kSkeletonObj) && !IsEnemyObject(kPlayerObj) && g_EnemyParentIdx == -1,
            Detail());
    } else if (scenario == "latch-at-poll") {
        // Kept: one kill a poll, the way the parent's harness played it,
        // latches at the 20th kill of a planned 40 at 50 %, with one line.
        ResearchSetup(); TrackForLatch();
        long latchedAfter = 0;
        for (int i = 0; i < 25; ++i) {
            DungeonChestOnKill(&Pack()[i]);
            Tick();
            if (DC::state.tally.latched && latchedAfter == 0) latchedAfter = DC::state.tally.kills;
        }
        check(scenario, latchedAfter == 20 && UnlockedLines() == 1 && unlockCalls == 1, LatchDetail(latchedAfter));
    } else if (scenario == "latch-at-the-kill") {
        // D15: the threshold is 20 from a poll, then 25 kills reach the real
        // consumer with no poll between them, as a second of AoE play does.
        // The latch comes at the 20th kill, which prints the one latch line;
        // the next poll prints no second one.
        ResearchSetup(); TrackForLatch();
        long latchedAfter = 0;
        for (int i = 0; i < 25; ++i) {
            DungeonChestOnKill(&Pack()[i]);
            if (DC::state.tally.latched && latchedAfter == 0) latchedAfter = DC::state.tally.kills;
        }
        const long linesAtKills = UnlockedLines();
        const std::string latchedAt = Field("latchedAt");
        Tick();
        check(scenario, latchedAfter == 20 && linesAtKills == 1 && UnlockedLines() == 1 && latchedAt == "20"
                && unlockCalls == 1 && DC::state.tally.kills == 25,
            LatchDetail(latchedAfter) + " latchedAt(after kills)=" + latchedAt);
    } else {
        std::cout << "unknown scenario " << scenario << std::endl;
        return 2;
    }
    return failures ? 1 : 0;
}
#endif

#ifdef ADAPTER_CENSUS
// ---- the census ---------------------------------------------------------------
// Three creators of Enemy_Creator_obj (one has spawned) and one ambush creator
// still to spawn: 3 pending, so with 5 alive the estimate is 5 + ceil(3 x 614 /
// 117) = 21.
static CInstance c1{ 2001, kCreatorObj, false }, c2{ 2002, kCreatorObj, false }, c3{ 2003, kCreatorObj, true };
static CInstance c4{ 2004, kCreatorAmbushObj, false };
static void Room() { instances = { &c1, &c2, &c3, &c4 }; }

static void Track(int64_t room) {
    DC::state.totalSource = &DungeonChestEstimateTotal;
    if (DC::Pct(DC::state) == 0) DC::SetMode(DC::state, 50);
    DC::Poll(DC::state, true, room, 1, 5, &chestDummy);
}
static long RefusalLines() {
    long n = 0;
    for (const auto& l : outLines) if (l.rfind("dungeonchest: no planned total in this room (", 0) == 0) ++n;
    return n;
}
static std::string Detail() {
    std::string lines;
    for (const auto& l : outLines) lines += " [" + l + "]";
    return Status() + " | out:" + lines;
}
static bool Shows(const std::string& word) { return Status().find(" total=unavailable(" + word + ") ") != std::string::npos; }

static int Run(const std::string& scenario) {
    Room();
    if (scenario == "census-estimates") {
        // The control: a readable census estimates and logs nothing.
        Track(1);
        check(scenario, DC::state.tally.total == 21 && Status().find(" total=21 creators=4 pending=3 unreadable=0 ") != std::string::npos
                && RefusalLines() == 0, Detail());
    } else if (scenario == "census-count-failed") {
        // instance_number fails for the ambush creators: the census refuses
        // rather than estimate from the three it could count.
        countThrowsFor = kCreatorAmbushObj;
        Track(1);
        check(scenario, DC::state.tally.total == 0 && Shows("count-failed") && !DC::state.tally.latched, Detail());
    } else if (scenario == "census-family-unresolved") {
        creatorFamilyResolves = false;
        Track(1);
        check(scenario, DC::state.tally.total == 0 && Shows("family-unresolved"), Detail());
    } else if (scenario == "census-no-creators") {
        instances.clear();
        Track(1);
        check(scenario, DC::state.tally.total == 0 && Shows("no-creators"), Detail());
    } else if (scenario == "census-refusal-logged-once") {
        // Five polls of a refusing room print one line, naming the cause and
        // that the game's own rule stays; the next room prints its own.
        countThrowsFor = kCreatorAmbushObj;
        for (int i = 0; i < 5; ++i) Track(1);
        const long first = RefusalLines();
        const long asks = DC::state.counters.totalAsks;
        const std::string line = outLines.empty() ? std::string() : outLines.front();
        for (int i = 0; i < 3; ++i) Track(2);
        check(scenario, first == 1 && asks == 5 && RefusalLines() == 2 && line.find("count-failed") != std::string::npos
                && line.find("game's own rule") != std::string::npos, Detail());
    } else {
        std::cout << "unknown scenario " << scenario << std::endl;
        return 2;
    }
    return failures ? 1 : 0;
}
#endif

#ifdef ADAPTER_DRAW
// ---- the head label's draw (D14) ----------------------------------------------
// The mode on at 50 % of a planned 100: 50 kills remain, so the label shows
// `Chest: 50 kills to go`. The player stands still at (960, 540) with its box
// top at 500, in a 1920x1080 view drawn to a GUI of the same size, so the
// label's spot is (960, 350) and its text sits one line height under it.
static void ShowLabel() {
    DC::state.totalSource = &FixedSource;
    DC::SetMode(DC::state, 50);
    DC::Poll(DC::state, true, 1, 1, 10, &chestDummy);
}
// One frame of Draw GUI: the game leaves `inherited` as the current font,
// then the head draw runs. Answers that frame's draw_text calls.
static std::vector<DrawCall> Frame(double inherited) {
    currentFont = inherited;
    draws.clear();
    DungeonChestDraw();
    return draws;
}
static std::string Calls(const std::vector<DrawCall>& calls) {
    std::string out;
    for (const auto& c : calls)
        out += " [" + c.text + " @" + std::to_string((int)c.x) + "," + std::to_string((int)c.y)
            + " font=" + std::to_string((int)c.font) + "]";
    return out;
}
// Every call of a frame ran in `font`, and the fill sits one line of that
// font under the spot.
static bool DrawnIn(const std::vector<DrawCall>& calls, double font) {
    if (calls.size() != 5) return false;
    for (const auto& c : calls) if (c.font != font) return false;
    return calls.back().x == 960 && calls.back().y == 350 + LineHeight(font);
}

static int Run(const std::string& scenario) {
    ShowLabel();
    if (scenario == "label-text-steady") {
        // Kept: frames with no kill in the font the game usually leaves draw
        // one label each, the same text on the same whole pixel, as four
        // outline passes and then the fill.
        bool ok = true;
        std::string detail;
        std::vector<DrawCall> first;
        for (int frame = 0; frame < 6; ++frame) {
            const std::vector<DrawCall> calls = Frame(kFontA);
            detail += " frame" + std::to_string(frame) + ":" + Calls(calls);
            if (frame == 0) first = calls;
            ok = ok && calls.size() == 5 && !first.empty() && calls.back().x == first.back().x
                && calls.back().y == first.back().y && calls.back().x == std::floor(calls.back().x)
                && calls.back().y == std::floor(calls.back().y);
            for (size_t i = 0; ok && i < calls.size(); ++i)
                ok = calls[i].text == "Chest: 50 kills to go" && (i < 4 ? calls[i].colour == 0 : calls[i].colour != 0);
        }
        check(scenario, ok, detail);
    } else if (scenario == "label-state-restored") {
        // Kept: what the game had set reads back unchanged after the draw.
        currentHalign = 0; currentValign = 0; currentColour = 123; currentAlpha = 0.5;
        Frame(kFontA);
        check(scenario, currentFont == kFontA && currentHalign == 0 && currentValign == 0 && currentColour == 123
                && currentAlpha == 0.5,
            "font=" + std::to_string(currentFont) + " halign=" + std::to_string(currentHalign) + " valign="
                + std::to_string(currentValign) + " colour=" + std::to_string(currentColour) + " alpha="
                + std::to_string(currentAlpha));
    } else if (scenario == "label-font-pinned") {
        // D14: the game leaves the smaller font B current on one frame. Every
        // label draw still runs in __newfont6, measured in __newfont6, so the
        // label is the same size every frame; the inherited font is put back.
        bool ok = true;
        std::string detail;
        const double frames[] = { kFontA, kFontA, kFontB, kFontA, kFontA };
        for (double inherited : frames) {
            const std::vector<DrawCall> calls = Frame(inherited);
            detail += " inherited=" + std::to_string((int)inherited) + ":" + Calls(calls);
            ok = ok && DrawnIn(calls, kNewFont6) && currentFont == inherited;
        }
        check(scenario, ok && Field("labelFont") == "__newfont6", "labelFont=" + Field("labelFont") + detail);
    } else if (scenario == "label-font-override") {
        // `hhlabelfont fntCustom` names the font the label draws in.
        g_HhLabelFont = "fntCustom";
        bool ok = true;
        std::string detail;
        for (double inherited : { kFontA, kFontB, kFontA }) {
            const std::vector<DrawCall> calls = Frame(inherited);
            detail += Calls(calls);
            ok = ok && DrawnIn(calls, kFontCustom) && currentFont == inherited;
        }
        check(scenario, ok && Field("labelFont") == "fntCustom", "labelFont=" + Field("labelFont") + detail);
    } else if (scenario == "label-font-unresolved") {
        // __newfont6 does not resolve: the label still draws, in the inherited
        // font, and the status says so.
        newFont6Resolves = false;
        const std::vector<DrawCall> calls = Frame(kFontA);
        check(scenario, DrawnIn(calls, kFontA) && Field("labelFont") == "inherited",
            "labelFont=" + Field("labelFont") + Calls(calls));
    } else if (scenario == "label-diagnostics") {
        // The two counters Live procedure 3 reads as deltas: a label draw whose
        // inherited font differs from the previous label draw's (A A B A: two),
        // and one whose GUI size differs from the previous one's (two).
        for (double inherited : { kFontA, kFontA, kFontB, kFontA }) Frame(inherited);
        const std::string fontSwitches = Field("fontSwitches");
        guiWidth = 1280; guiHeight = 720;
        Frame(kFontA);
        guiWidth = 1920; guiHeight = 1080;
        Frame(kFontA);
        check(scenario, fontSwitches == "2" && Field("fontSwitches") == "2" && Field("guiResizes") == "2"
                && Field("inheritedFont") == "1" && Field("inheritedUnread") == "0",
            "fontSwitches(before resizes)=" + fontSwitches + " | " + Status());
    } else if (scenario == "label-font-unread") {
        // The instrument's own read (instrument-blindness review, round 2):
        // draw_get_font answers unset, so the inherited font is unknown. The
        // label still draws in __newfont6, the unread draws count in
        // `inheritedUnread`, and `inheritedFont=unread` says `fontSwitches=0`
        // measured nothing, however the font underneath changed (A, B, A).
        fontReadKind = VALUE_UNDEFINED;
        bool ok = true;
        std::string detail;
        for (double inherited : { kFontA, kFontB, kFontA }) {
            const std::vector<DrawCall> calls = Frame(inherited);
            detail += Calls(calls);
            ok = ok && DrawnIn(calls, kNewFont6);
        }
        const std::string blind = Status();
        ok = ok && Field("inheritedFont") == "unread" && Field("inheritedUnread") == "3" && Field("fontSwitches") == "0";
        // Control: an asset reference is a read, and a switch read through it
        // counts (A then B: one switch); the unread draws stay counted.
        fontReadKind = VALUE_REF;
        for (double inherited : { kFontA, kFontB }) {
            const std::vector<DrawCall> calls = Frame(inherited);
            detail += Calls(calls);
            ok = ok && DrawnIn(calls, kNewFont6) && currentFont == inherited;
        }
        ok = ok && Field("inheritedFont") == "2" && Field("inheritedUnread") == "3" && Field("fontSwitches") == "1";
        check(scenario, ok, "blind: " + blind + " | read: " + Status() + detail);
    } else {
        std::cout << "unknown scenario " << scenario << std::endl;
        return 2;
    }
    return failures ? 1 : 0;
}
#endif

int main(int argc, char** argv) {
    if (argc < 2) { std::cout << "usage: <scenario>" << std::endl; return 2; }
    return Run(argv[1]);
}
