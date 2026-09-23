// Behavioral harness for the crafting-materials core (ForgePact issue #14).
//
// The Python runner splices the REAL plugin/include/ForgePact/CraftMatsMod.hpp
// in at the marker below (its #pragma and #include lines removed). The header is
// game-independent by contract - it names no runtime interface - so it compiles
// here with no runtime stub.
//
// The game mechanism is not measured yet (docs/crafting-materials-research.md,
// Phase 1 pending), so these scenarios drive only the arithmetic every
// hypothesis shares: per material, need N, bag count k, stash-tab count s ->
// take min(N-k, s) from the stash's material tab only when the switch is on
// and k < N; an unreadable count refuses; a loss signal turns the mod off for
// the session. Whichever mechanism the owner picks, its adapter feeds Plan()
// the counts it re-read at the press and reports each take with OnTakeReport().
//
// Baseline: off by default; off plans nothing whatever it is fed; on, a bag
// that covers the need plans nothing. Target: the deficit and only the deficit
// comes from the stash tab, never more than it holds, refusals and losses are
// named once, and the lines name what the mod did.
//
// Phase 1e adds the kept stash map's currency rule (CraftMatsKeptMap). Baseline:
// nothing kept is not current, and an index still held after a character load
// or a room change is not current either. Target: the game's own refresh makes
// it current, again after an invalidation (same index or a new one), and a
// clear is not current.
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// PRODUCTION_CRAFTMATS

using ForgePact::CraftMatsMod;
using ForgePact::CraftMatsNeed;
using ForgePact::CraftMatsOutcome;
using ForgePact::CraftMatsPlan;
using ForgePact::CraftMatsRefusal;
using ForgePact::CraftMatsSource;
using ForgePact::CraftMatsTakeReport;
using ForgePact::CraftMatsKeptMap;
using ForgePact::CraftMatsMapReason;

static int g_Failures = 0;

static void Check(const std::string& label, bool ok, const std::string& detail)
{
    if (ok) std::cout << "PASS " << label << "\n";
    else { std::cout << "FAIL " << label << " " << detail << "\n"; ++g_Failures; }
}

static std::string N(long long v) { return std::to_string(v); }

static CraftMatsNeed Need(const std::string& material, int64_t need, int64_t bag, int64_t stash)
{
    CraftMatsNeed n;
    n.material = material;
    n.need = need;
    n.bag = bag;
    n.stash = stash;
    return n;
}

static CraftMatsTakeReport Report(const std::string& material, int64_t asked, bool success, int64_t before, int64_t after)
{
    CraftMatsTakeReport r;
    r.material = material;
    r.asked = asked;
    r.success = success;
    r.stashBefore = before;
    r.stashAfter = after;
    return r;
}

static std::string Describe(const CraftMatsPlan& p)
{
    std::string s = "refused=" + N(p.refused) + " reason=" + CraftMatsMod::RefusalName(p.reason) + " takes=[";
    for (size_t i = 0; i < p.takes.size(); ++i)
        s += (i ? "," : "") + p.takes[i].material + ":" + N(p.takes[i].amount) + "@" + N((int)p.takes[i].from);
    return s + "]";
}

static bool Empty(const CraftMatsPlan& p) { return !p.refused && p.takes.empty(); }

// ---- baseline: off is vanilla -------------------------------------------------

static void BaselineOffByDefault()
{
    CraftMatsMod mod;
    const std::string line = mod.StatLine();
    Check("baseline/off_by_default", !mod.IsEnabled() && !mod.OffThisSession() && line.rfind("craftmats: off", 0) == 0,
          "enabled=" + N(mod.IsEnabled()) + " line=\"" + line + "\"");
}

static void BaselineOffNeverPlansATake()
{
    CraftMatsMod mod;
    // Everything that would make the mod act when on: a deficit the stash tab
    // covers, a deficit it half covers, an unreadable count, a duplicate row.
    const CraftMatsPlan a = mod.Plan({ Need("ore", 5, 0, 10) });
    const CraftMatsPlan b = mod.Plan({ Need("ore", 5, 2, 1), Need("gem", 3, 0, 3) });
    const CraftMatsPlan c = mod.Plan({ Need("ore", 5, -1, -1) });
    const CraftMatsPlan d = mod.Plan({ Need("ore", 2, 0, 9), Need("ore", 2, 0, 9) });
    Check("baseline/off_never_plans_a_take",
          Empty(a) && Empty(b) && Empty(c) && Empty(d) && mod.Plans() == 0 && mod.WhileOff() == 4
              && mod.TakeFirstRefusal() == CraftMatsRefusal::None && mod.Refused(CraftMatsRefusal::Unreadable) == 0,
          Describe(a) + " " + Describe(b) + " " + Describe(c) + " " + Describe(d) + " plans=" + N(mod.Plans())
              + " while-off=" + N(mod.WhileOff()));
}

static void BaselineBagCoversNeedTakesNothing()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsPlan exact = mod.Plan({ Need("ore", 5, 5, 10) });
    const CraftMatsPlan more = mod.Plan({ Need("ore", 5, 40, 10), Need("gem", 1, 1, 0) });
    // Negative control for the unreadable rule: a stash tab that cannot be read
    // does not matter when the bag already covers the need.
    const CraftMatsPlan stashUnread = mod.Plan({ Need("ore", 5, 6, -1) });
    // A zero or negative need is not a need.
    const CraftMatsPlan noNeed = mod.Plan({ Need("ore", 0, 0, 10), Need("gem", -3, 0, 10) });
    Check("baseline/bag_covers_need_takes_nothing",
          Empty(exact) && Empty(more) && Empty(stashUnread) && Empty(noNeed)
              && mod.TakeFirstRefusal() == CraftMatsRefusal::None && mod.PlansWithTakes() == 0,
          Describe(exact) + " " + Describe(more) + " " + Describe(stashUnread) + " " + Describe(noNeed));
}

// ---- target: the mod on --------------------------------------------------------

static void TargetDeficitIsTakenFromTheStashTabOnly()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    // ore is short by 3 and the stash tab holds 10; gem is covered by the bag.
    const CraftMatsPlan p = mod.Plan({ Need("ore", 5, 2, 10), Need("gem", 2, 4, 50) });
    bool ok = !p.refused && p.takes.size() == 1 && p.takes[0].material == "ore" && p.takes[0].amount == 3;
    for (const auto& t : p.takes) ok = ok && t.from == CraftMatsSource::StashMaterialTab;
    // Bag first, then stash: an empty bag takes the whole need from the tab.
    const CraftMatsPlan all = mod.Plan({ Need("dust", 4, 0, 4) });
    ok = ok && all.takes.size() == 1 && all.takes[0].amount == 4;
    Check("target/deficit_is_taken_from_the_stash_tab_only", ok && mod.PlansWithTakes() == 2,
          Describe(p) + " " + Describe(all) + " with-takes=" + N(mod.PlansWithTakes()));
}

static void TargetNeverTakesMoreThanTheStashHolds()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsPlan part = mod.Plan({ Need("ore", 5, 1, 2) });   // short by 4, the tab holds 2
    const CraftMatsPlan none = mod.Plan({ Need("ore", 5, 1, 0) });   // the tab holds nothing
    const CraftMatsPlan zero = mod.Plan({ Need("ore", 5, 0, 0) });
    const bool ok = part.takes.size() == 1 && part.takes[0].amount == 2 && Empty(none) && Empty(zero);
    Check("target/never_takes_more_than_the_stash_holds", ok,
          Describe(part) + " " + Describe(none) + " " + Describe(zero));
}

static void TargetDuplicateRowsOfOneMaterialAreSummed()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsPlan p = mod.Plan({ Need("ore", 2, 1, 10), Need("ore", 3, 1, 10) });
    Check("target/duplicate_rows_of_one_material_are_summed",
          !p.refused && p.takes.size() == 1 && p.takes[0].amount == 4, Describe(p));
}

static void TargetDisagreeingReadsOfOneMaterialRefuse()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    // Two reads of one material in one press that disagree cannot both be true.
    const CraftMatsPlan p = mod.Plan({ Need("ore", 2, 1, 10), Need("ore", 3, 2, 10) });
    Check("target/disagreeing_reads_of_one_material_refuse",
          p.refused && p.reason == CraftMatsRefusal::Unreadable && p.takes.empty(), Describe(p));
}

static void TargetUnreadableSourceRefusesAndIsNamedOnce()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsPlan bagUnread = mod.Plan({ Need("ore", 5, -1, 10) });
    const CraftMatsRefusal first = mod.TakeFirstRefusal();
    const std::string line = mod.RefusalLine(first);
    // The stash tab unreadable with a real deficit refuses too - and the other
    // material, readable and short, is not taken either: a partial plan is not
    // made from a press the core could not fully read.
    const CraftMatsPlan stashUnread = mod.Plan({ Need("gem", 1, 0, 5), Need("ore", 5, 2, -1) });
    const CraftMatsRefusal second = mod.TakeFirstRefusal();
    Check("target/unreadable_source_refuses_and_is_named_once",
          bagUnread.refused && bagUnread.takes.empty() && bagUnread.reason == CraftMatsRefusal::Unreadable
              && stashUnread.refused && stashUnread.takes.empty()
              && first == CraftMatsRefusal::Unreadable && second == CraftMatsRefusal::None
              && mod.Refused(CraftMatsRefusal::Unreadable) == 2 && mod.IsEnabled()
              && line.rfind("craftmats: unreadable - ", 0) == 0 && line.find("nothing was taken") != std::string::npos,
          Describe(bagUnread) + " " + Describe(stashUnread) + " first=" + CraftMatsMod::RefusalName(first)
              + " second=" + CraftMatsMod::RefusalName(second) + " line=\"" + line + "\"");
}

static void TargetConfirmedTakeCountsAndIsReportedOnce()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsOutcome o = mod.OnTakeReport(Report("ore", 3, true, 10, 7));
    const bool firstDue = mod.TakeFirstTake();
    const std::string line = mod.FirstTakeLine();
    const CraftMatsOutcome o2 = mod.OnTakeReport(Report("gem", 1, true, 1, 0));
    const bool againDue = mod.TakeFirstTake();
    Check("target/confirmed_take_counts_and_is_reported_once",
          o == CraftMatsOutcome::Taken && o2 == CraftMatsOutcome::Taken && firstDue && !againDue
              && mod.Taken() == 2 && mod.TakenUnits() == 4 && mod.IsEnabled()
              && line.rfind("craftmats: first take from the stash tab - ", 0) == 0 && line.find("taken=1") != std::string::npos,
          "taken=" + N(mod.Taken()) + " units=" + N(mod.TakenUnits()) + " first=" + N(firstDue) + " again=" + N(againDue)
              + " line=\"" + line + "\"");
}

static void TargetNotTakenLeavesTheModOnAndIsNamedOnce()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsOutcome a = mod.OnTakeReport(Report("ore", 3, false, 10, 10));
    const CraftMatsRefusal first = mod.TakeFirstRefusal();
    const CraftMatsOutcome b = mod.OnTakeReport(Report("ore", 3, false, 10, 10));
    const CraftMatsRefusal second = mod.TakeFirstRefusal();
    Check("target/not_taken_leaves_the_mod_on_and_is_named_once",
          a == CraftMatsOutcome::NotTaken && b == CraftMatsOutcome::NotTaken && mod.IsEnabled() && !mod.OffThisSession()
              && first == CraftMatsRefusal::NotTaken && second == CraftMatsRefusal::None
              && mod.Refused(CraftMatsRefusal::NotTaken) == 2 && mod.Taken() == 0,
          "first=" + std::string(CraftMatsMod::RefusalName(first)) + " second=" + CraftMatsMod::RefusalName(second)
              + " enabled=" + N(mod.IsEnabled()));
}

static void TargetLossSignalTurnsTheModOffForTheSession()
{
    // (1) The stash tab shrank without the game's success answer.
    CraftMatsMod shrank;
    shrank.SetEnabled(true);
    const CraftMatsOutcome a = shrank.OnTakeReport(Report("ore", 3, false, 10, 7));
    const bool lossDue = shrank.TakeFirstLoss();
    const std::string lossLine = shrank.LossLine();
    const bool lossAgain = shrank.TakeFirstLoss();
    const bool reArm = shrank.SetEnabled(true);
    const CraftMatsPlan after = shrank.Plan({ Need("ore", 5, 0, 10) });
    const bool shrankOk = a == CraftMatsOutcome::Loss && !shrank.IsEnabled() && shrank.OffThisSession() && lossDue && !lossAgain
        && !reArm && !shrank.IsEnabled() && Empty(after) && shrank.Losses() == 1
        && lossLine.rfind("craftmats: off for this session - ", 0) == 0;
    // (2) Success, but the re-read cannot be made.
    CraftMatsMod unread;
    unread.SetEnabled(true);
    const CraftMatsOutcome b = unread.OnTakeReport(Report("ore", 3, true, 10, -1));
    // (3) Success, but the re-read disagrees with what was asked.
    CraftMatsMod mismatch;
    mismatch.SetEnabled(true);
    const CraftMatsOutcome c = mismatch.OnTakeReport(Report("ore", 3, true, 10, 8));
    // (4) No success and no re-read: whether it shrank cannot be told.
    CraftMatsMod blind;
    blind.SetEnabled(true);
    const CraftMatsOutcome d = blind.OnTakeReport(Report("ore", 3, false, 10, -1));
    // (5) A before-read that was never made cannot confirm anything either.
    CraftMatsMod noBefore;
    noBefore.SetEnabled(true);
    const CraftMatsOutcome e = noBefore.OnTakeReport(Report("ore", 3, true, -1, 7));
    // Turning the switch off and on by hand does not clear it; off stays allowed.
    const bool offAllowed = shrank.SetEnabled(false);
    Check("target/loss_signal_turns_the_mod_off_for_the_session",
          shrankOk && b == CraftMatsOutcome::Loss && !unread.IsEnabled() && unread.OffThisSession()
              && c == CraftMatsOutcome::Loss && mismatch.OffThisSession()
              && d == CraftMatsOutcome::Loss && blind.OffThisSession()
              && e == CraftMatsOutcome::Loss && noBefore.OffThisSession() && offAllowed && shrank.OffThisSession(),
          "a=" + N((int)a) + " b=" + N((int)b) + " c=" + N((int)c) + " d=" + N((int)d) + " e=" + N((int)e)
              + " reArm=" + N(reArm) + " line=\"" + lossLine + "\" " + Describe(after));
}

static void TargetReportWhileOffIsIgnored()
{
    // The adapter never takes while off; a stray report changes nothing.
    CraftMatsMod mod;
    const CraftMatsOutcome o = mod.OnTakeReport(Report("ore", 3, false, 10, 7));
    Check("target/report_while_off_is_ignored",
          o == CraftMatsOutcome::NotTaken && !mod.OffThisSession() && mod.Losses() == 0 && mod.Taken() == 0,
          "outcome=" + N((int)o) + " off-this-session=" + N(mod.OffThisSession()));
}

static void TargetStatLineNamesWhatItDid()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    mod.Plan({ Need("ore", 5, 2, 10) });
    mod.OnTakeReport(Report("ore", 3, true, 10, 7));
    mod.Plan({ Need("ore", 5, -1, 10) });
    const std::string on = mod.StatLine();
    mod.OnTakeReport(Report("ore", 1, false, 7, 6));
    const std::string off = mod.StatLine();
    const bool ok = on.rfind("craftmats: ON ", 0) == 0 && on.find("plans=2") != std::string::npos
        && on.find("with-takes=1") != std::string::npos && on.find("taken=1 (units=3)") != std::string::npos
        && on.find("unreadable=1") != std::string::npos && on.find("losses=0") != std::string::npos
        && on.find("session=ok") != std::string::npos
        && off.rfind("craftmats: off ", 0) == 0 && off.find("losses=1") != std::string::npos
        && off.find("session=off") != std::string::npos;
    Check("target/statline_names_what_it_did", ok, "on=\"" + on + "\" off=\"" + off + "\"");
}

// ---- the kept stash map's currency rule (Phase 1e) ------------------------------
//
// The research build keeps the ds_map the game's own GetItemMap(9) call returned.
// GameMaker reuses a destroyed map's index, so "the index still exists" says
// nothing about whether it is still the stash's map: the kept map is current
// only when a refresh (the game's own call returned it) came after the latest
// invalidation (a character load or a room change). ds_exists is the plugin's
// half and is never an input here.

static std::string Describe(const CraftMatsKeptMap& m)
{
    return "current=" + N(m.IsCurrent()) + " kept=" + N(m.IsKept()) + " index=" + N(m.Index())
        + " reason=" + CraftMatsKeptMap::ReasonName(m.Reason()) + " refreshed=" + N(m.Refreshes());
}

static void BaselineKeptMapNothingKeptIsNotCurrent()
{
    CraftMatsKeptMap fresh;
    const bool freshOk = !fresh.IsCurrent() && !fresh.IsKept() && fresh.Reason() == CraftMatsMapReason::NotKept;
    // An invalidation with nothing kept does not make anything current either,
    // and "not kept" stays the reason.
    CraftMatsKeptMap loaded;
    loaded.Invalidate(CraftMatsMapReason::CharacterLoaded);
    const bool loadedOk = !loaded.IsCurrent() && !loaded.IsKept() && loaded.Reason() == CraftMatsMapReason::NotKept;
    Check("baseline/kept_map_nothing_kept_is_not_current", freshOk && loadedOk,
          Describe(fresh) + " | " + Describe(loaded));
}

static void BaselineKeptMapReusedIndexIsNotCurrentAfterAnInvalidation()
{
    // The reused-index case: the index is still held (and ds_exists would still
    // answer true for it), but nothing has refreshed it since the character
    // load or the room change, so it is not current.
    CraftMatsKeptMap loaded;
    loaded.Refreshed(1049);
    loaded.Invalidate(CraftMatsMapReason::CharacterLoaded);
    CraftMatsKeptMap moved;
    moved.Refreshed(1049);
    moved.Invalidate(CraftMatsMapReason::RoomChanged);
    // A second invalidation keeps it stale; asking again changes nothing.
    moved.Invalidate(CraftMatsMapReason::RoomChanged);
    const bool ok = !loaded.IsCurrent() && loaded.IsKept() && loaded.Index() == 1049
        && loaded.Reason() == CraftMatsMapReason::CharacterLoaded
        && !moved.IsCurrent() && moved.IsKept() && moved.Index() == 1049
        && moved.Reason() == CraftMatsMapReason::RoomChanged && !moved.IsCurrent();
    Check("baseline/kept_map_reused_index_is_not_current_after_an_invalidation", ok,
          Describe(loaded) + " | " + Describe(moved));
}

static void TargetKeptMapRefreshMakesItCurrent()
{
    CraftMatsKeptMap m;
    m.Refreshed(1049);
    Check("target/kept_map_refresh_makes_it_current",
          m.IsCurrent() && m.IsKept() && m.Index() == 1049 && m.Reason() == CraftMatsMapReason::None
              && m.Refreshes() == 1 && std::string(CraftMatsKeptMap::ReasonName(m.Reason())) == "none",
          Describe(m));
}

static void TargetKeptMapRefreshAfterAnInvalidationIsCurrentAgain()
{
    // The same index returned again by the game's own call after the room change.
    CraftMatsKeptMap same;
    same.Refreshed(1049);
    same.Invalidate(CraftMatsMapReason::RoomChanged);
    same.Refreshed(1049);
    // A new index after a character load: current, with the new one.
    CraftMatsKeptMap renewed;
    renewed.Refreshed(1049);
    renewed.Invalidate(CraftMatsMapReason::CharacterLoaded);
    renewed.Refreshed(1050);
    const bool ok = same.IsCurrent() && same.Index() == 1049 && same.Refreshes() == 2
        && renewed.IsCurrent() && renewed.Index() == 1050 && renewed.Reason() == CraftMatsMapReason::None;
    Check("target/kept_map_refresh_after_an_invalidation_is_current_again", ok,
          Describe(same) + " | " + Describe(renewed));
}

static void TargetKeptMapClearIsNotCurrent()
{
    CraftMatsKeptMap m;
    m.Refreshed(1049);
    m.Clear();
    const std::string cleared = Describe(m);
    const bool clearedOk = !m.IsCurrent() && !m.IsKept() && m.Reason() == CraftMatsMapReason::NotKept
        && std::string(CraftMatsKeptMap::ReasonName(m.Reason())) == "not-kept";
    // Keeping starts again from the game's next call.
    m.Refreshed(1051);
    Check("target/kept_map_clear_is_not_current", clearedOk && m.IsCurrent() && m.Index() == 1051,
          cleared + " | " + Describe(m));
}

static void TargetKeptMapReasonNamesAreTheStatTokens()
{
    const bool ok = std::string(CraftMatsKeptMap::ReasonName(CraftMatsMapReason::None)) == "none"
        && std::string(CraftMatsKeptMap::ReasonName(CraftMatsMapReason::NotKept)) == "not-kept"
        && std::string(CraftMatsKeptMap::ReasonName(CraftMatsMapReason::CharacterLoaded)) == "character-loaded"
        && std::string(CraftMatsKeptMap::ReasonName(CraftMatsMapReason::RoomChanged)) == "room-changed";
    Check("target/kept_map_reason_names_are_the_stat_tokens", ok, "");
}

int main()
{
    BaselineOffByDefault();
    BaselineOffNeverPlansATake();
    BaselineBagCoversNeedTakesNothing();
    TargetDeficitIsTakenFromTheStashTabOnly();
    TargetNeverTakesMoreThanTheStashHolds();
    TargetDuplicateRowsOfOneMaterialAreSummed();
    TargetDisagreeingReadsOfOneMaterialRefuse();
    TargetUnreadableSourceRefusesAndIsNamedOnce();
    TargetConfirmedTakeCountsAndIsReportedOnce();
    TargetNotTakenLeavesTheModOnAndIsNamedOnce();
    TargetLossSignalTurnsTheModOffForTheSession();
    TargetReportWhileOffIsIgnored();
    TargetStatLineNamesWhatItDid();
    BaselineKeptMapNothingKeptIsNotCurrent();
    BaselineKeptMapReusedIndexIsNotCurrentAfterAnInvalidation();
    TargetKeptMapRefreshMakesItCurrent();
    TargetKeptMapRefreshAfterAnInvalidationIsCurrentAgain();
    TargetKeptMapClearIsNotCurrent();
    TargetKeptMapReasonNamesAreTheStatTokens();
    std::cout << (g_Failures ? "RESULT FAIL " + std::to_string(g_Failures) : std::string("RESULT OK")) << "\n";
    return g_Failures ? 1 : 0;
}
