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
// run, a target given up is not given up again until another target (or none)
// has been seen, and a target that vanishes before the run fills asks for
// nothing.
//
// Red first: with these scenarios written and the header holding only an empty
// namespace, the first error was `error C2039: 'PetLootStuckWatch': is not a
// member of 'ForgePact'` (the first of the using-declarations below).
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

static void TargetGivenUpOncePerTarget()
{
    // A target given up is not given up again while it stays the target, nor
    // after it leaves the radius and comes back; once another target, or
    // none, has been seen, the same id can be given up again (a coin, which
    // has no hold of its own, that sticks twice is counted twice). The
    // longest run keeps growing past the count while a given-up target stays
    // in reach, which is what shows a give-up that did not move the pet.
    PetLootStuckWatch w;
    const FeedResult stuck = Feed(w, 0, kPetLootStuckFrames + 500, kA, kNear);
    const FeedResult away = Feed(w, stuck.next, 10, kA, kFar);
    const FeedResult back = Feed(w, away.next, kPetLootStuckFrames * 3, kA, kNear);
    const FeedResult none = Feed(w, back.next, 1, kNone, 0.0);
    const FeedResult again = Feed(w, none.next, kPetLootStuckFrames, kA, kNear);
    const FeedResult other = Feed(w, again.next, 1, kB, kNear);
    const FeedResult third = Feed(w, other.next, kPetLootStuckFrames, kA, kNear);
    Check("target/given_up_once_per_target",
          stuck.count == 1 && stuck.first == kPetLootStuckFrames - 1 && away.count == 0 && back.count == 0 &&
          again.count == 1 && third.count == 1 && w.Longest() == kPetLootStuckFrames + 500,
          "stuck=" + std::to_string(stuck.count) + " back=" + std::to_string(back.count) +
          " again=" + std::to_string(again.count) + " third=" + std::to_string(third.count) +
          " longest=" + N(w.Longest()));
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

int main()
{
    BaselineGameKeepsASurvivingTarget();
    BaselineModOffNeverAsks();
    TargetConstants();
    TargetStuckTargetIsGivenUpAtExactlyTheCount();
    TargetTravellingTargetNeverCounts();
    TargetDifferentTargetRestarts();
    TargetNoTargetRestarts();
    TargetSkippedFrameRestarts();
    TargetGivenUpOncePerTarget();
    TargetVanishedTargetAsksNothing();
    TargetModOnAsksAndCounts();
    std::cout << (g_Failures ? "RESULT FAIL " + std::to_string(g_Failures) : std::string("RESULT OK")) << "\n";
    return g_Failures ? 1 : 0;
}
