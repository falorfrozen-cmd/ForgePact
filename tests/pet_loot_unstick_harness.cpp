// Behavioral harness for "Pet moves on from loot it cannot pick up"
// (`petunstick`, ForgePact issue #94: with lots of loot on the ground the
// companion stays on one item it cannot pick up).
//
// The Python runner splices the REAL plugin/include/ForgePact/PetLootUnstickMod.hpp
// in at the marker below (its #pragma and #include lines removed). The watch in
// it, PetLootStuckWatch, is game-independent by contract - it is handed a frame
// number, the pet's loot target id (or none) and the pet-to-target distance in
// pixels, never an instance - so it compiles here with no runtime stub. The
// only name the header needs from Common.hpp is Out(), which the mod's toggle
// line uses; it is defined below.
//
// What the game does without the mod (a static reading of Companion_obj's
// events, docs/pet-loot-stuck-research.md): the pet's `lootTarget` is replaced
// only when that instance ceases to exist. Arrival does not end the travel, and
// an item whose pickup fails stays on the ground and passes the next scan, so a
// target that survives is kept for as long as it survives. The baseline
// scenarios pin that rule, written out here as the reference, and pin that the
// real mod, while off, never asks for anything. The target scenarios pin the
// watch: the same target within kPetLootStuckRadiusPx is given up at exactly
// the frame its run reaches kPetLootStuckFrames, a travel beyond the radius
// never counts, a different target, no target or a skipped frame restarts the
// run, and a target that vanishes before the run fills asks for nothing.
//
// A target taken straight back (Replan 1 of workorder forgepact-pet-loot-stuck):
// after the tick drops the pet's target, the game can hand the same id back in
// its very next Step, so no tick reads "no target" in between. The pre-fix
// rules, written out below as LatchedReference, gave such a target up once and
// never again, and their re-pick count skipped a target equal to the previous
// tick's, so a pet stuck on a coin was helped at most once and nothing said
// so. The fixed rules live in the header: the watch re-arms at each give-up
// (the same target, still in reach, is given up again every
// kPetLootStuckFrames frames), and PetLootRepickRing counts a target seen on
// the frame right after its give-up as a re-pick, once per give-up. TickSim
// replays the tick's own order (re-pick decision, then the watch, then the
// give-up remembered) through the real header.
//
// Red first: with these scenarios written and the header holding only an empty
// namespace, the first error was `error C2039: 'PetLootStuckWatch': is not a
// member of 'ForgePact'` (the first of the using-declarations below). Replan 1's
// scenarios, written against the unfixed (latching) header, first failed with
// `error C2039: 'PetLootRepickRing': is not a member of 'ForgePact'`.
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

static std::vector<std::string> g_Logged;
static void Out(const std::string& s) { g_Logged.push_back(s); }

// PRODUCTION_PETUNSTICK

using ForgePact::PetLootStuckWatch;
using ForgePact::PetLootRepickRing;
using ForgePact::PetLootRepick;
using ForgePact::PetLootKind;
using ForgePact::PetLootUnstickMod;
using ForgePact::kPetLootStuckFrames;
using ForgePact::kPetLootStuckRadiusPx;
using ForgePact::kPetLootHoldFrames;

static int g_Failures = 0;

static void Check(const std::string& label, bool ok, const std::string& detail)
{
    if (ok) std::cout << "PASS " << label << "\n";
    else { std::cout << "FAIL " << label << " " << detail << "\n"; ++g_Failures; }
}

// Ids are GameMaker instance ids, as `lootTarget` holds them (a real).
static constexpr double kA = 100001.0, kB = 100002.0;
// Inside the game's own 144 px pickup circle, and well outside the radius.
static constexpr double kNear = 40.0, kFar = 600.0;
static const std::optional<double> kNone = std::nullopt;

struct FeedResult {
    int64_t first = -1;   // the frame of the first give-up, or -1
    int count = 0;        // how many frames answered give up
    int64_t next = 0;     // the frame after the last one fed
};

// Feeds `frames` consecutive frames from `start` with one target and distance.
template <class Watch>
static FeedResult Feed(Watch& w, int64_t start, int64_t frames, std::optional<double> target, double distance)
{
    FeedResult r;
    for (int64_t f = start; f < start + frames; ++f) {
        if (w.Observe(f, target, distance)) {
            if (r.first < 0) r.first = f;
            ++r.count;
        }
    }
    r.next = start + frames;
    return r;
}

static std::string N(int64_t v) { return std::to_string((long long)v); }

// The game's retarget rule, as the reference the target departs from: the
// scan's pick replaces the current target only when the current target's
// instance no longer exists.
static std::optional<double> GameRetarget(std::optional<double> current, bool currentExists,
                                          std::optional<double> scanPick)
{
    if (current && currentExists) return current;
    return scanPick;
}

// The pre-fix rules, as the reference the fix departs from (the header and the
// tick before Replan 1): the watch gives a target up once and then answers
// nothing for it until a different target, or none, has been seen; the re-pick
// count returns at once when this tick's target equals the previous tick's,
// and otherwise counts a give-up of the same id younger than
// kPetLootHoldFrames. Frame() replays one tick in the tick's order.
struct LatchedReference {
    std::optional<double> target;
    bool givenUp = false;
    int64_t run = 0;
    int64_t lastFrame = 0;
    bool haveLast = false;
    std::vector<std::pair<double, int64_t>> giveUps;   // id, frame
    std::optional<double> prev;
    int64_t prevFrame = -2;
    int repicks = 0;

    bool Observe(int64_t frame, double t, double distancePx)
    {
        const bool consecutive = haveLast && frame == lastFrame + 1;
        lastFrame = frame;
        haveLast = true;
        if (!target || *target != t) { target = t; givenUp = false; run = 0; }
        if (!consecutive || !(distancePx <= kPetLootStuckRadiusPx)) run = 0;
        if (!(distancePx <= kPetLootStuckRadiusPx)) return false;
        ++run;
        if (givenUp || run < kPetLootStuckFrames) return false;
        givenUp = true;
        return true;
    }

    void Frame(int64_t f, std::optional<double> t, double distancePx)
    {
        const std::optional<double> previous = f == prevFrame + 1 ? prev : std::nullopt;
        prevFrame = f;
        prev.reset();
        if (!t) { target.reset(); givenUp = false; run = 0; haveLast = false; return; }
        prev = t;
        if (!(previous && *previous == *t)) {
            for (const auto& g : giveUps) {
                if (g.first == *t && f - g.second < kPetLootHoldFrames) { ++repicks; break; }
            }
        }
        if (Observe(f, *t, distancePx)) giveUps.push_back({ *t, f });
    }
};

// The fixed rules, through the real header, in PetLootUnstickTick's order: the
// previous tick's live target (none unless the previous frame saw one), the
// re-pick decision on every tick that sees a live target, then the watch, and
// a give-up remembered with its kind once the watch answers. No target resets
// the watch, as the tick's no-target route does.
struct TickSim {
    PetLootStuckWatch watch;
    PetLootRepickRing ring;
    PetLootKind kind = PetLootKind::Ground;
    std::optional<double> prev;
    int64_t prevFrame = -2;
    std::vector<int64_t> giveUps;
    std::vector<int64_t> repickFrames;
    std::vector<PetLootKind> repickKinds;

    void Frame(int64_t f, std::optional<double> t, double distancePx)
    {
        const std::optional<double> previous = f == prevFrame + 1 ? prev : std::nullopt;
        prevFrame = f;
        prev.reset();
        if (!t) { watch.Reset(); return; }
        prev = t;
        if (const std::optional<PetLootRepick> hit = ring.Seen(f, *t, previous)) {
            repickFrames.push_back(f);
            repickKinds.push_back(hit->kind);
        }
        if (watch.Observe(f, t, distancePx)) {
            giveUps.push_back(f);
            ring.Remember(f, *t, kind);
        }
    }
};

// ---- baseline ---------------------------------------------------------------

static void BaselineGameKeepsASurvivingTarget()
{
    // A survives (its pickup keeps failing) for 600 frames while the scan
    // offers B every frame: the pet keeps A throughout - the reported
    // symptom's shape. Negative control: once A ceases to exist, B is taken.
    std::optional<double> target = kA;
    bool keptThroughout = true;
    for (int f = 0; f < 600; ++f) {
        target = GameRetarget(target, true, kB);
        keptThroughout = keptThroughout && target && *target == kA;
    }
    const auto afterGone = GameRetarget(target, false, kB);
    Check("baseline/game_keeps_a_surviving_target",
          keptThroughout && afterGone && *afterGone == kB,
          "kept=" + std::to_string(keptThroughout));
}

static void BaselineModOffNeverAsks()
{
    // Off by default; while off, the mod answers nothing for a target that
    // sits within reach far longer than the stuck count, and counts nothing.
    PetLootUnstickMod& mod = PetLootUnstickMod::Instance();
    const bool offByDefault = !mod.IsEnabled();
    const FeedResult r = Feed(mod, 1, 600, kA, kNear);
    Check("baseline/mod_off_never_asks",
          offByDefault && r.count == 0 && mod.HeldBack() == 0 && mod.CoinsReleased() == 0 &&
          mod.LongestRun() == 0 && g_Logged.empty(),
          "offByDefault=" + std::to_string(offByDefault) + " asks=" + std::to_string(r.count) +
          " longest=" + N(mod.LongestRun()));
}

static void BaselineLatchedWatchKeepsATargetTakenStraightBack()
{
    // The pre-fix rules fed the same id, within reach, on every frame -
    // including the frame right after the give-up, as when the game hands a
    // dropped target straight back - for kPetLootHoldFrames frames: one
    // give-up and then nothing, and no re-pick counted, so the stat line
    // showed neither. Positive control on the same reference: one tick of no
    // target and then the same id is counted as a re-pick.
    LatchedReference ref;
    for (int64_t f = 0; f < kPetLootHoldFrames; ++f) ref.Frame(f, kA, kNear);
    const size_t giveUps = ref.giveUps.size();
    const int64_t first = ref.giveUps.empty() ? -1 : ref.giveUps[0].second;
    const int repicks = ref.repicks;
    ref.Frame(kPetLootHoldFrames, kNone, 0.0);
    ref.Frame(kPetLootHoldFrames + 1, kA, kNear);
    Check("baseline/latched_watch_keeps_a_target_taken_straight_back",
          giveUps == 1 && first == kPetLootStuckFrames - 1 && repicks == 0 && ref.repicks == 1,
          "giveUps=" + std::to_string(giveUps) + " first=" + N(first) + " repicks=" + std::to_string(repicks) +
          " control=" + std::to_string(ref.repicks));
}

// ---- target -----------------------------------------------------------------

static void TargetConstants()
{
    // 1.5 s at 60 fps; a radius just outside the game's 144 px pickup circle;
    // the same ~10 s hold the Pet Quest Collector uses.
    Check("target/constants",
          kPetLootStuckFrames == 90 && kPetLootStuckRadiusPx == 160.0 && kPetLootStuckRadiusPx > 144.0 &&
          kPetLootHoldFrames == 600,
          "frames=" + N(kPetLootStuckFrames) + " hold=" + N(kPetLootHoldFrames));
}

static void TargetStuckTargetIsGivenUpAtExactlyTheCount()
{
    // The first in-reach frame counts 1, so the answer comes on the
    // kPetLootStuckFrames-th consecutive frame and not one earlier.
    PetLootStuckWatch w;
    const FeedResult early = Feed(w, 1000, kPetLootStuckFrames - 1, kA, kNear);
    const bool onTime = w.Observe(early.next, kA, kNear);
    const bool atEdge = [] {
        PetLootStuckWatch e;   // the radius itself is within reach
        return Feed(e, 0, kPetLootStuckFrames, kA, kPetLootStuckRadiusPx).first == kPetLootStuckFrames - 1;
    }();
    Check("target/stuck_target_is_given_up_at_exactly_the_count",
          early.count == 0 && onTime && early.next == 1000 + kPetLootStuckFrames - 1 && atEdge &&
          w.Longest() == kPetLootStuckFrames,
          "early=" + std::to_string(early.count) + " onTime=" + std::to_string(onTime) +
          " atEdge=" + std::to_string(atEdge) + " longest=" + N(w.Longest()));
}

static void TargetTravellingTargetNeverCounts()
{
    // Beyond the radius the pet is travelling: 600 frames ask nothing, and
    // the count starts on the first in-reach frame. Just past the radius, and
    // an unreadable (NaN) distance, do not count either.
    PetLootStuckWatch w;
    const FeedResult far = Feed(w, 0, 600, kA, kFar);
    const FeedResult edge = Feed(w, far.next, 600, kA, kPetLootStuckRadiusPx + 0.5);
    const FeedResult nan = Feed(w, edge.next, 600, kA, std::nan(""));
    const FeedResult arrived = Feed(w, nan.next, kPetLootStuckFrames, kA, kNear);
    Check("target/travelling_target_never_counts",
          far.count == 0 && edge.count == 0 && nan.count == 0 &&
          arrived.count == 1 && arrived.first == nan.next + kPetLootStuckFrames - 1,
          "far=" + std::to_string(far.count) + " edge=" + std::to_string(edge.count) +
          " nan=" + std::to_string(nan.count) + " arrived.first=" + N(arrived.first));
}

static void TargetDifferentTargetRestarts()
{
    PetLootStuckWatch w;
    const FeedResult a = Feed(w, 0, kPetLootStuckFrames - 1, kA, kNear);
    const FeedResult b = Feed(w, a.next, kPetLootStuckFrames - 1, kB, kNear);
    const bool bOnTime = w.Observe(b.next, kB, kNear);
    Check("target/different_target_restarts",
          a.count == 0 && b.count == 0 && bOnTime,
          "a=" + std::to_string(a.count) + " b=" + std::to_string(b.count) + " bOnTime=" + std::to_string(bOnTime));
}

static void TargetNoTargetRestarts()
{
    // One frame with no target between two runs on the same item restarts
    // the count; so does Reset(), which the tick calls with no pet out.
    PetLootStuckWatch w;
    const FeedResult a1 = Feed(w, 0, kPetLootStuckFrames - 1, kA, kNear);
    const FeedResult gap = Feed(w, a1.next, 1, kNone, 0.0);
    const FeedResult a2 = Feed(w, gap.next, kPetLootStuckFrames - 1, kA, kNear);
    w.Reset();
    const FeedResult a3 = Feed(w, a2.next, kPetLootStuckFrames - 1, kA, kNear);
    const bool onTime = w.Observe(a3.next, kA, kNear);
    Check("target/no_target_restarts",
          a1.count == 0 && gap.count == 0 && a2.count == 0 && a3.count == 0 && onTime,
          "a2=" + std::to_string(a2.count) + " a3=" + std::to_string(a3.count) + " onTime=" + std::to_string(onTime));
}

static void TargetSkippedFrameRestarts()
{
    // "Consecutive" means consecutive: a frame number that skips (the mod
    // was off in between) restarts the count.
    PetLootStuckWatch w;
    const FeedResult a1 = Feed(w, 0, kPetLootStuckFrames - 1, kA, kNear);
    const FeedResult a2 = Feed(w, a1.next + 1, kPetLootStuckFrames - 1, kA, kNear);
    const bool onTime = w.Observe(a2.next, kA, kNear);
    Check("target/skipped_frame_restarts", a1.count == 0 && a2.count == 0 && onTime,
          "a2=" + std::to_string(a2.count) + " onTime=" + std::to_string(onTime));
}

// The baseline's sequence through the real header: the same id, within reach,
// on every frame including the one right after each give-up, for
// kPetLootHoldFrames + 1 frames (so the feed ends after its last give-up and
// that give-up is followed by its re-pick). Then, kept from the old
// once-per-target scenario: ten frames travelling beyond the radius give
// nothing up, and back in reach the run restarts from the first in-reach frame.
static bool TakenStraightBackIsGivenUpAgain(PetLootKind kind, std::string& detail)
{
    TickSim sim;
    sim.kind = kind;
    const int64_t frames = kPetLootHoldFrames + 1;
    for (int64_t f = 0; f < frames; ++f) sim.Frame(f, kA, kNear);

    const size_t n = sim.giveUps.size();
    const size_t expected = (size_t)((frames - kPetLootStuckFrames) / kPetLootStuckFrames + 1);
    bool spaced = n > 0 && sim.giveUps[0] == kPetLootStuckFrames - 1;
    for (size_t i = 1; i < n; ++i)
        spaced = spaced && sim.giveUps[i] - sim.giveUps[i - 1] == kPetLootStuckFrames;
    bool repickEach = sim.repickFrames.size() == n;
    for (size_t i = 0; repickEach && i < n; ++i)
        repickEach = sim.repickFrames[i] == sim.giveUps[i] + 1 && sim.repickKinds[i] == kind;
    const bool endsAfter = n > 0 && sim.giveUps.back() < frames - 1;
    const long ln = (long)n;
    const bool counted = sim.ring.Repicked() == ln &&
        sim.ring.Ground() == (kind == PetLootKind::Ground ? ln : 0) &&
        sim.ring.Coin() == (kind == PetLootKind::Coin ? ln : 0);

    const int64_t away = frames;
    for (int64_t f = away; f < away + 10; ++f) sim.Frame(f, kA, kFar);
    const size_t beforeBack = sim.giveUps.size();
    const size_t repicksBeforeBack = sim.repickFrames.size();
    for (int64_t f = away + 10; f < away + 10 + kPetLootStuckFrames; ++f) sim.Frame(f, kA, kNear);
    const bool awayBack = beforeBack == n && sim.giveUps.size() == n + 1 &&
        sim.giveUps.back() == away + 10 + kPetLootStuckFrames - 1 && sim.repickFrames.size() == repicksBeforeBack;

    const bool longest = sim.watch.Longest() == kPetLootStuckFrames;
    detail = "giveUps=" + std::to_string(n) + "/" + std::to_string(expected) + " spaced=" + std::to_string(spaced) +
             " repickEach=" + std::to_string(repickEach) + " endsAfter=" + std::to_string(endsAfter) +
             " repicked=" + std::to_string(sim.ring.Repicked()) + " ground=" + std::to_string(sim.ring.Ground()) +
             " coin=" + std::to_string(sim.ring.Coin()) + " awayBack=" + std::to_string(awayBack) +
             " longest=" + N(sim.watch.Longest());
    return n == expected && n >= 2 && spaced && repickEach && endsAfter && counted && awayBack && longest;
}

static void TargetGroundTakenStraightBackIsGivenUpAgain()
{
    // A ground item the game hands straight back (its timer reset, or the
    // item counted as timer absent=): given up every kPetLootStuckFrames
    // frames, each give-up followed by one ground re-pick, and the longest
    // run between give-ups never past the count.
    std::string detail;
    const bool ok = TakenStraightBackIsGivenUpAgain(PetLootKind::Ground, detail);
    Check("target/ground_taken_straight_back_is_given_up_again", ok, detail);
}

static void TargetCoinTakenStraightBackIsGivenUpAgain()
{
    // A coin has no itemCompanionTimer, so only the target is dropped and the
    // game may pick it again at once: the case a latched watch helped at
    // most once. Counted under coin, never under ground.
    std::string detail;
    const bool ok = TakenStraightBackIsGivenUpAgain(PetLootKind::Coin, detail);
    Check("target/coin_taken_straight_back_is_given_up_again", ok, detail);
}

static void TargetRepickCountedOncePerGiveUp()
{
    // After one tick of no target (the case counted before the fix too).
    PetLootRepickRing afterNone;
    afterNone.Remember(100, kA, PetLootKind::Ground);
    const bool none = afterNone.Seen(102, kA, kNone).has_value();
    // A different target in between, then the given-up id.
    PetLootRepickRing viaOther;
    viaOther.Remember(200, kA, PetLootKind::Ground);
    const bool otherFirst = viaOther.Seen(201, kB, kNone).has_value();
    const bool thenA = viaOther.Seen(202, kA, kB).has_value();
    // An id never given up, however it arrives (negative control).
    PetLootRepickRing never;
    never.Remember(300, kA, PetLootKind::Ground);
    const bool neverB = never.Seen(301, kB, kNone).has_value() || never.Seen(302, kB, kA).has_value() ||
                        never.Seen(303, kB, kB).has_value();
    // Younger than kPetLootHoldFrames counts; that old or older does not.
    PetLootRepickRing aged;
    aged.Remember(1000, kA, PetLootKind::Coin);
    const bool young = aged.Seen(1000 + kPetLootHoldFrames - 1, kA, kNone).has_value();
    const bool atHold = aged.Seen(1000 + kPetLootHoldFrames, kA, kNone).has_value();
    const bool older = aged.Seen(1000 + kPetLootHoldFrames + 1, kA, kNone).has_value();
    // Through the tick's order: a target that stays the target after its
    // re-pick (travelling, so nothing is given up) is counted once, not once
    // per tick; leaving it for a tick and taking it back within the hold
    // counts again, once.
    TickSim sim;
    for (int64_t f = 0; f < kPetLootStuckFrames; ++f) sim.Frame(f, kA, kNear);
    const int64_t gaveUp = sim.giveUps.empty() ? -1 : sim.giveUps[0];
    for (int64_t f = kPetLootStuckFrames; f < kPetLootStuckFrames + 50; ++f) sim.Frame(f, kA, kFar);
    sim.Frame(kPetLootStuckFrames + 50, kNone, 0.0);
    for (int64_t f = kPetLootStuckFrames + 51; f < kPetLootStuckFrames + 61; ++f) sim.Frame(f, kA, kFar);
    const bool stays = sim.giveUps.size() == 1 && gaveUp == kPetLootStuckFrames - 1 &&
        sim.repickFrames == std::vector<int64_t>{ kPetLootStuckFrames, kPetLootStuckFrames + 51 } &&
        sim.ring.Repicked() == 2 && sim.ring.Ground() == 2;
    const bool counters = afterNone.Repicked() == 1 && viaOther.Repicked() == 1 && never.Repicked() == 0 &&
        aged.Repicked() == 1 && aged.Coin() == 1 && aged.Ground() == 0;
    Check("target/repick_counted_once_per_give_up",
          none && !otherFirst && thenA && !neverB && young && !atHold && !older && stays && counters,
          "none=" + std::to_string(none) + " otherFirst=" + std::to_string(otherFirst) +
          " thenA=" + std::to_string(thenA) + " neverB=" + std::to_string(neverB) +
          " young=" + std::to_string(young) + " atHold=" + std::to_string(atHold) +
          " older=" + std::to_string(older) + " stays=" + std::to_string(stays) +
          " repicks=" + std::to_string(sim.repickFrames.size()) + " counters=" + std::to_string(counters));
}

static void TargetVanishedTargetAsksNothing()
{
    // The pickup worked: the item went away before the count filled, the game
    // dropped the target, and the watch asks for nothing however long the pet
    // then goes without a target.
    PetLootStuckWatch w;
    const FeedResult a = Feed(w, 0, kPetLootStuckFrames - 30, kA, kNear);
    const FeedResult gone = Feed(w, a.next, 600, kNone, 0.0);
    Check("target/vanished_target_asks_nothing", a.count == 0 && gone.count == 0 && w.Run() == 0,
          "a=" + std::to_string(a.count) + " gone=" + std::to_string(gone.count));
}

static void TargetModOnAsksAndCounts()
{
    // Through the real singleton: on, it answers exactly like the watch; the
    // toggle prints one line each way; the tick's counters land in the stat
    // line `petunstick 0` prints; off, it asks nothing, and a run while off
    // forgets the watch, so the item given up before is a fresh target when
    // the mod comes back on.
    PetLootUnstickMod& mod = PetLootUnstickMod::Instance();
    const size_t logged = g_Logged.size();
    mod.SetEnabled(true);
    const FeedResult on = Feed(mod, 5000, kPetLootStuckFrames, kA, kNear);
    mod.NoteHeldBack();
    mod.NoteHeldBack();
    mod.NoteCoinReleased();
    const std::string stat = mod.StatLine();
    mod.SetEnabled(false);
    const FeedResult off = Feed(mod, on.next, 600, kA, kNear);
    mod.SetEnabled(true);
    const FeedResult restarted = Feed(mod, off.next, kPetLootStuckFrames, kA, kNear);
    mod.SetEnabled(false);
    const bool lines = g_Logged.size() == logged + 4 &&
        g_Logged[logged].rfind("petunstick -> ON", 0) == 0 &&
        g_Logged[logged + 1].rfind("petunstick -> OFF", 0) == 0;
    const std::string expected = "petunstick stat: held back=2 coins released=1 longest same-target=" +
                                 N(kPetLootStuckFrames) + " frames";
    Check("target/mod_on_asks_and_counts",
          on.count == 1 && on.first == 5000 + kPetLootStuckFrames - 1 && off.count == 0 &&
          restarted.count == 1 && restarted.first == off.next + kPetLootStuckFrames - 1 &&
          lines && stat == expected && mod.HeldBack() == 2 && mod.CoinsReleased() == 1,
          "on=" + std::to_string(on.count) + " off=" + std::to_string(off.count) +
          " restarted=" + std::to_string(restarted.count) + " lines=" + std::to_string(lines) +
          " stat='" + stat + "'");
}

static void TargetNotLootDropsOnSight()
{
    // Live 2 (2026-10-02): the pet's lootTarget can hold a stale instance id
    // the game reused for something that is not loot at all (measured: a
    // zone's decoration object), and the watch - a 160 px, 90-frame count -
    // is the wrong tool for it. The routing question is pure, and this pins
    // its whole truth table: both loot families feed the watch, a readable
    // kind from neither is dropped on sight, and an unreadable kind is not
    // evidence of a wrong id, so it keeps the watch route too.
    const bool ground = ForgePact::PetLootRoute(true, true, false) == ForgePact::PetLootTargetRoute::Watch;
    const bool coin = ForgePact::PetLootRoute(true, false, true) == ForgePact::PetLootTargetRoute::Watch;
    const bool other = ForgePact::PetLootRoute(true, false, false) == ForgePact::PetLootTargetRoute::DropOnSight;
    const bool unreadable = ForgePact::PetLootRoute(false, false, false) == ForgePact::PetLootTargetRoute::Watch;
    Check("target/not_loot_target_drops_on_sight", ground && coin && other && unreadable,
          "ground=" + std::to_string(ground) + " coin=" + std::to_string(coin) +
          " other=" + std::to_string(other) + " unreadable=" + std::to_string(unreadable));
}

int main()
{
    BaselineGameKeepsASurvivingTarget();
    BaselineModOffNeverAsks();
    BaselineLatchedWatchKeepsATargetTakenStraightBack();
    TargetConstants();
    TargetStuckTargetIsGivenUpAtExactlyTheCount();
    TargetTravellingTargetNeverCounts();
    TargetDifferentTargetRestarts();
    TargetNoTargetRestarts();
    TargetSkippedFrameRestarts();
    TargetGroundTakenStraightBackIsGivenUpAgain();
    TargetCoinTakenStraightBackIsGivenUpAgain();
    TargetRepickCountedOncePerGiveUp();
    TargetVanishedTargetAsksNothing();
    TargetModOnAsksAndCounts();
    TargetNotLootDropsOnSight();
    std::cout << (g_Failures ? "RESULT FAIL " + std::to_string(g_Failures) : std::string("RESULT OK")) << "\n";
    return g_Failures ? 1 : 0;
}
