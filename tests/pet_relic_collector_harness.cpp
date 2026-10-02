// Behavioral harness for Pet Collects Relics' target choice (ForgePact issue
// #124: the pet fetches relics on screen, and never one the player already
// owns at 10/10).
//
// The Python runner splices the REAL plugin/include/ForgePact/PetQuestCollectorMod.hpp
// in at PRODUCTION_PETQUEST and plugin/include/ForgePact/PetRelicCollectorMod.hpp
// at PRODUCTION_PETRELIC (their #pragma and #include lines removed). The relic
// tick reuses the quest collector's PetQuestSelector for the pick, the hold-back
// and the family cursor; what is new is game-independent by contract - relic
// ids, a maxed set, squared distances and frame numbers, never an instance - so
// both headers compile here with no runtime stub. The only name they need from
// Common.hpp is Out(), defined below.
//
// The baseline is the quest selector's nearest rule applied to relics with no
// maxed filter: with a 10/10 relic nearer than a lower one it walks to the
// 10/10 relic, which the game will not let the pet pick up (an equipped copy at
// 10/10 returns false from PickupRelic), so the pet is stuck on it - #94's
// shape. The targets pin the maxed rule (the lower relic is taken, a screen of
// only maxed relics picks none, a relic that reaches 10 on a collect is skipped
// once the maxed set is refreshed), the arbiter that keeps the two collectors
// from walking one pet two ways, and a failed relic held back through the
// selector.
//
// Red first: with these scenarios written and the relic header not yet
// written (an empty file at the path), the first error was
// `error C2039: 'FilterRelicCandidates': is not a member of 'ForgePact'` (the
// first of the using-declarations below), then the same for
// `PetFetchArbiter` and `PetFetcher`.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

static std::vector<std::string> g_Logged;
static void Out(const std::string& s) { g_Logged.push_back(s); }

// PRODUCTION_PETQUEST

// PRODUCTION_PETRELIC

using ForgePact::FilterRelicCandidates;
using ForgePact::PetFetchArbiter;
using ForgePact::PetFetcher;
using ForgePact::PetQuestCandidate;
using ForgePact::PetQuestOutcome;
using ForgePact::PetQuestSelector;
using ForgePact::PetRelicCandidate;
using ForgePact::PetRelicCollectorMod;
using ForgePact::PetRelicMaxedCache;
using ForgePact::kPetRelicMaxedRefreshTicks;

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

// Instance ids are GameMaker ids, relic ids are the definition's `b`,
// distances are squared pixels as the tick computes them. The maxed relic
// (id 15) lies nearer the pet than the lower one (id 42).
static constexpr double kNearMaxed = 200001.0, kFarLower = 200002.0, kOther = 200003.0;
static constexpr int kMaxedRelic = 15, kLowerRelic = 42, kOtherRelic = 7;

static std::vector<PetRelicCandidate> MaxedAndLower()
{
    return { { kFarLower, 900.0, kLowerRelic }, { kNearMaxed, 100.0, kMaxedRelic } };
}

// The relic candidates as the quest selector would have them with no maxed
// filter: every relic on screen, maxed or not.
static std::vector<PetQuestCandidate> Unfiltered(const std::vector<PetRelicCandidate>& relics)
{
    std::vector<PetQuestCandidate> out;
    for (const PetRelicCandidate& r : relics) out.push_back({ r.id, r.distance2 });
    return out;
}

// ---- baseline ---------------------------------------------------------------

static void BaselineNearestRulePicksTheMaxedRelic()
{
    // The reference: the nearest rule with no maxed filter walks to the 10/10
    // relic, and after the game refuses it, is held back and the hold expires,
    // it walks to it again - the stuck-on-an-uncollectable-item loop the owner
    // ruled out.
    PetQuestSelector s;
    const auto first = s.Pick(Unfiltered(MaxedAndLower()), 10);
    s.Note(PetQuestOutcome::Refused, 20);
    const auto afterHold = s.Pick(Unfiltered({ { kNearMaxed, 100.0, kMaxedRelic } }),
                                  20 + ForgePact::kPetQuestHoldFrames);
    Check("baseline/nearest_rule_picks_the_maxed_relic",
          first && *first == kNearMaxed && afterHold && *afterHold == kNearMaxed,
          "first=" + Id(first) + " afterHold=" + Id(afterHold));
}

// ---- target -----------------------------------------------------------------

static void TargetMaxedRelicNeverPicked()
{
    // A 10/10 relic nearer the pet and a lower relic farther away: the filter
    // leaves the 10/10 one out before Pick, so the pet takes the lower one,
    // and the dropped candidate is counted.
    PetQuestSelector s;
    long dropped = 0;
    const std::unordered_set<int> maxed{ kMaxedRelic };
    const auto kept = FilterRelicCandidates(MaxedAndLower(), maxed, dropped);
    const auto pick = s.Pick(kept, 10);
    bool maxedKept = false;
    for (const PetQuestCandidate& c : kept) maxedKept = maxedKept || c.id == kNearMaxed;
    // Negative control: with an empty maxed set nothing is dropped and the
    // nearer relic is the pick again, so the filter is what made the change.
    PetQuestSelector control;
    long controlDropped = 0;
    const auto controlPick = control.Pick(FilterRelicCandidates(MaxedAndLower(), {}, controlDropped), 10);
    Check("target/maxed_relic_never_picked",
          pick && *pick == kFarLower && !maxedKept && dropped == 1
              && controlPick && *controlPick == kNearMaxed && controlDropped == 0,
          "pick=" + Id(pick) + " dropped=" + std::to_string(dropped) + " control=" + Id(controlPick));
}

static void TargetOnlyMaxedPicksNone()
{
    // A screen holding only 10/10 relics: Pick gets an empty list, returns
    // none and opens no travel, tick after tick - idle, not oscillating.
    PetQuestSelector s;
    const std::unordered_set<int> maxed{ kMaxedRelic, kOtherRelic };
    const std::vector<PetRelicCandidate> onlyMaxed{ { kNearMaxed, 100.0, kMaxedRelic }, { kOther, 400.0, kOtherRelic } };
    bool anyPick = false;
    long dropped = 0;
    for (int64_t frame = 1; frame <= 5; ++frame)
        anyPick = anyPick || s.Pick(FilterRelicCandidates(onlyMaxed, maxed, dropped), frame).has_value();
    Check("target/only_maxed_picks_none", !anyPick && !s.Travelling() && dropped == 10 && s.HeldBack() == 0,
          "dropped=" + std::to_string(dropped));
}

static void TargetNewlyMaxedRelicIsSkippedAfterRefresh()
{
    // The pet collects relic 42 at 9/10; the pickup raises it to 10/10. The
    // collect marks the cached maxed set stale, so the next tick re-reads it
    // and a second relic 42 on the ground is left alone.
    PetRelicMaxedCache cache;
    const bool dueAtStart = cache.Due(0);
    cache.Store({ kMaxedRelic }, 0);
    const bool dueSoonAfter = cache.Due(5);
    // Without the refresh-on-collect flag the set would stay stale until the
    // 60-tick refresh: relic 42 would still be picked at frame 10.
    PetQuestSelector stale;
    long d0 = 0;
    const auto stalePick = stale.Pick(FilterRelicCandidates({ { kOther, 100.0, kLowerRelic } }, cache.Ids(), d0), 10);
    cache.MarkStale();                    // the collect returned true
    const bool dueAfterCollect = cache.Due(10);
    cache.Store({ kMaxedRelic, kLowerRelic }, 10);   // the re-read finds 42 at 10/10
    PetQuestSelector s;
    long dropped = 0;
    const auto pick = s.Pick(FilterRelicCandidates({ { kOther, 100.0, kLowerRelic } }, cache.Ids(), dropped), 11);
    // The periodic refresh: due again kPetRelicMaxedRefreshTicks after a store.
    const bool periodic = !cache.Due(10 + kPetRelicMaxedRefreshTicks - 1) && cache.Due(10 + kPetRelicMaxedRefreshTicks);
    Check("target/newly_maxed_relic_is_skipped_after_refresh",
          dueAtStart && !dueSoonAfter && stalePick && *stalePick == kOther && dueAfterCollect
              && !pick && dropped == 1 && periodic && cache.Scans() == 2 && kPetRelicMaxedRefreshTicks == 60,
          "stalePick=" + Id(stalePick) + " pick=" + Id(pick) + " scans=" + std::to_string(cache.Scans()));
}

static void TargetQuestTravelBlocksARelicPick()
{
    // One pet, one fetch: while the quest collector holds the travel the
    // relic tick picks nothing; once the quest travel ends, it may.
    PetFetchArbiter a;
    const bool questClaimed = a.Claim(PetFetcher::Quest);
    const bool relicMay = a.MayPick(PetFetcher::Relic);
    const bool relicClaim = a.Claim(PetFetcher::Relic);
    const bool questMay = a.MayPick(PetFetcher::Quest);   // the holder keeps its travel
    a.Release(PetFetcher::Relic);                          // a release by a non-holder does nothing
    const bool stillQuest = a.Holder() == PetFetcher::Quest;
    a.Release(PetFetcher::Quest);
    const bool relicAfter = a.MayPick(PetFetcher::Relic) && a.Claim(PetFetcher::Relic);
    Check("target/quest_travel_blocks_a_relic_pick",
          questClaimed && !relicMay && !relicClaim && questMay && stillQuest && relicAfter
              && a.Holder() == PetFetcher::Relic,
          "relicMay=" + std::to_string(relicMay) + " relicAfter=" + std::to_string(relicAfter));
}

static void TargetRelicTravelBlocksAQuestPick()
{
    PetFetchArbiter a;
    const bool relicClaimed = a.Claim(PetFetcher::Relic);
    const bool questMay = a.MayPick(PetFetcher::Quest);
    const bool questClaim = a.Claim(PetFetcher::Quest);
    a.Release(PetFetcher::Relic);
    const bool questAfter = a.MayPick(PetFetcher::Quest) && a.Claim(PetFetcher::Quest);
    Check("target/relic_travel_blocks_a_quest_pick",
          relicClaimed && !questMay && !questClaim && questAfter && a.Holder() == PetFetcher::Quest,
          "questMay=" + std::to_string(questMay));
}

static void TargetFailedRelicIsHeldBack()
{
    // A collect the game refused, or one that left the relic in place, holds
    // that relic back through the selector: the next pick is the other relic,
    // not the same one again.
    bool ok = true;
    std::string detail;
    const std::vector<PetRelicCandidate> two{ { kFarLower, 100.0, kLowerRelic }, { kOther, 400.0, kOtherRelic } };
    for (PetQuestOutcome o : { PetQuestOutcome::Refused, PetQuestOutcome::NoEffect, PetQuestOutcome::Gate, PetQuestOutcome::Timeout }) {
        PetQuestSelector s;
        long dropped = 0;
        const auto p1 = s.Pick(FilterRelicCandidates(two, {}, dropped), 100);
        const bool held = s.Note(o, 110);
        const auto p2 = s.Pick(FilterRelicCandidates(two, {}, dropped), 111);
        const bool thisOk = p1 && *p1 == kFarLower && held && p2 && *p2 == kOther && s.HeldBack() == 1;
        if (!thisOk) detail += " outcome=" + std::to_string((int)o) + " p2=" + Id(p2);
        ok = ok && thisOk;
    }
    // Negative control: a collect that removed the relic holds nothing.
    PetQuestSelector c;
    long dropped = 0;
    c.Pick(FilterRelicCandidates(two, {}, dropped), 100);
    ok = ok && !c.Note(PetQuestOutcome::Collected, 110) && c.HeldBack() == 0;
    Check("target/failed_relic_is_held_back", ok, detail);
}

static void TargetSwitchLineNamesTheMod()
{
    // `petrelic 1` / `petrelic 0` print the switch line the panel's toast and
    // the live checks read.
    g_Logged.clear();
    PetRelicCollectorMod::Instance().SetEnabled(true);
    const bool on = PetRelicCollectorMod::Instance().IsEnabled();
    PetRelicCollectorMod::Instance().SetEnabled(false);
    const bool off = !PetRelicCollectorMod::Instance().IsEnabled();
    const bool lines = g_Logged.size() == 2 && g_Logged[0].rfind("petrelic -> ON (", 0) == 0 && g_Logged[1] == "petrelic -> OFF";
    Check("target/switch_line_names_the_mod", on && off && lines,
          g_Logged.empty() ? std::string("no line") : g_Logged[0]);
}

int main()
{
    BaselineNearestRulePicksTheMaxedRelic();
    TargetMaxedRelicNeverPicked();
    TargetOnlyMaxedPicksNone();
    TargetNewlyMaxedRelicIsSkippedAfterRefresh();
    TargetQuestTravelBlocksARelicPick();
    TargetRelicTravelBlocksAQuestPick();
    TargetFailedRelicIsHeldBack();
    TargetSwitchLineNamesTheMod();
    std::cout << (g_Failures ? "RESULT FAIL " + std::to_string(g_Failures) : std::string("RESULT OK")) << "\n";
    return g_Failures ? 1 : 0;
}
