// Behavioral harness for the Pet Quest Collector's target selection (ForgePact
// issue #94: with many quest items on screen the pet circled one of them).
//
// The Python runner splices the REAL plugin/include/ForgePact/PetQuestCollectorMod.hpp
// in at the marker below (its #pragma and #include lines removed). The selector
// in it, PetQuestSelector, is game-independent by contract - it is handed ids,
// squared distances and frame numbers, never an instance - so it compiles here
// with no runtime stub. The only name the header needs from Common.hpp is
// Out(), which the mod's toggle line uses; it is defined below.
//
// What the tick did before the fix (ModuleMain.cpp's PetQuestCollectorTick, a
// static reading of our own code): Idle picks the nearest collectable item on
// screen with no memory of what just failed, so a collect that left the item
// in place, a gate refusal at arrival or a travel timeout each made the same
// item the next target, forever; and the Idle walk read at most 64 family
// instances per tick from index 0, so a collectable item past index 63 was
// never a candidate. The baseline scenarios pin that rule, written out here as
// the reference, and pin that the real selector still picks the nearest when
// nothing has failed. The target scenarios pin the fix: a failed target is
// held back for kPetQuestHoldFrames and then eligible again, every candidate
// held back picks none (the pet waits instead of spinning), the family cursor
// reaches past the budget within two ticks, and a target is never swapped
// while the pet travels to it.
//
// Red first: with these scenarios written and the header unchanged, the first
// error was `error C2039: 'PetQuestCandidate': is not a member of 'ForgePact'`
// (the first of the using-declarations below), then the same for
// `PetQuestOutcome`.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

static std::vector<std::string> g_Logged;
static void Out(const std::string& s) { g_Logged.push_back(s); }

// PRODUCTION_PETQUEST

using ForgePact::PetQuestCandidate;
using ForgePact::PetQuestOutcome;
using ForgePact::PetQuestSelector;
using ForgePact::kPetQuestHoldFrames;
using ForgePact::kPetQuestHoldMax;

static int g_Failures = 0;

static void Check(const std::string& label, bool ok, const std::string& detail)
{
    if (ok) std::cout << "PASS " << label << "\n";
    else { std::cout << "FAIL " << label << " " << detail << "\n"; ++g_Failures; }
}

static std::string Id(const std::optional<double>& id)
{
    return id ? std::to_string((long long)*id) : std::string("none");
}

// Ids are GameMaker instance ids; distances are squared pixels, as the tick
// computes them. A is the nearest, then B, then C.
static constexpr double kA = 100001.0, kB = 100002.0, kC = 100003.0;
static std::vector<PetQuestCandidate> ThreeItems()
{
    return { { kB, 400.0 }, { kA, 100.0 }, { kC, 900.0 } };
}

// The pre-fix Idle rule, as the reference the target departs from: the
// nearest candidate, with nothing remembered between picks.
static std::optional<double> PreFixNearest(const std::vector<PetQuestCandidate>& c)
{
    std::optional<double> best;
    double bestD2 = -1.0;
    for (const PetQuestCandidate& x : c)
        if (bestD2 < 0.0 || x.distance2 < bestD2) { bestD2 = x.distance2; best = x.id; }
    return best;
}

// The pre-fix walk: from index 0, at most `budget` instances, every tick.
static std::vector<int> PreFixWalk(int total, int budget)
{
    std::vector<int> seen;
    for (int i = 0; i < total && budget > 0; ++i, --budget) seen.push_back(i);
    return seen;
}

// The fixed walk, exactly as the tick does it: start at the cursor, wrap.
static std::vector<int> CursorWalk(PetQuestSelector& s, int total, int budget)
{
    std::vector<int> seen;
    const int start = s.NextStart(total, budget);
    const int n = std::min(total, budget);
    for (int k = 0; k < n; ++k) seen.push_back((start + k) % total);
    return seen;
}

static bool Has(const std::vector<int>& v, int i) { return std::find(v.begin(), v.end(), i) != v.end(); }

// ---- baseline ---------------------------------------------------------------

static void BaselineNearestRepicksAFailedTarget()
{
    // The reference: after a collect on A that left A in place, the pre-fix
    // rule picks A again at once - the reported symptom's shape.
    const auto items = ThreeItems();
    const auto first = PreFixNearest(items);
    const auto again = PreFixNearest(items);   // A is still there
    const bool refPinsTheBug = first && *first == kA && again && *again == kA;

    // The real selector with nothing failed is that same nearest rule: a
    // successful collect holds nothing back, so an item that somehow
    // remained would be picked again, exactly as before.
    PetQuestSelector s;
    const auto p1 = s.Pick(items, 10);
    const bool held = s.Note(PetQuestOutcome::Collected, 20);
    const auto p2 = s.Pick(items, 30);
    const bool selectorIsNearest = p1 && *p1 == kA && !held && p2 && *p2 == kA && s.HeldBack() == 0;

    Check("baseline/nearest_repicks_a_failed_target", refPinsTheBug && selectorIsNearest,
          "ref " + Id(first) + "," + Id(again) + " selector " + Id(p1) + "," + Id(p2) +
          " held=" + std::to_string(held) + " heldBack=" + std::to_string(s.HeldBack()));
}

static void BaselineWalkNeverReachesPastTheBudget()
{
    // The pre-fix walk with 100 family instances and a budget of 64: index
    // 70 is never read, however many ticks pass.
    bool reached = false;
    for (int tick = 0; tick < 5; ++tick) reached = reached || Has(PreFixWalk(100, 64), 70);
    Check("baseline/walk_never_reaches_past_the_budget", !reached, "");
}

// ---- target -----------------------------------------------------------------

static void TargetFailedTargetIsHeldBack()
{
    // Each outcome that leaves the item in place holds it back; the next pick
    // is the next-nearest item, and `held back=` counts each hold.
    bool ok = true;
    std::string detail;
    const PetQuestOutcome failures[] = { PetQuestOutcome::NoEffect, PetQuestOutcome::Gate,
                                         PetQuestOutcome::Timeout, PetQuestOutcome::Refused };
    for (PetQuestOutcome o : failures) {
        PetQuestSelector s;
        const auto p1 = s.Pick(ThreeItems(), 100);
        const bool held = s.Note(o, 120);
        const auto p2 = s.Pick(ThreeItems(), 144);
        const bool this_ok = p1 && *p1 == kA && held && s.IsHeld(kA, 144) && p2 && *p2 == kB && s.HeldBack() == 1;
        if (!this_ok) detail += " outcome=" + std::to_string((int)o) + " p1=" + Id(p1) + " p2=" + Id(p2);
        ok = ok && this_ok;
    }
    // Negative controls: a collect that removed the item, and a target that
    // vanished on its own, hold nothing back and count nothing.
    for (PetQuestOutcome o : { PetQuestOutcome::Collected, PetQuestOutcome::Lost, PetQuestOutcome::Abandoned }) {
        PetQuestSelector s;
        s.Pick(ThreeItems(), 100);
        const bool held = s.Note(o, 120);
        const bool this_ok = !held && !s.IsHeld(kA, 121) && s.HeldBack() == 0;
        if (!this_ok) detail += " control outcome=" + std::to_string((int)o) + " held";
        ok = ok && this_ok;
    }
    Check("target/failed_target_is_held_back", ok, detail);
}

static void TargetHoldExpires()
{
    // A held item is left alone for kPetQuestHoldFrames and is eligible again
    // after: the hold is a pause, not a ban.
    PetQuestSelector s;
    const std::vector<PetQuestCandidate> onlyA{ { kA, 100.0 } };
    s.Pick(onlyA, 1000);
    s.Note(PetQuestOutcome::NoEffect, 1000);
    const auto during = s.Pick(onlyA, 1000 + kPetQuestHoldFrames - 1);
    const auto after = s.Pick(onlyA, 1000 + kPetQuestHoldFrames);
    Check("target/hold_expires", kPetQuestHoldFrames >= 300 && !during && after && *after == kA,
          "hold=" + std::to_string(kPetQuestHoldFrames) + " during=" + Id(during) + " after=" + Id(after));
}

static void TargetAllHeldBackPicksNone()
{
    // Every candidate held back: no pick, and the selector is not travelling,
    // so the pet waits instead of spinning between failed items.
    PetQuestSelector s;
    s.Hold(kA, 50);
    s.Hold(kB, 50);
    s.Hold(kC, 50);
    const auto p = s.Pick(ThreeItems(), 60);
    // An empty screen picks none as well.
    const auto e = s.Pick({}, 60);
    Check("target/all_held_back_picks_none", !p && !e && !s.Travelling(),
          "pick=" + Id(p) + " empty=" + Id(e));
}

static void TargetCursorReachesPastTheBudget()
{
    // 100 family instances, budget 64: the item at index 70 is read within
    // two ticks, and every index within two ticks.
    PetQuestSelector s;
    const auto t1 = CursorWalk(s, 100, 64);
    const auto t2 = CursorWalk(s, 100, 64);
    bool all = true;
    for (int i = 0; i < 100; ++i) all = all && (Has(t1, i) || Has(t2, i));
    const bool bounded = t1.size() == 64 && t2.size() == 64;
    // A family within the budget is walked whole, from 0, every tick.
    PetQuestSelector small;
    const auto s1 = CursorWalk(small, 40, 64);
    const auto s2 = CursorWalk(small, 40, 64);
    const bool whole = s1.size() == 40 && s1.front() == 0 && s2.size() == 40 && s2.front() == 0;
    // A family that shrank below the cursor still starts inside it (the
    // cursor is at 90 after three walks of 30 over 100).
    PetQuestSelector shrink;
    for (int tick = 0; tick < 3; ++tick) CursorWalk(shrink, 100, 30);
    const int start = shrink.NextStart(40, 30);
    Check("target/cursor_reaches_past_the_budget",
          (Has(t1, 70) || Has(t2, 70)) && all && bounded && whole && start >= 0 && start < 40,
          "t2.front=" + std::to_string(t2.empty() ? -1 : t2.front()) + " start=" + std::to_string(start));
}

static void TargetTravelTargetIsKept()
{
    // While the pet travels, a nearer item appearing does not swap the
    // target; the next pick after the travel ends does see it.
    PetQuestSelector s;
    const auto p1 = s.Pick(ThreeItems(), 10);
    std::vector<PetQuestCandidate> withNearer = ThreeItems();
    withNearer.push_back({ 100009.0, 1.0 });
    const auto p2 = s.Pick(withNearer, 11);
    const bool travelling = s.Travelling();
    s.Note(PetQuestOutcome::Collected, 40);
    const auto p3 = s.Pick(withNearer, 70);
    Check("target/travel_target_is_kept",
          p1 && *p1 == kA && p2 && *p2 == kA && travelling && p3 && *p3 == 100009.0,
          Id(p1) + "," + Id(p2) + "," + Id(p3));
}

static void TargetHoldSetIsBounded()
{
    // At most kPetQuestHoldMax ids are held; past that the one closest to
    // expiring gives way, and the newest hold is always kept.
    PetQuestSelector s;
    for (int i = 0; i < 40; ++i) s.Hold(200000.0 + i, 100 + i);
    const bool newestHeld = s.IsHeld(200039.0, 140);
    const bool oldestGone = !s.IsHeld(200000.0, 140);
    // A second hold on one id refreshes it rather than taking a second slot.
    PetQuestSelector r;
    r.Hold(kA, 0);
    r.Hold(kA, 500);
    const bool refreshed = r.HeldCount(501) == 1 && r.IsHeld(kA, 500 + kPetQuestHoldFrames - 1);
    Check("target/hold_set_is_bounded",
          kPetQuestHoldMax == 32 && s.HeldCount(140) == kPetQuestHoldMax && newestHeld && oldestGone && refreshed,
          "held=" + std::to_string(s.HeldCount(140)));
}

static void TargetNoteWithoutATargetHoldsNothing()
{
    // A Note that closes no travel (the tick's defensive paths) holds and
    // counts nothing.
    PetQuestSelector s;
    const bool held = s.Note(PetQuestOutcome::NoEffect, 5);
    Check("target/note_without_a_target_holds_nothing", !held && s.HeldBack() == 0 && s.HeldCount(5) == 0, "");
}

int main()
{
    BaselineNearestRepicksAFailedTarget();
    BaselineWalkNeverReachesPastTheBudget();
    TargetFailedTargetIsHeldBack();
    TargetHoldExpires();
    TargetAllHeldBackPicksNone();
    TargetCursorReachesPastTheBudget();
    TargetTravelTargetIsKept();
    TargetHoldSetIsBounded();
    TargetNoteWithoutATargetHoldsNothing();
    std::cout << (g_Failures ? "RESULT FAIL " + std::to_string(g_Failures) : std::string("RESULT OK")) << "\n";
    return g_Failures ? 1 : 0;
}
