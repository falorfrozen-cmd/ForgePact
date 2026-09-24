// Behavioral harness for the crafting-materials core (ForgePact issue #14).
//
// The Python runner splices the REAL plugin/include/ForgePact/CraftMatsMod.hpp
// in at the marker below (its #pragma and #include lines removed). The header is
// game-independent by contract - it names no runtime interface - so it compiles
// here with no runtime stub.
//
// Phase 0's scenarios drive the arithmetic every hypothesis shared: per
// material, need N, bag count k, stash-tab count s ->
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
//
// The player build (2026-09-24, the Phase C build) adds the decisions its
// adapter acts on: the count, the per-frame walk, the needs pairing, the press
// gate, the split across entries, the move outcome, the consume check and the
// save. Baseline: off, or outside the crafting route, the count is the game's
// own; off, the press is the game's own. Target: the rest, below. The source
// enum widens to the two special tabs (Socketable joins Materials); no Phase 0
// or 1e scenario's rule is replaced, so none is renamed - the refusal lines
// keep their `craftmats: <reason> - ` heads and now say the craft was refused.
//
// Red first: with the new scenarios written and the core not yet extended, this
// file did not compile. The first error was the using-declarations' `error
// C2039: 'CraftMatsDestination': is not a member of 'ForgePact'`, and the first
// line naming each target's missing decision was: count_* `'CountAnswer': is
// not a member of 'ForgePact::CraftMatsMod'` (and `'StashSocketTab': illegal
// qualified name`); the walk, needs, split, move, press, consume and save
// targets used `MustWalk`, `BeginFind`/`Needs`, `Split`, `OnMoveReport`,
// `MayCraft`/`PressStep`, `OnConsume` and `SaveDue`/`PressLine`, beyond the
// compiler's 100-error cap, so their own lines were not printed.
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
using ForgePact::CraftMatsDestination;
using ForgePact::CraftMatsEntry;
using ForgePact::CraftMatsEntryTake;
using ForgePact::CraftMatsMoveReport;
using ForgePact::CraftMatsMoved;
using ForgePact::CraftMatsNeeds;
using ForgePact::CraftMatsPressStep;
using ForgePact::CraftMatsSave;
using ForgePact::CraftMatsWalk;

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

// ---- the player build: the count, the needs, the press (Phase C build) --------
//
// The adapter hooks CountInventoryItem, PilipaliDecrypt and the crafting route's
// scripts; every decision it acts on is made here. Materials are named the way
// the per-press line names them, "class=<c> b=<b>".

static CraftMatsEntry Entry(const std::string& material, const std::string& key, int64_t count, CraftMatsSource from, int row)
{
    CraftMatsEntry e;
    e.material = material;
    e.key = key;
    e.count = count;
    e.from = from;
    e.row = row;
    return e;
}

// The Socketable tab holds Ol (class 15, b 1) in two entries, the Materials tab
// Greater Unstable Dust (class 14, b 51) in one.
static CraftMatsWalk SampleWalk()
{
    CraftMatsWalk w;
    w.readable = true;
    w.entries = {
        Entry(CraftMatsMod::Material(15, 1), "0-0-11-15", 200, CraftMatsSource::StashSocketTab, 73),
        Entry(CraftMatsMod::Material(15, 1), "0-0-12-15", 16, CraftMatsSource::StashSocketTab, 74),
        Entry(CraftMatsMod::Material(14, 51), "0-0-13-14", 153, CraftMatsSource::StashMaterialTab, -1),
    };
    return w;
}

static CraftMatsMoveReport Move(int64_t asked, int64_t srcBefore, int64_t srcAfter, bool entryGone, bool cellGone,
                                int64_t dstBefore, int64_t dstAfter)
{
    CraftMatsMoveReport r;
    r.asked = asked;
    r.sourceBefore = srcBefore;
    r.sourceAfter = srcAfter;
    r.sourceEntryGone = entryGone;
    r.sourceCellGone = cellGone;
    r.destBefore = dstBefore;
    r.destAfter = dstAfter;
    return r;
}

static std::string Describe(const CraftMatsNeeds& n)
{
    std::string s = "readable=" + N(n.readable) + " stash-added=" + N(n.stashAdded) + " self=" + N(n.self) + " rows=[";
    for (size_t i = 0; i < n.rows.size(); ++i)
        s += (i ? "," : "") + n.rows[i].material + ":need=" + N(n.rows[i].need) + ":k=" + N(n.rows[i].bag);
    return s + "]";
}

static void BaselineCountOffOrOutsideTheRouteIsTheGamesOwn()
{
    CraftMatsMod off;                                     // off by default
    const int64_t a = off.CountAnswer(true, 2, 216);      // in the route, but off
    CraftMatsMod on;
    on.SetEnabled(true);
    const int64_t b = on.CountAnswer(false, 2, 216);      // on, outside every route frame
    const int64_t c = on.CountAnswer(false, 2, -1);       // outside the route an unreadable stash is never asked about
    Check("baseline/count_off_or_outside_the_route_is_the_games_own",
          a == 2 && b == 2 && c == 2 && off.CountsRaised() == 0 && on.CountsRaised() == 0
              && on.TakeFirstRefusal() == CraftMatsRefusal::None && off.TakeFirstRefusal() == CraftMatsRefusal::None,
          "a=" + N(a) + " b=" + N(b) + " c=" + N(c) + " raised=" + N(on.CountsRaised()));
}

static void TargetCountOnInRouteAddsTheSpecialTabsStash()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsWalk w = SampleWalk();
    const int64_t ol = w.Count(CraftMatsMod::Material(15, 1));
    const int64_t dust = w.Count(CraftMatsMod::Material(14, 51));
    const int64_t none = w.Count(CraftMatsMod::Material(14, 50));
    const int64_t k = mod.CountAnswer(true, 2, ol);       // the bag's 2 Ol plus the stash's 216
    const int64_t zero = mod.CountAnswer(true, 1, none);  // nothing in the stash: the game's own number
    CraftMatsWalk unread;                                 // a walk that failed answers -1, never 0
    const bool sourcesOk = (int)CraftMatsSource::StashMaterialTab == 1 && (int)CraftMatsSource::StashSocketTab == 2
        && std::string(CraftMatsMod::SourceName(CraftMatsSource::StashMaterialTab)) == "materials"
        && std::string(CraftMatsMod::SourceName(CraftMatsSource::StashSocketTab)) == "socketable";
    Check("target/count_on_in_route_adds_the_special_tabs_stash",
          ol == 216 && dust == 153 && none == 0 && k == 218 && zero == 1 && mod.CountsRaised() == 1
              && unread.Count(CraftMatsMod::Material(15, 1)) == -1 && CraftMatsMod::Material(15, 1) == "class=15 b=1"
              && sourcesOk,
          "ol=" + N(ol) + " dust=" + N(dust) + " none=" + N(none) + " k=" + N(k) + " zero=" + N(zero)
              + " raised=" + N(mod.CountsRaised()));
}

static void TargetCountUnreadableStashLeavesTheGamesCountAndIsNamedOnce()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const int64_t a = mod.CountAnswer(true, 2, -1);
    const CraftMatsRefusal first = mod.TakeFirstRefusal();
    const std::string line = mod.RefusalLine(first);
    const int64_t b = mod.CountAnswer(true, 3, -1);
    const CraftMatsRefusal second = mod.TakeFirstRefusal();
    Check("target/count_unreadable_stash_leaves_the_games_count_and_is_named_once",
          a == 2 && b == 3 && first == CraftMatsRefusal::StashUnreadable && second == CraftMatsRefusal::None
              && mod.Refused(CraftMatsRefusal::StashUnreadable) == 2 && mod.IsEnabled() && mod.CountsRaised() == 0
              && line.rfind("craftmats: stash-unreadable - ", 0) == 0,
          "a=" + N(a) + " b=" + N(b) + " first=" + CraftMatsMod::RefusalName(first) + " second="
              + CraftMatsMod::RefusalName(second) + " line=\"" + line + "\"");
}

static void TargetDisplayCountReusesOneWalkPerFrameAndThePressWalksFresh()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const bool nothingKept = mod.MustWalk(10, false);
    mod.KeepWalk(10, SampleWalk());
    const bool sameFrame = mod.MustWalk(10, false);       // a display count in the same frame reuses it
    const bool press = mod.MustWalk(10, true);            // CraftFindRecipeItems / DoCraftResult never reuse
    const bool nextFrame = mod.MustWalk(11, false);
    const int64_t kept = mod.KeptWalk().Count(CraftMatsMod::Material(15, 1));
    Check("target/display_count_reuses_one_walk_per_frame_and_the_press_walks_fresh",
          nothingKept && !sameFrame && press && nextFrame && kept == 216 && mod.Walks() == 1,
          "nothing-kept=" + N(nothingKept) + " same-frame=" + N(sameFrame) + " press=" + N(press)
              + " next-frame=" + N(nextFrame) + " kept=" + N(kept));
}

static void TargetNeedsPairEachCountWithTheDecodeBeforeIt()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    mod.BeginFind(7);
    mod.OnDecode(3);                                              // input 1: 3 Ol
    mod.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    mod.OnDecode(5);                                              // input 2 accepts two bases: counted one at a time,
    mod.OnFindCount(CraftMatsMod::Material(14, 50), 0, false);    // and the last count is the one the game used
    mod.OnFindCount(CraftMatsMod::Material(14, 51), 1, true);
    mod.OnDecode(99);                                             // a decode no count follows names no input
    const CraftMatsNeeds n = mod.Needs();
    const bool ok = n.readable && n.stashAdded && n.self == 7 && n.rows.size() == 2
        && n.rows[0].material == "class=15 b=1" && n.rows[0].need == 3 && n.rows[0].bag == 2
        && n.rows[1].material == "class=14 b=51" && n.rows[1].need == 5 && n.rows[1].bag == 1;
    // The next CraftFindRecipeItems entry discards the record.
    mod.BeginFind(8);
    mod.OnDecode(4);
    mod.OnFindCount(CraftMatsMod::Material(14, 51), 4, false);
    const CraftMatsNeeds next = mod.Needs();
    const bool nextOk = next.readable && !next.stashAdded && next.self == 8 && next.rows.size() == 1 && next.rows[0].need == 4;
    Check("target/needs_pair_each_count_with_the_decode_before_it", ok && nextOk, Describe(n) + " | " + Describe(next));
}

static void TargetNeedsWithNoDecodeAreUnreadable()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    mod.BeginFind(7);
    mod.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);      // a count with no decode before it
    const CraftMatsNeeds none = mod.Needs();
    mod.BeginFind(7);
    mod.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);      // the first of two inputs has no decode
    mod.OnDecode(3);
    mod.OnFindCount(CraftMatsMod::Material(14, 51), 1, true);
    const CraftMatsNeeds firstMissing = mod.Needs();
    mod.BeginFind(7);
    mod.OnDecode(-1);                                             // a decode that did not read as a whole amount
    mod.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    const CraftMatsNeeds badAmount = mod.Needs();
    // Negative control: the same count after a decode is readable.
    mod.BeginFind(7);
    mod.OnDecode(3);
    mod.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    const CraftMatsNeeds control = mod.Needs();
    Check("target/needs_with_no_decode_are_unreadable",
          !none.readable && !firstMissing.readable && !badAmount.readable && control.readable && control.rows.size() == 1,
          Describe(none) + " | " + Describe(firstMissing) + " | " + Describe(badAmount) + " | " + Describe(control));
}

static std::string Describe(const std::vector<CraftMatsEntryTake>& takes)
{
    std::string s = "[";
    for (size_t i = 0; i < takes.size(); ++i)
        s += (i ? "," : "") + takes[i].entry.key + ":" + N(takes[i].amount) + (takes[i].whole ? "whole" : "partial");
    return s + "]";
}

static void TargetTakeSplitsAcrossEntriesWholeThenPartial()
{
    CraftMatsWalk w;
    w.readable = true;
    w.entries = {
        Entry(CraftMatsMod::Material(14, 51), "a", 2, CraftMatsSource::StashMaterialTab, -1),
        Entry(CraftMatsMod::Material(15, 1), "other", 50, CraftMatsSource::StashSocketTab, 3),   // another material: never taken
        Entry(CraftMatsMod::Material(14, 51), "b", 3, CraftMatsSource::StashMaterialTab, -1),
        Entry(CraftMatsMod::Material(14, 51), "c", 10, CraftMatsSource::StashMaterialTab, -1),
    };
    const std::string m = CraftMatsMod::Material(14, 51);
    std::vector<CraftMatsEntryTake> four, five, one, tooMany, nothing;
    const bool fourOk = CraftMatsMod::Split(m, 4, w, four);
    const bool fiveOk = CraftMatsMod::Split(m, 5, w, five);
    const bool oneOk = CraftMatsMod::Split(m, 1, w, one);
    const bool tooManyOk = CraftMatsMod::Split(m, 16, w, tooMany);   // the tabs hold 15
    const bool nothingOk = CraftMatsMod::Split(m, 0, w, nothing);
    CraftMatsWalk unread;
    std::vector<CraftMatsEntryTake> blind;
    const bool blindOk = CraftMatsMod::Split(m, 1, unread, blind);
    const bool ok = fourOk && four.size() == 2 && four[0].entry.key == "a" && four[0].amount == 2 && four[0].whole
        && four[1].entry.key == "b" && four[1].amount == 2 && !four[1].whole
        && fiveOk && five.size() == 2 && five[0].whole && five[1].whole && five[1].amount == 3
        && oneOk && one.size() == 1 && one[0].entry.key == "a" && one[0].amount == 1 && !one[0].whole
        && !tooManyOk && tooMany.empty() && !nothingOk && nothing.empty() && !blindOk && blind.empty();
    Check("target/take_splits_across_entries_whole_then_partial", ok,
          "4=" + Describe(four) + " 5=" + Describe(five) + " 1=" + Describe(one) + " 16=" + Describe(tooMany));
}

static void TargetMoveConfirmedOnlyWhenSourceAndDestinationMovedByTheAmount()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const CraftMatsOutcome partial = mod.OnMoveReport(Move(2, 216, 214, false, false, 1, 3));   // stack 1 -> 3
    const CraftMatsOutcome whole = mod.OnMoveReport(Move(3, 3, 0, true, true, 0, 3));          // new unit of 3
    const CraftMatsOutcome declined = mod.OnMoveReport(Move(2, 216, 216, false, false, 1, 1));
    const bool onOk = partial == CraftMatsOutcome::Taken && whole == CraftMatsOutcome::Taken
        && declined == CraftMatsOutcome::NotTaken && mod.IsEnabled() && mod.Taken() == 2 && mod.TakenUnits() == 5
        && mod.Refused(CraftMatsRefusal::NotTaken) == 1;
    // Each of these is a loss and turns the mod off for the session.
    const CraftMatsMoveReport losses[] = {
        Move(2, 216, 214, false, false, 1, 1),   // the stash dropped, the bag did not rise
        Move(2, 216, 216, false, false, 1, 3),   // the bag rose, the stash did not drop
        Move(2, 216, 213, false, false, 1, 3),   // the stash dropped by more than the amount
        Move(2, 216, 214, false, false, 1, 4),   // the bag rose by more than the amount
        Move(3, 3, 0, true, false, 0, 3),        // the entry is gone but its cell is not: the save crash
        Move(3, 3, 3, false, true, 0, 3),        // the cell is gone but the entry is not
        Move(2, 3, 0, true, true, 0, 2),         // a whole entry of 3 gone for an amount of 2
        Move(2, 216, 214, false, false, 1, -1),  // the destination could not be re-read
        Move(2, -1, 214, false, false, 1, 3),    // the source was never read before
    };
    bool lossOk = true;
    std::string detail;
    for (const CraftMatsMoveReport& r : losses) {
        CraftMatsMod m;
        m.SetEnabled(true);
        const CraftMatsOutcome o = m.OnMoveReport(r);
        const bool due = m.TakeFirstLoss();
        const std::string line = m.LossLine();
        const bool one = o == CraftMatsOutcome::Loss && m.OffThisSession() && !m.IsEnabled() && due
            && line.rfind("craftmats: off for this session - ", 0) == 0 && !m.SetEnabled(true);
        lossOk = lossOk && one;
        detail += " " + N((int)o);
    }
    Check("target/move_confirmed_only_when_source_and_destination_moved_by_the_amount", onOk && lossOk,
          "partial=" + N((int)partial) + " whole=" + N((int)whole) + " declined=" + N((int)declined) + " losses:" + detail);
}

static void TargetPressCraftsOnlyWhenEveryTakeIsConfirmed()
{
    CraftMatsMod mod;
    mod.SetEnabled(true);
    const bool bagCovers = mod.MayCraft({});   // no take planned: the game crafts from the bag
    const bool allTaken = mod.MayCraft({ CraftMatsOutcome::Taken, CraftMatsOutcome::Taken });
    const bool oneDeclined = mod.MayCraft({ CraftMatsOutcome::Taken, CraftMatsOutcome::NotTaken });
    const bool oneLost = mod.MayCraft({ CraftMatsOutcome::Loss });
    Check("target/press_crafts_only_when_every_take_is_confirmed",
          bagCovers && allTaken && !oneDeclined && !oneLost && mod.PressesRefused() == 2,
          "bag=" + N(bagCovers) + " all=" + N(allTaken) + " declined=" + N(oneDeclined) + " lost=" + N(oneLost)
              + " refused=" + N(mod.PressesRefused()));

    // The gate before any take. No record, or no stash count added during this
    // press's CraftFindRecipeItems: the game's own press, untouched.
    CraftMatsMod gate;
    gate.SetEnabled(true);
    const CraftMatsPressStep noFind = gate.PressStep(7);
    gate.BeginFind(7);
    gate.OnDecode(3);
    gate.OnFindCount(CraftMatsMod::Material(15, 1), 3, false);
    const CraftMatsPressStep bagOnly = gate.PressStep(7);
    gate.BeginFind(7);
    gate.OnDecode(3);
    gate.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    const CraftMatsPressStep plan = gate.PressStep(7);
    const CraftMatsPressStep reused = gate.PressStep(7);   // one record serves one press
    gate.BeginFind(7);
    gate.OnDecode(3);
    gate.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    const CraftMatsPressStep otherSelf = gate.PressStep(8);
    gate.BeginFind(7);
    gate.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    const CraftMatsPressStep noDecode = gate.PressStep(7);
    // A recipe row the adapter could not number matches nothing, itself included.
    gate.BeginFind(-1);
    gate.OnDecode(3);
    gate.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    const CraftMatsPressStep unnumbered = gate.PressStep(-1);
    const CraftMatsRefusal first = gate.TakeFirstRefusal();
    const std::string line = gate.RefusalLine(first);
    // A plan the core refused refuses the press before any take.
    CraftMatsPlan refusedPlan;
    refusedPlan.refused = true;
    const bool refusedPlanCrafts = gate.MayCraft(refusedPlan, {});
    Check("target/press_gate_is_vanilla_without_a_stash_count_and_refuses_what_it_cannot_pair",
          noFind == CraftMatsPressStep::Vanilla && bagOnly == CraftMatsPressStep::Vanilla
              && plan == CraftMatsPressStep::Plan && reused == CraftMatsPressStep::Refuse
              && otherSelf == CraftMatsPressStep::Refuse && noDecode == CraftMatsPressStep::Refuse
              && unnumbered == CraftMatsPressStep::Refuse && !refusedPlanCrafts
              && gate.PressesRefused() == 5 && first == CraftMatsRefusal::Unreadable
              && line.find("refused") != std::string::npos && gate.IsEnabled(),
          "no-find=" + N((int)noFind) + " bag-only=" + N((int)bagOnly) + " plan=" + N((int)plan) + " reused="
              + N((int)reused) + " other-self=" + N((int)otherSelf) + " no-decode=" + N((int)noDecode)
              + " unnumbered=" + N((int)unnumbered) + " line=\"" + line + "\"");
}

static void BaselinePressOffIsVanilla()
{
    CraftMatsMod mod;   // off
    mod.BeginFind(7);
    mod.OnDecode(3);
    mod.OnFindCount(CraftMatsMod::Material(15, 1), 2, true);
    const CraftMatsPressStep step = mod.PressStep(7);
    Check("baseline/press_off_is_vanilla",
          step == CraftMatsPressStep::Vanilla && mod.PressesRefused() == 0 && mod.TakeFirstRefusal() == CraftMatsRefusal::None,
          "step=" + N((int)step));
}

static void TargetConsumeMismatchTurnsTheModOffForTheSession()
{
    CraftMatsMod ok;
    ok.SetEnabled(true);
    const std::string m = CraftMatsMod::Material(15, 1);
    // The bag held 1, 2 came from the stash, the recipe needs 3: 0 after.
    const bool matched = ok.OnConsume(m, 1, 2, 3, 0);
    const bool matchedOk = matched && ok.IsEnabled() && !ok.TakeFirstMismatch();
    // The game's own duplication: it produced, and consumed nothing.
    CraftMatsMod dup;
    dup.SetEnabled(true);
    const bool dupMatched = dup.OnConsume(m, 1, 2, 3, 3);
    const bool due = dup.TakeFirstMismatch();
    const bool dueAgain = dup.TakeFirstMismatch();
    const std::string line = dup.MismatchLine();
    const bool dupOk = !dupMatched && dup.OffThisSession() && !dup.IsEnabled() && due && !dueAgain && !dup.SetEnabled(true)
        && line.rfind("craftmats: consume mismatch - ", 0) == 0 && line.find("off for this session") != std::string::npos
        && line.find("class=15 b=1") != std::string::npos;
    // A total that could not be re-read is not a match either.
    CraftMatsMod blind;
    blind.SetEnabled(true);
    const bool blindMatched = blind.OnConsume(m, 1, 2, 3, -1);
    Check("target/consume_mismatch_turns_the_mod_off_for_the_session",
          matchedOk && dupOk && !blindMatched && blind.OffThisSession(),
          "matched=" + N(matched) + " dup=" + N(dupMatched) + " blind=" + N(blindMatched) + " line=\"" + line + "\"");
}

static void TargetSaveRequestedOnlyAfterAConfirmedMove()
{
    CraftMatsMoved a;
    a.material = CraftMatsMod::Material(15, 1);
    a.from = CraftMatsSource::StashSocketTab;
    a.to = CraftMatsDestination::BagStack;
    a.units = 2;
    CraftMatsMoved b = a;
    b.units = 1;   // a second entry of the same material, same route: one figure
    CraftMatsMoved c;
    c.material = CraftMatsMod::Material(14, 51);
    c.from = CraftMatsSource::StashMaterialTab;
    c.to = CraftMatsDestination::Cube;
    c.units = 5;
    const std::string one = CraftMatsMod::PressLine({ a, b }, CraftMatsSave::Yes);
    const std::string two = CraftMatsMod::PressLine({ a, c }, CraftMatsSave::Failed);
    const bool ok = !CraftMatsMod::SaveDue(0) && CraftMatsMod::SaveDue(3)
        && one == "craftmats: moved 3 class=15 b=1 from socketable to bag-stack; saved=yes"
        && two == "craftmats: moved 2 class=15 b=1 from socketable to bag-stack, 5 class=14 b=51 from materials to cube; saved=failed"
        && std::string(CraftMatsMod::SaveName(CraftMatsSave::No)) == "no"
        && std::string(CraftMatsMod::DestinationName(CraftMatsDestination::BagNew)) == "bag-new";
    Check("target/save_requested_only_after_a_confirmed_move", ok, "one=\"" + one + "\" two=\"" + two + "\"");
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
    BaselineCountOffOrOutsideTheRouteIsTheGamesOwn();
    TargetCountOnInRouteAddsTheSpecialTabsStash();
    TargetCountUnreadableStashLeavesTheGamesCountAndIsNamedOnce();
    TargetDisplayCountReusesOneWalkPerFrameAndThePressWalksFresh();
    TargetNeedsPairEachCountWithTheDecodeBeforeIt();
    TargetNeedsWithNoDecodeAreUnreadable();
    TargetTakeSplitsAcrossEntriesWholeThenPartial();
    TargetMoveConfirmedOnlyWhenSourceAndDestinationMovedByTheAmount();
    TargetPressCraftsOnlyWhenEveryTakeIsConfirmed();
    BaselinePressOffIsVanilla();
    TargetConsumeMismatchTurnsTheModOffForTheSession();
    TargetSaveRequestedOnlyAfterAConfirmedMove();
    std::cout << (g_Failures ? "RESULT FAIL " + std::to_string(g_Failures) : std::string("RESULT OK")) << "\n";
    return g_Failures ? 1 : 0;
}
