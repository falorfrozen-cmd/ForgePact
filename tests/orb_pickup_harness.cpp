// Behavioral harness for the orb (globe) pickup radius mod.
//
// The Python runner injects the REAL PullOneGlobe, the REAL OrbScan and the
// REAL OrbPickupTick below, plus the production constants, so no number here
// restates one from the plugin. Only the game API is replaced.
//
// Two things are being held apart, and the harness exists to prove they stay
// apart. The PULL genuinely needs per-frame work: globes glide toward the
// player at a constant kGlobePullSpeed px per frame, and that constant speed
// was a deliberate fix to a user report (the old proportional step "read as an
// unnatural teleport right before pickup"). The ENUMERATION does not - it
// walked every globe instance of every globe type, every frame, at up to ~196
// CallBuiltins per frame. Throttling the scan must leave the glide
// byte-for-byte identical, which is what the baseline scenarios pin.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// ---- minimal game-API stand-ins ------------------------------------------
enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING,
       VALUE_UNDEFINED, VALUE_BOOL, VALUE_ARRAY };
struct CInstance;
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    explicit RValue(CInstance* p) : m_Kind(VALUE_OBJECT) { (void)p; }
    double ToDouble() const { return number; }
    int64_t ToInt64() const { return (int64_t)number; }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};
#ifndef NULL_INDEX
#define NULL_INDEX INT_MIN
#endif
struct CInstance { int id = 0; };
using AurieStatus = int;
static bool AurieSuccess(int s) { return s == 0; }
// The plugin's counters are Interlocked because several of them are read by
// the IPC command path; the harness only needs the arithmetic.
static long InterlockedIncrement(volatile long* target) { return ++(*target); }

// ---- the controlled world -------------------------------------------------
struct Globe { int id; int objIdx; double x; double y; bool alive = true; };
struct World {
    std::vector<Globe> globes;
    int64_t room = 100;
    bool roomReadable = true;
    // The live runner returns a REF for `room`; true switches the fake to a
    // plain number, so both kinds are exercised against one key derivation.
    bool roomIsReal = false;
    // What kind instance_find hands back. VALUE_REF is what this runner really
    // returns; the other two exist so the scenarios can ask what happens when
    // a runner returns something else, which is the whole point of validating
    // a handle that is about to be held across frames.
    int handleKind = VALUE_REF;
    long instanceNumber = 0, instanceFind = 0, instanceExists = 0;
    long varGet = 0, varSet = 0, calls = 0;
    void resetCounts() {
        instanceNumber = instanceFind = instanceExists = varGet = varSet = calls = 0;
    }
    Globe* find(int id) {
        for (auto& g : globes) if (g.id == id) return &g;
        return nullptr;
    }
};
static World world;
static CInstance g_GlobalInstance;
static RValue g_RoomMember;

// An instance handle the way this runner really hands one out: VALUE_REF, not
// VALUE_OBJECT (docs/RUNTIME_DATA_MODELS.md; Known Limitation 7 is what
// happens when a kind check decides whether the work happens at all).
static RValue handleFor(int id) { RValue r; r.m_Kind = world.handleKind; r.number = (double)id; return r; }

struct FakeRunner {
    RValue CallBuiltin(const char* name, std::vector<RValue> args) {
        ++world.calls;
        const std::string key(name);
        if (key == "instance_number") {
            ++world.instanceNumber;
            const int obj = (int)args[0].ToDouble();
            int n = 0;
            for (auto& g : world.globes) if (g.alive && g.objIdx == obj) ++n;
            return RValue((double)n);
        }
        if (key == "instance_find") {
            ++world.instanceFind;
            const int obj = (int)args[0].ToDouble();
            int want = (int)args[1].ToDouble(), seen = 0;
            for (auto& g : world.globes) {
                if (!g.alive || g.objIdx != obj) continue;
                if (seen++ == want) return handleFor(g.id);
            }
            return RValue();                          // VALUE_UNDEFINED
        }
        if (key == "instance_exists") {
            ++world.instanceExists;
            Globe* g = world.find((int)args[0].ToDouble());
            return RValue(g && g->alive ? 1.0 : 0.0);
        }
        if (key == "variable_instance_get") {
            ++world.varGet;
            Globe* g = world.find((int)args[0].ToDouble());
            if (!g) return RValue();
            const std::string v = args[1].ToString();
            if (v == "x") return RValue(g->x);
            if (v == "y") return RValue(g->y);
            return RValue();
        }
        if (key == "variable_instance_set") {
            ++world.varSet;
            Globe* g = world.find((int)args[0].ToDouble());
            if (!g) return RValue();
            const std::string v = args[1].ToString();
            if (v == "x") g->x = args[2].ToDouble();
            if (v == "y") g->y = args[2].ToDouble();
            return RValue();
        }
        return RValue();
    }
    AurieStatus GetGlobalInstance(CInstance** out) { *out = &g_GlobalInstance; return 0; }
    // `room` is a BUILT-IN, so GetInstanceMember never answers for it on the
    // real runner - kept here, always failing, as the negative control that
    // pins why CurrentRoomKey stopped using it.
    AurieStatus GetInstanceMember(RValue, const char*, RValue*& out) {
        out = nullptr;
        return 1;
    }
    // Measured live 2026-09-15 (`roomprobe`): the runner answers `room` as a
    // VALUE_REF stringifying to "ref room Act_06_01" - NOT a real. A stub that
    // handed back a convenient number could not represent the input that broke
    // this, so it hands back a ref by default.
    AurieStatus GetBuiltin(const char* name, CInstance*, int, RValue& out) {
        if (std::string(name) != "room" || !world.roomReadable) return 1;
        out = RValue();
        if (world.roomIsReal) { out.m_Kind = VALUE_REAL; out.number = (double)world.room; }
        else { out.m_Kind = VALUE_REF; out.text = "ref room Act_" + std::to_string(world.room); }
        return 0;
    }
};
static FakeRunner runnerStorage;
static FakeRunner* g_Yytk = &runnerStorage;

// ---- what the injected production code leans on ---------------------------
// PRODUCTION_CONSTANTS

static volatile long g_OrbPickupHits = 0;
static volatile long g_OrbNoPlayer = 0;
static volatile long g_OrbOutOfReach = 0;
static volatile long g_OrbNearestPx = -1;
static volatile long g_OrbGlobesSeen = 0;
static volatile long g_OrbUncacheableKind = 0;
static bool g_PlayerPosValidFlag = false;
static double g_PlayerX = 0.0, g_PlayerY = 0.0;
// std::atomic<bool> in production; the harness runs single-threaded and only
// needs .load().
struct FakeAtomicBool {
    bool value = false;
    bool load() const { return value; }
    void store(bool v) { value = v; }
};
static FakeAtomicBool g_PlayerPosValid;

static std::vector<int> g_GlobeObjIdx;
static bool g_OrbAssetsResolved = false;
// Stubbed: resolving real asset indices would pull hs-game-sdk headers into
// the harness for no benefit. g_GlobeObjIdx is pre-populated instead.
static void ResolveOrbAssets() { g_OrbAssetsResolved = true; }

// PRODUCTION_ORB

// ---- the pre-change shape, kept runnable ----------------------------------
// So the before/after call counts are measured against each other in the same
// run rather than quoted from a comment.
static void PreChangeOrbTick(uint32_t)
{
    ResolveOrbAssets();
    if (g_GlobeObjIdx.empty()) return;
    int budget = 64;
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

// ---- scenarios ------------------------------------------------------------
static int failures = 0;
static void check(const std::string& label, double got, double want) {
    const bool ok = std::fabs(got - want) < 1e-9;
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}
static void checkInt(const std::string& label, long long got, long long want) {
    const bool ok = (got == want);
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " want=" << want << "\n";
}
static void checkAtMost(const std::string& label, long long got, long long limit) {
    const bool ok = (got <= limit);
    if (!ok) ++failures;
    std::cout << (ok ? "PASS " : "FAIL ") << label << " got=" << got << " limit=" << limit << "\n";
}

static const int kExpGlobe = 300;
static const int kMfGlobe = 301;
static const double kReach = kGlobeBaseRadius * kOrbPickupFactor;

static void resetAll() {
    world = World{};
    world.resetCounts();
    g_GlobeObjIdx = { kExpGlobe, kMfGlobe };
    g_OrbAssetsResolved = true;
    g_OrbPickupHits = g_OrbNoPlayer = g_OrbOutOfReach = g_OrbGlobesSeen = 0;
    g_OrbUncacheableKind = 0;
    g_OrbNearestPx = -1;
    g_PlayerPosValid.store(true);
    g_PlayerX = 0.0; g_PlayerY = 0.0;
    OrbCacheReset();
}

int main() {
    // --- 1. baseline: the constant-speed glide is unchanged -----------------
    resetAll();
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 } };
    OrbPickupTick(1);
    check("pull/one_frame_step", world.find(1)->x, 100.0 - kGlobePullSpeed);
    OrbPickupTick(2);
    check("pull/two_frame_step", world.find(1)->x, 100.0 - 2 * kGlobePullSpeed);

    // --- 2. baseline: the last step lands exactly, never past ---------------
    resetAll();
    world.globes = { { 1, kExpGlobe, kGlobePullSpeed / 2.0, 0.0 } };
    OrbPickupTick(1);
    check("pull/no_overshoot_x", world.find(1)->x, 0.0);
    check("pull/no_overshoot_y", world.find(1)->y, 0.0);

    // --- 3. baseline: a globe outside reach is never moved ------------------
    resetAll();
    world.globes = { { 1, kExpGlobe, kReach * 4.0, 0.0 } };
    for (uint32_t f = 1; f <= 40; ++f) OrbPickupTick(f);
    check("out_of_reach/not_moved", world.find(1)->x, kReach * 4.0);
    checkInt("out_of_reach/not_pulled", g_OrbPickupHits, 0);

    // --- 4. baseline: no player position moves nothing, and says so ---------
    // Known Limitation 7: this counter is what turned seen=176993
    // noplayer=176993 into a diagnosis. It has to keep incrementing.
    resetAll();
    g_PlayerPosValid.store(false);
    world.globes = { { 1, kExpGlobe, 10.0, 0.0 } };
    for (uint32_t f = 1; f <= 3; ++f) OrbPickupTick(f);
    check("no_player/not_moved", world.find(1)->x, 10.0);
    checkInt("no_player/globes_seen", g_OrbGlobesSeen > 0 ? 1 : 0, 1);
    checkInt("no_player/noplayer_counted", g_OrbNoPlayer > 0 ? 1 : 0, 1);

    // --- 5. target: the enumeration is throttled, the pull is not -----------
    resetAll();
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 } };
    world.resetCounts();
    for (uint32_t f = 1; f <= (uint32_t)kOrbScanFrames; ++f) PreChangeOrbTick(f);
    const long numberBefore = world.instanceNumber, findBefore = world.instanceFind;
    resetAll();
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 } };
    world.resetCounts();
    const double startX = world.find(1)->x;
    for (uint32_t f = 1; f <= (uint32_t)kOrbScanFrames; ++f) OrbPickupTick(f);
    checkInt("scan15/instance_number_before", numberBefore, 2 * kOrbScanFrames);
    checkInt("scan15/instance_find_before", findBefore, kOrbScanFrames);
    checkInt("scan15/instance_number_after", world.instanceNumber, 2);
    checkInt("scan15/instance_find_after", world.instanceFind, 1);
    // ...and the globe still moved on every one of those frames.
    check("scan15/still_pulled_every_frame",
          startX - world.find(1)->x, kOrbScanFrames * kGlobePullSpeed);

    // --- 6. target: a globe that comes into reach between scans -------------
    // It is never missed, and the lateness is bounded by the scan period.
    //
    // ASSERT THE PULL HAPPENED BEFORE BOUNDING WHEN IT HAPPENED. pulledAt
    // stays -1 when the globe is never touched at all, and -1 <= 15 prints
    // PASS - so a broken band, a broken cache and a blind room key would all
    // satisfy the bound. Zero and success indistinguishable is the exact shape
    // of the "0 calls" false negative AGENTS.md is built around; the sibling
    // est_force_harness.cpp guards the same -1 idiom the same way.
    //
    // It is a GUARD, not a witness, and the difference was got wrong once
    // already: band/was_pulled does NOT fail against the unthrottled code -
    // there the teleport below is picked up by the next scan and the whole
    // scenario passes end to end. Its failing direction is reachable (got=0
    // want=1 with the cache disabled), which is what makes it worth keeping,
    // but nothing here is evidence that the throttle works.
    resetAll();
    const double far = kReach + (double)kOrbScanFrames * kOrbApproachMargin + 500.0;
    world.globes = { { 1, kExpGlobe, far, 0.0 } };
    OrbPickupTick(1);                                   // scan: too far to cache
    check("band/not_cached_when_far", world.find(1)->x, far);
    g_PlayerX = far - kReach / 2.0;                     // the player walks over to it
    long long pulledAt = -1;
    for (uint32_t f = 2; f <= 2 + (uint32_t)kOrbScanFrames; ++f) {
        const double before = world.find(1)->x;
        OrbPickupTick(f);
        if (pulledAt < 0 && world.find(1)->x != before) pulledAt = f - 1;
    }
    checkInt("band/was_pulled", pulledAt >= 0 ? 1 : 0, 1);
    checkAtMost("band/pulled_within_scan_period", pulledAt, kOrbScanFrames);

    // --- 6b. the negative control for 6 -------------------------------------
    // Same globe, same frames, player never approaches: nothing may move. A
    // tick that pulled everything it could see would satisfy `was_pulled`
    // above on its own, so the positive needs a negative beside it.
    resetAll();
    world.globes = { { 1, kExpGlobe, far, 0.0 } };
    for (uint32_t f = 1; f <= 2 + (uint32_t)kOrbScanFrames; ++f) OrbPickupTick(f);
    check("band/no_approach_not_moved", world.find(1)->x, far);
    checkInt("band/no_approach_not_pulled", g_OrbPickupHits, 0);

    // --- 6c. target: the band bound holds at ANY movement speed -------------
    // reach + kOrbScanFrames * kOrbApproachMargin only bounds what a globe can
    // close between two scans if the PLAYER really closes less than
    // kOrbApproachMargin px per frame. Nothing enforces that: the panel's own
    // Movement Speed control multiplies total movement speed by up to 10x.
    // Above that the player crosses the band inside one scan period, the globe
    // is never cached and never pulled - and seen>0 / pulled>0 keep `orbpickup
    // stat` looking healthy, so the widened radius simply stops applying,
    // invisibly and intermittently. The scan is therefore forced by distance
    // travelled as well as by the frame counter, which makes the bound hold by
    // construction instead of by assumption.
    resetAll();
    const double justOutsideBand = kReach + 500.0;      // 140 px beyond the band
    world.globes = { { 1, kExpGlobe, justOutsideBand, 0.0 } };
    OrbPickupTick(1);                                   // scan: not cached
    check("fastplayer/not_cached_when_far", world.find(1)->x, justOutsideBand);
    long long fastPulledAt = -1;
    world.resetCounts();
    for (uint32_t f = 2; f <= 1 + (uint32_t)kOrbScanFrames; ++f) {
        g_PlayerX += 140.0;                             // ~10x an ordinary base step
        const double before = world.find(1)->x;
        OrbPickupTick(f);
        if (fastPulledAt < 0 && world.find(1)->x != before) fastPulledAt = f - 1;
    }
    checkInt("fastplayer/was_pulled", fastPulledAt >= 0 ? 1 : 0, 1);
    // The cost of holding the bound at speed: more scans, never more than the
    // per-frame enumeration this change replaced.
    checkAtMost("fastplayer/scan_cost_bounded", world.instanceFind, kOrbScanFrames);

    // --- 6d. ...and an ordinary walk still pays for the throttle ------------
    // The extra scans above must be bought by speed, not by moving at all.
    resetAll();
    world.globes = { { 1, kExpGlobe, kReach * 4.0, 0.0 } };
    world.resetCounts();
    for (uint32_t f = 1; f <= (uint32_t)kOrbScanFrames; ++f) {
        g_PlayerX += 10.0;                              // an unhurried walk
        OrbPickupTick(f);
    }
    checkInt("walk/one_scan_only", world.instanceFind, 1);

    // --- 7. target: a globe collected between scans is not written to -------
    resetAll();
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 }, { 2, kMfGlobe, 120.0, 0.0 } };
    OrbPickupTick(1);                                   // both cached
    world.find(1)->alive = false;                       // collected by the game
    const double deadX = world.find(1)->x;
    const double liveX = world.find(2)->x;
    OrbPickupTick(2);
    check("destroyed/not_written", world.find(1)->x, deadX);
    // ...and one dead handle does not abort the rest of the tick.
    check("destroyed/others_still_pulled", liveX - world.find(2)->x, kGlobePullSpeed);

    // --- 8. target: a room change drops the cache -------------------------
    resetAll();
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 } };
    OrbPickupTick(1);
    world.globes = { { 9, kExpGlobe, 60.0, 0.0 } };     // new zone, new globes
    world.room = 200;
    world.resetCounts();
    OrbPickupTick(2);                                   // not a scan frame
    checkInt("room_change/rescanned", world.instanceFind, 1);
    check("room_change/new_globe_pulled", world.find(9)->x, 60.0 - kGlobePullSpeed);

    // --- 9. the three stat counters stay on ONE clock -----------------------
    // Known Limitation 7 was diagnosed from a RATIO - `seen=176993
    // noplayer=176993` - so counters under one label have to be counted the
    // same way. `seen` moved into the scan; if `outofreach` had been left in
    // the per-frame pull, one globe sitting inside the band but outside reach
    // would contribute ~15 outofreach per 1 seen, and the next person to read
    // a pasted `orbpickup stat` would mis-read it.
    resetAll();
    world.globes = { { 1, kExpGlobe, kReach + 100.0, 0.0 } };   // in band, out of reach
    for (uint32_t f = 1; f <= (uint32_t)kOrbScanFrames; ++f) OrbPickupTick(f);
    checkInt("counters/seen_once_per_scan", g_OrbGlobesSeen, 1);
    checkInt("counters/outofreach_once_per_scan", g_OrbOutOfReach, 1);
    check("counters/outofreach_globe_not_moved", world.find(1)->x, kReach + 100.0);

    // ...and the same for noplayer, which is the counter the decision tree
    // reads against `seen`.
    resetAll();
    g_PlayerPosValid.store(false);
    world.globes = { { 1, kExpGlobe, 10.0, 0.0 } };
    for (uint32_t f = 1; f <= (uint32_t)kOrbScanFrames; ++f) OrbPickupTick(f);
    checkInt("counters/seen_with_no_player", g_OrbGlobesSeen, 1);
    checkInt("counters/noplayer_once_per_scan", g_OrbNoPlayer, 1);

    // --- 10. target: the player resolving again forces a rescan -------------
    // A scan that ran while the player was unresolved cached nothing at all -
    // it has no position to measure reach against - and the transition back to
    // resolved is not something the frame counter can see. Without forcing a
    // scan on it, the cache stays empty until the counter next comes round, so
    // every dropout that lands on a scan frame costs a full ~250 ms window
    // where globes inside the widened radius sit still. That is precisely the
    // feature whose documented failure mode IS intermittent player resolution
    // (Known Limitations item 7, `seen=176993 noplayer=176993`), and
    // `orbpickup stat` cannot show it: noplayer ticks once and the next scan
    // looks healthy. The per-frame enumeration this replaced resumed on the
    // very next frame; so does this.
    resetAll();
    g_PlayerPosValid.store(false);
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 } };
    OrbPickupTick(1);                                   // scan: no player, nothing cached
    check("playervalid/not_pulled_while_unresolved", world.find(1)->x, 100.0);
    g_PlayerPosValid.store(true);                       // ...and the player resolves again
    long long revalidAt = -1;
    for (uint32_t f = 2; f <= 1 + 2 * (uint32_t)kOrbScanFrames; ++f) {
        const double before = world.find(1)->x;
        OrbPickupTick(f);
        if (revalidAt < 0 && world.find(1)->x != before) revalidAt = f;
    }
    checkInt("playervalid/was_pulled", revalidAt >= 0 ? 1 : 0, 1);
    checkInt("playervalid/pulled_on_next_frame", revalidAt, 2);

    // --- 11. a handle that cannot outlive the frame is refused and counted --
    // This is the one cross-frame instance lifetime the throttle introduces: a
    // handle found by one scan is stored and handed to instance_exists and
    // variable_instance_get up to kOrbScanFrames frames later. That is safe
    // because instance_find returns a runtime-validated VALUE_REF id, which
    // survives the instance being destroyed - instance_exists simply answers
    // false. But that is a MEASURED property of this runner, and nothing in
    // the code held the runner to it. A VALUE_OBJECT is a CInstance*, and a
    // cached one pointing at a destroyed instance is the g_ForgedItems hazard.
    // So the kind is validated where the handle is STORED, refused for the
    // cache if it cannot outlive the frame, and the refusal counted into
    // `orbpickup stat` - AGENTS.md's validate-refuse-and-count, rather than a
    // crash dump.
    //
    // The refusal is for the cache only. The handle is perfectly good within
    // the frame that found it, so the globe is still pulled, at scan cadence
    // rather than per frame. Refusing the WORK would make orb pickup fully
    // inert on such a runner while still reporting itself on - Known
    // Limitations item 7 - which is the failure this whole predicate exists to
    // avoid, so it must not be reintroduced by the predicate itself.
    resetAll();
    world.handleKind = VALUE_OBJECT;                    // a struct pointer, not an id
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 } };
    for (uint32_t f = 1; f <= 3; ++f) OrbPickupTick(f);
    // One pull, on the scan frame: 100 -> 94 at kGlobePullSpeed. Frames 2 and 3
    // are not scans and the handle was deliberately not cached, so it stays 94
    // - which is also what proves the handle was not kept across frames.
    check("uncacheable/pulled_in_scan", world.find(1)->x, 94.0);
    checkInt("uncacheable/refusal_counted", g_OrbUncacheableKind, 1);
    checkInt("uncacheable/still_seen", g_OrbGlobesSeen, 1);

    // ...and every kind that CAN outlive a frame is still accepted. A kind
    // check that quietly narrows the accepted set to the one value this runner
    // returns today is the recurring bug of Known Limitations item 7, so the
    // older-runner bare instance id gets its own control rather than being
    // assumed dead.
    resetAll();
    world.handleKind = VALUE_REAL;                      // older runner: a bare id
    world.globes = { { 1, kExpGlobe, 100.0, 0.0 } };
    OrbPickupTick(1);
    check("bareid/pulled", world.find(1)->x, 100.0 - kGlobePullSpeed);
    checkInt("bareid/not_refused", g_OrbUncacheableKind, 0);

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
