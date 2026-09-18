// Behavioral harness for the auto-prospect core (ForgePact issue #9, Stage B).
//
// The Python runner splices the REAL plugin/include/ForgePact/AutoProspectMod.hpp
// in at the marker below (its #pragma and #include lines removed). The header is
// game-independent by contract - it names no runtime interface - so it compiles
// here with no runtime stub, and every threshold is read from its own
// declarations rather than restated.
//
// The core decides; the adapter in ModuleMain.cpp (Stage B round 2) only
// re-reads the game and invokes. So these scenarios drive the core the way
// the adapter will: the m_MoveItemToGrid hook calls OnInsert during the step,
// FrameCallback builds an AutoProspectView from what it re-found this frame
// and calls Decide, and an Invoke is followed by OnInvoked. `Frame` below is
// that adapter loop, with the invoke itself replaced by a counter.
//
// Baseline: the mod is off by default and, off, never invokes whatever it is
// fed. Target: one invoke per landed insert into the ProspectGrid, nothing
// for anything else, and every refusal counted, named once and said in a
// line that names what the mod did.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// PRODUCTION_AUTOPROSPECT

using ForgePact::AutoProspectAction;
using ForgePact::AutoProspectDecision;
using ForgePact::AutoProspectMod;
using ForgePact::AutoProspectRefusal;
using ForgePact::AutoProspectView;

static int g_Failures = 0;

static void Check(const std::string& label, bool ok, const std::string& detail)
{
    if (ok) std::cout << "PASS " << label << "\n";
    else { std::cout << "FAIL " << label << " " << detail << "\n"; ++g_Failures; }
}

static const int64_t kNode = 5001;
static const int64_t kOtherNode = 5002;
static const int kCells = 54;   // the vanilla 9x6 grid; only the free-cell count matters to the core

// What the adapter re-read this frame: window, ProspectGrid node and button
// all found, the cells read.
static AutoProspectView View(int64_t node, int filled, const std::string& prints = "", bool button = true)
{
    AutoProspectView v;
    v.window = true;
    v.grid = true;
    v.button = button;
    v.args = button;
    v.contents = true;
    v.nodeId = node;
    v.filled = filled;
    v.empty = kCells - filled;
    v.fingerprints = prints;
    return v;
}

struct Tally { int invokes = 0; int refusals = 0; AutoProspectRefusal last = AutoProspectRefusal::None; };

// One adapter frame: Decide, and on Invoke "call the handler" and report it
// with the grid as re-read straight after the call. The handler changes the
// grid inside the call (Phase 1 P-shapes: every `press` line's contents-after
// already showed the materials), so `after` is what the adapter reads then;
// with no `after`, the call changed nothing.
static AutoProspectDecision Frame(AutoProspectMod& mod, const AutoProspectView& v, Tally& t,
                                  const AutoProspectView* after = nullptr, bool dispatched = true)
{
    const AutoProspectDecision d = mod.Decide(v);
    if (d.action == AutoProspectAction::Invoke) { ++t.invokes; mod.OnInvoked(dispatched, after ? *after : v); }
    if (d.action == AutoProspectAction::Refuse) { ++t.refusals; t.last = d.reason; }
    return d;
}

static std::string N(long v) { return std::to_string(v); }

// ---- baseline: off by default, and off does nothing -----------------------

static void BaselineOffByDefault()
{
    AutoProspectMod mod;
    const std::string line = mod.StatLine();
    Check("baseline/off_by_default", !mod.IsEnabled() && line.rfind("autoprospect: off", 0) == 0,
          "enabled=" + N(mod.IsEnabled()) + " line=\"" + line + "\"");
}

static void BaselineOffNeverInvokes()
{
    AutoProspectMod mod;
    Tally t;
    // Everything that would make the mod act when on: first sight, inserts
    // into the grid, a rising count, a full grid, a missing window or button.
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 2), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, kCells - 1), t);
    mod.OnInsert(kNode, true, false);
    AutoProspectView closed;
    Frame(mod, closed, t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 3, "", false), t);
    Check("baseline/off_never_invokes",
          t.invokes == 0 && t.refusals == 0 && mod.Invoked() == 0 && !mod.HasPending()
              && mod.TakeFirstRefusal() == AutoProspectRefusal::None,
          "invokes=" + N(t.invokes) + " refusals=" + N(t.refusals) + " invoked=" + N(mod.Invoked())
              + " pending=" + N(mod.HasPending()));
}

// ---- target: the mod on -----------------------------------------------------
//
// Observed failing 2026-09-18 against a stub core that never invokes (every
// Decide returns None, OnInsert/OnInvoked do nothing, every counter 0,
// Settled() -1, empty StatLine, RefusalName and RefusalLine), before the core
// was written:
//   FAIL target/insert_into_prospect_grid_invokes_once invokes=0 invoked=0 refusals=0
//   FAIL target/late_landing_invokes_when_it_lands a=0 b=0 c=0 invokes=0
//   FAIL target/insert_elsewhere_ignored invokes=0 elsewhere=0 pending=0
//   FAIL target/rearrangement_never_invokes_and_expires_not_landed invokes=0 notLanded=0 pending=0
//   FAIL target/insert_while_invoking_ignored_and_counted invokes=0 whileInvoking=0 pending=0
//   FAIL target/inserts_in_one_frame_coalesce invokes=0 inserts=0 coalesced=0
//   FAIL target/grid_full_refuses_and_counts action=0 reason=0 invokes=0 gridFull=0 line=""
//   FAIL target/no_window_refuses_and_drops_pending action=0 reason=0 dropped=1 invokes=0
//   FAIL target/no_button_refuses_and_drops_pending action=0 reason=0 dropped=1 invokes=0
//   FAIL target/pending_on_a_replaced_node_is_dropped action=0 reason=0 invokes=0
//   FAIL target/new_node_resets_settled_count settled=-1 invokes=0
//   FAIL target/removal_lowers_settled_count invokes=0 settled=-1
//   FAIL target/ran_no_effect_counted_one_frame_later invokes=0 before=0 unchanged=0 ranNoEffect=0 prospected=0
//   FAIL target/first_refusal_reported_once_per_reason first=0 second=0 third=0 noButton=0 name=""
//   FAIL target/statline_names_what_it_did line="" off=""
// The two "never invokes" halves - the rearrangement and the insert elsewhere
// - fail on the counter they must also report (notLanded, elsewhere), not on
// an invoke: a core that never invokes trivially never invokes wrongly, so
// those counters are the witnesses. baseline/off_never_invokes PASSED against
// that stub, as a baseline should; baseline/off_by_default failed only on the
// empty stat line (`enabled=0 line=""`).

static void TargetInsertIntoProspectGridInvokesOnce()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);             // first sight settles 0
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 3, "m-0");
    Frame(mod, View(kNode, 1, "a-14"), t, &mats);   // landed: invoke; the materials appear inside the call
    for (int i = 0; i < 5; ++i) Frame(mod, mats, t);   // materials stay; no new insert
    Check("target/insert_into_prospect_grid_invokes_once", t.invokes == 1 && mod.Invoked() == 1 && t.refusals == 0,
          "invokes=" + N(t.invokes) + " invoked=" + N(mod.Invoked()) + " refusals=" + N(t.refusals));
}

static void TargetLateLandingInvokesWhenItLands()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectDecision a = Frame(mod, View(kNode, 0), t);   // the cells have not updated yet
    const AutoProspectDecision b = Frame(mod, View(kNode, 0), t);
    const AutoProspectDecision c = Frame(mod, View(kNode, 1, "a-14"), t);
    Check("target/late_landing_invokes_when_it_lands",
          a.action == AutoProspectAction::None && b.action == AutoProspectAction::None
              && c.action == AutoProspectAction::Invoke && t.invokes == 1,
          "a=" + N((int)a.action) + " b=" + N((int)b.action) + " c=" + N((int)c.action) + " invokes=" + N(t.invokes));
}

static void TargetInsertElsewhereIgnored()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kOtherNode, false, false);    // into the inventory grid, not the ProspectGrid
    Frame(mod, View(kNode, 1), t);
    Frame(mod, View(kNode, 1), t);
    Check("target/insert_elsewhere_ignored", t.invokes == 0 && mod.Elsewhere() == 1 && !mod.HasPending(),
          "invokes=" + N(t.invokes) + " elsewhere=" + N(mod.Elsewhere()) + " pending=" + N(mod.HasPending()));
}

static void TargetRearrangementNeverInvokesAndExpiresNotLanded()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 2, "a-14"), t);
    mod.OnInsert(kNode, true, false);          // an item moved from one cell of the grid to another
    for (int i = 0; i < ForgePact::kAutoProspectLandFrames + 2; ++i) Frame(mod, View(kNode, 2, "a-14"), t);
    Check("target/rearrangement_never_invokes_and_expires_not_landed",
          t.invokes == 0 && mod.NotLanded() == 1 && !mod.HasPending(),
          "invokes=" + N(t.invokes) + " notLanded=" + N(mod.NotLanded()) + " pending=" + N(mod.HasPending()));
}

static void TargetInsertWhileInvokingIgnoredAndCounted()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectDecision d = mod.Decide(View(kNode, 1, "a-14"));
    if (d.action == AutoProspectAction::Invoke) {
        ++t.invokes;
        // The game's own handler moves materials into the grid inside our call.
        mod.OnInsert(kNode, true, true);
        mod.OnInsert(kNode, true, true);
        mod.OnInvoked(true, View(kNode, 3, "m-0"));
    }
    for (int i = 0; i < 5; ++i) Frame(mod, View(kNode, 3, "m-0"), t);
    Check("target/insert_while_invoking_ignored_and_counted",
          t.invokes == 1 && mod.WhileInvoking() == 2 && !mod.HasPending(),
          "invokes=" + N(t.invokes) + " whileInvoking=" + N(mod.WhileInvoking()) + " pending=" + N(mod.HasPending()));
}

static void TargetInsertsInOneFrameCoalesce()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    mod.OnInsert(kNode, true, false);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 4, "m-0");
    Frame(mod, View(kNode, 3, "a-14,b-14,c-14"), t, &mats);
    for (int i = 0; i < 3; ++i) Frame(mod, View(kNode, 4, "m-0"), t);
    Check("target/inserts_in_one_frame_coalesce",
          t.invokes == 1 && mod.Inserts() == 3 && mod.Coalesced() == 2,
          "invokes=" + N(t.invokes) + " inserts=" + N(mod.Inserts()) + " coalesced=" + N(mod.Coalesced()));
}

static void TargetGridFullRefusesAndCounts()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    const int start = kCells - ForgePact::kAutoProspectMinFreeCells;   // exactly the minimum free
    Frame(mod, View(kNode, start), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectDecision d = Frame(mod, View(kNode, start + 1), t);   // one fewer free than needed
    for (int i = 0; i < 3; ++i) Frame(mod, View(kNode, start + 1), t);
    const std::string line = mod.RefusalLine(AutoProspectRefusal::GridFull);
    const std::string want = "holding back - " + N(ForgePact::kAutoProspectMinFreeCells - 1) + " free cells, needs "
        + N(ForgePact::kAutoProspectMinFreeCells) + "; take the materials out";
    Check("target/grid_full_refuses_and_counts",
          d.action == AutoProspectAction::Refuse && d.reason == AutoProspectRefusal::GridFull && t.invokes == 0
              && mod.Refused(AutoProspectRefusal::GridFull) == 1 && !mod.HasPending()
              && line.find(want) != std::string::npos,
          "action=" + N((int)d.action) + " reason=" + N((int)d.reason) + " invokes=" + N(t.invokes)
              + " gridFull=" + N(mod.Refused(AutoProspectRefusal::GridFull)) + " line=\"" + line + "\"");
}

static void TargetNoWindowRefusesAndDropsPending()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    AutoProspectView closed;                   // nothing found: the window closed with an insert pending
    const AutoProspectDecision d = Frame(mod, closed, t);
    const bool dropped = !mod.HasPending();
    Frame(mod, View(kNode, 1), t);             // the same node read again: nothing may fire for the old insert
    Check("target/no_window_refuses_and_drops_pending",
          d.action == AutoProspectAction::Refuse && d.reason == AutoProspectRefusal::NoWindow && dropped && t.invokes == 0
              && mod.Refused(AutoProspectRefusal::NoWindow) == 1,
          "action=" + N((int)d.action) + " reason=" + N((int)d.reason) + " dropped=" + N(dropped)
              + " invokes=" + N(t.invokes));
}

static void TargetNoButtonRefusesAndDropsPending()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectDecision d = Frame(mod, View(kNode, 1, "a-14", false), t);
    const bool dropped = !mod.HasPending();
    Frame(mod, View(kNode, 1, "a-14", true), t);
    Check("target/no_button_refuses_and_drops_pending",
          d.action == AutoProspectAction::Refuse && d.reason == AutoProspectRefusal::NoButton && dropped && t.invokes == 0
              && mod.Refused(AutoProspectRefusal::NoButton) == 1,
          "action=" + N((int)d.action) + " reason=" + N((int)d.reason) + " dropped=" + N(dropped)
              + " invokes=" + N(t.invokes));
}

static void TargetPendingOnAReplacedNodeIsDropped()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    // The window was closed and reopened within the frame: a new node, whose
    // count says nothing about the old insert.
    const AutoProspectDecision d = Frame(mod, View(kOtherNode, 1), t);
    Check("target/pending_on_a_replaced_node_is_dropped",
          d.action == AutoProspectAction::Refuse && d.reason == AutoProspectRefusal::NodeChanged && t.invokes == 0
              && !mod.HasPending(),
          "action=" + N((int)d.action) + " reason=" + N((int)d.reason) + " invokes=" + N(t.invokes));
}

static void TargetNewNodeResetsSettledCount()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 5), t);             // node A settles at 5
    Frame(mod, View(kOtherNode, 0), t);        // reopened: node B, first sight at 0
    const int settled = mod.Settled();
    mod.OnInsert(kOtherNode, true, false);
    Frame(mod, View(kOtherNode, 1, "a-14"), t);   // 1 is below A's 5, above B's 0
    Check("target/new_node_resets_settled_count", settled == 0 && t.invokes == 1,
          "settled=" + N(settled) + " invokes=" + N(t.invokes));
}

static void TargetRemovalLowersSettledCount()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 5, "m-0"), t);
    Frame(mod, View(kNode, 2, "m-0"), t);      // the player took materials out
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 3, "m-0,a-14"), t);
    Check("target/removal_lowers_settled_count", t.invokes == 1 && mod.Settled() >= 0,
          "invokes=" + N(t.invokes) + " settled=" + N(mod.Settled()));
}

static void TargetRanNoEffectCountedOneFrameLater()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 1, "a-14"), t);     // invoke
    const long before = mod.RanNoEffect();     // nothing is known until the next frame
    Frame(mod, View(kNode, 1, "a-14"), t);     // the grid did not change
    const long unchanged = mod.RanNoEffect();
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 4, "m-0,n-0");
    Frame(mod, View(kNode, 2, "a-14,b-14"), t, &mats);   // invoke
    Frame(mod, View(kNode, 4, "m-0,n-0"), t);  // changed: prospected
    Check("target/ran_no_effect_counted_one_frame_later",
          t.invokes == 2 && before == 0 && unchanged == 1 && mod.RanNoEffect() == 1 && mod.Prospected() == 1,
          "invokes=" + N(t.invokes) + " before=" + N(before) + " unchanged=" + N(unchanged)
              + " ranNoEffect=" + N(mod.RanNoEffect()) + " prospected=" + N(mod.Prospected()));
}

static void TargetFirstRefusalReportedOncePerReason()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 1, "a-14", false), t);       // no-button
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 2, "a-14,b-14", false), t);  // no-button again
    Frame(mod, View(kNode, kCells - 1), t);             // settles with 1 free cell
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, kCells), t);                 // grid-full
    const AutoProspectRefusal first = mod.TakeFirstRefusal();
    const AutoProspectRefusal second = mod.TakeFirstRefusal();
    const AutoProspectRefusal third = mod.TakeFirstRefusal();
    const std::string name = AutoProspectMod::RefusalName(AutoProspectRefusal::NoButton);
    Check("target/first_refusal_reported_once_per_reason",
          first == AutoProspectRefusal::NoButton && second == AutoProspectRefusal::GridFull
              && third == AutoProspectRefusal::None && mod.Refused(AutoProspectRefusal::NoButton) == 2 && name == "no-button",
          "first=" + N((int)first) + " second=" + N((int)second) + " third=" + N((int)third)
              + " noButton=" + N(mod.Refused(AutoProspectRefusal::NoButton)) + " name=\"" + name + "\"");
}

static void TargetStatlineNamesWhatItDid()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 1, "a-14"), t);     // invoke
    Frame(mod, View(kNode, 1, "a-14"), t);     // ran-no-effect
    Frame(mod, View(kNode, kCells - 1), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, kCells), t);        // grid-full
    const std::string line = mod.StatLine();
    const bool ok = line.rfind("autoprospect: ON ", 0) == 0
        && line.find("invoked=1") != std::string::npos
        && line.find("ran-no-effect=1") != std::string::npos
        && line.find("grid-full=1") != std::string::npos
        && line.find("not-landed=0") != std::string::npos
        && line.find("inserts=2") != std::string::npos;
    mod.SetEnabled(false);
    const std::string off = mod.StatLine();
    Check("target/statline_names_what_it_did", ok && off.rfind("autoprospect: off ", 0) == 0,
          "line=\"" + line + "\" off=\"" + off + "\"");
}

// ---- target: insert timing (Stage B Phase 1, the ship round) ----------------
//
// Phase 1's click-in `watch` line read `contents=6->6`: the cell was already
// filled when m_MoveItemToGrid was entered. Whether the fill happened in the
// same frame as the hook or an earlier one is not measured, and a drag-in's
// timing is not observed at all. So an insert may land before the hook that
// reports it, and "filled unchanged since the frame before the hook" can be a
// real insert. The core tells an insert from a rearrangement by the count
// SETTLED at the last invoke, refusal or first sight, lowered by every removal
// it sees and never raised by an unreported fill - not by the count one frame
// earlier.
//
// Observed 2026-09-18 against the round-2 core (settled re-read on every
// frame with nothing pending, left at the pre-invoke count after an invoke and
// after a refusal), shimmed with the new names only - `args` and NoArgs, the
// three shape names, an OnInvoked that ignored the `after` view, and a
// TakeFirstProspect/FirstProspectLine that reported nothing:
//   FAIL target/click_in_filled_a_frame_before_the_hook_invokes_once invokes=0 notLanded=1
//   FAIL target/rearrangement_right_after_an_invoke_never_invokes invokes=2 notLanded=0 pending=0
//   FAIL target/refused_item_rearranged_never_invokes invokes=1 notLanded=0 noButton=1
//   FAIL target/no_args_refuses_and_says_why action=1 reason=0 invokes=1 line="autoprospect: none - nothing"
//   FAIL target/first_prospect_reported_once_in_the_players_log early=0 beforeEffect=0 first=0 second=0 prospected=2 line=""
//   FAIL adapter/measured_session_prospects_each_insert_once invokes=1 prospected=1 ranNoEffect=0 first=0 refusals=0
// The measured same-frame click-in and the insert straight after an invoke
// PASSED against it: the old core read the measured sequence correctly, and
// failed only when the fill ran a frame ahead of the hook (not measured
// either way) or when the grid already held something it had not settled -
// the materials of the invoke before, or an item a refusal left behind. The
// second of those fired the handler on a rearrangement.

static void TargetClickInFilledInTheSameFrameInvokesOnce()
{
    // The measured click-in: the frame before, 5 filled; during the step the
    // cell fills and then m_MoveItemToGrid runs (6->6 across the hook).
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 5, "m-0"), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 5, "m-0,n-0");
    Frame(mod, View(kNode, 6, "m-0,a-14"), t, &mats);
    for (int i = 0; i < 3; ++i) Frame(mod, mats, t);
    Check("target/click_in_filled_in_the_same_frame_invokes_once", t.invokes == 1 && mod.NotLanded() == 0,
          "invokes=" + N(t.invokes) + " notLanded=" + N(mod.NotLanded()));
}

static void TargetClickInFilledAFrameBeforeTheHookInvokesOnce()
{
    // The same insert, with the fill one frame ahead of the hook that reports
    // it: the frame between already shows 6.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 5, "m-0"), t);
    Frame(mod, View(kNode, 6, "m-0,a-14"), t);   // filled, not yet reported
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 5, "m-0,n-0");
    for (int i = 0; i < ForgePact::kAutoProspectLandFrames + 2 && t.invokes == 0; ++i)
        Frame(mod, View(kNode, 6, "m-0,a-14"), t, &mats);
    Check("target/click_in_filled_a_frame_before_the_hook_invokes_once", t.invokes == 1 && mod.NotLanded() == 0,
          "invokes=" + N(t.invokes) + " notLanded=" + N(mod.NotLanded()));
}

static void TargetInsertRightAfterAnInvokeInvokesAgain()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 2, "m-0,n-0");
    Frame(mod, View(kNode, 1, "a-14"), t, &mats);   // invoke
    mod.OnInsert(kNode, true, false);                // the next click-in, the very next step
    const AutoProspectView mats2 = View(kNode, 3, "m-0,n-0,o-0");
    Frame(mod, View(kNode, 3, "m-0,n-0,b-14"), t, &mats2);
    Check("target/insert_right_after_an_invoke_invokes_again", t.invokes == 2,
          "invokes=" + N(t.invokes) + " notLanded=" + N(mod.NotLanded()));
}

static void TargetRearrangementRightAfterAnInvokeNeverInvokes()
{
    // The materials an invoke produced are part of the settled grid: moving
    // one of them straight afterwards is a rearrangement, not an insert.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 3, "m-0,n-0");
    Frame(mod, View(kNode, 1, "a-14"), t, &mats);   // invoke
    mod.OnInsert(kNode, true, false);                // a material moved to another cell
    for (int i = 0; i < ForgePact::kAutoProspectLandFrames + 2; ++i) Frame(mod, mats, t);
    Check("target/rearrangement_right_after_an_invoke_never_invokes",
          t.invokes == 1 && mod.NotLanded() == 1 && !mod.HasPending(),
          "invokes=" + N(t.invokes) + " notLanded=" + N(mod.NotLanded()) + " pending=" + N(mod.HasPending()));
}

static void TargetRefusedItemRearrangedNeverInvokes()
{
    // A refused insert leaves its item in the grid; it is settled there, so
    // moving it inside the grid later does not prospect it.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 1, "a-14", false), t);   // no-button: the item stays
    mod.OnInsert(kNode, true, false);                // moved to another cell
    for (int i = 0; i < ForgePact::kAutoProspectLandFrames + 2; ++i) Frame(mod, View(kNode, 1, "a-14"), t);
    Check("target/refused_item_rearranged_never_invokes",
          t.invokes == 0 && mod.NotLanded() == 1 && mod.Refused(AutoProspectRefusal::NoButton) == 1,
          "invokes=" + N(t.invokes) + " notLanded=" + N(mod.NotLanded())
              + " noButton=" + N(mod.Refused(AutoProspectRefusal::NoButton)));
}

static void TargetNoArgsRefusesAndSaysWhy()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    AutoProspectView v = View(kNode, 1, "a-14");
    v.args = false;                                  // the button was found; its argument array was not
    const AutoProspectDecision d = Frame(mod, v, t);
    const std::string line = mod.RefusalLine(AutoProspectRefusal::NoArgs);
    Check("target/no_args_refuses_and_says_why",
          d.action == AutoProspectAction::Refuse && d.reason == AutoProspectRefusal::NoArgs && t.invokes == 0
              && line.find(ForgePact::kAutoProspectArgsVar) != std::string::npos,
          "action=" + N((int)d.action) + " reason=" + N((int)d.reason) + " invokes=" + N(t.invokes)
              + " line=\"" + line + "\"");
}

static void TargetFirstProspectReportedOnceInThePlayersLog()
{
    // S-player-dll: the player build logs one line naming work done - the
    // first prospect of the session - and never a second.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    const bool early = mod.TakeFirstProspect();
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 2, "m-0,n-0");
    Frame(mod, View(kNode, 1, "a-14"), t, &mats);   // invoke
    const bool beforeEffect = mod.TakeFirstProspect();
    Frame(mod, mats, t);                             // prospected
    const bool first = mod.TakeFirstProspect();
    const std::string line = mod.FirstProspectLine();
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats2 = View(kNode, 3, "m-0,n-0,o-0");
    Frame(mod, View(kNode, 3, "m-0,n-0,b-14"), t, &mats2);
    Frame(mod, mats2, t);
    const bool second = mod.TakeFirstProspect();
    Check("target/first_prospect_reported_once_in_the_players_log",
          !early && !beforeEffect && first && !second && mod.Prospected() == 2
              && line.rfind("autoprospect: first prospect - invoked=1 prospected=1 ", 0) == 0,
          "early=" + N(early) + " beforeEffect=" + N(beforeEffect) + " first=" + N(first) + " second=" + N(second)
              + " prospected=" + N(mod.Prospected()) + " line=\"" + line + "\"");
}

// ---- adapter: the recorded invoke shape ---------------------------------
//
// Phase 1 recorded ONE shape that meets the ship rule (research doc, § Stage B
// results, P-shapes): `exec-index button:activationArgs self=found` - the
// handler's own asset index through script_execute, `self` = the Prospect
// button found by its handler variable, `other` = the window, and the
// button's own activationArgs array as the one argument. The adapter reads
// those names from the header, so they are pinned here, and the measured
// sequence of that session is replayed through the core.

static void AdapterRecordedShapeNames()
{
    const std::string handler = ForgePact::kAutoProspectHandlerVar;
    const std::string args = ForgePact::kAutoProspectArgsVar;
    const std::string grid = ForgePact::kAutoProspectGridName;
    Check("adapter/recorded_shape_exec_index_button_activation_args_self_found",
          handler == "activationFunc" && args == "activationArgs" && grid == "ProspectGrid",
          "handler=\"" + handler + "\" args=\"" + args + "\" grid=\"" + grid + "\"");
}

static void AdapterMeasuredSessionProspectsEachInsertOnce()
{
    // P-control's click-in (6->6 across the hook), then the recorded shape's
    // own invoke (filled 5->2 in P-shapes) and a materials-only grid that a
    // further hand press left unchanged (P-materials-only): one invoke per
    // insert, none for anything else, and the one player-log line.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 4, "m-0,n-0"), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView after = View(kNode, 2, "m-0,n-0");
    Frame(mod, View(kNode, 5, "m-0,n-0,a-14"), t, &after);
    Frame(mod, after, t);
    const bool first = mod.TakeFirstProspect();
    for (int i = 0; i < 10; ++i) Frame(mod, after, t);   // materials only: nothing to do
    Check("adapter/measured_session_prospects_each_insert_once",
          t.invokes == 1 && mod.Prospected() == 1 && mod.RanNoEffect() == 0 && first && t.refusals == 0,
          "invokes=" + N(t.invokes) + " prospected=" + N(mod.Prospected()) + " ranNoEffect=" + N(mod.RanNoEffect())
              + " first=" + N(first) + " refusals=" + N(t.refusals));
}

int main()
{
    BaselineOffByDefault();
    BaselineOffNeverInvokes();
    TargetInsertIntoProspectGridInvokesOnce();
    TargetLateLandingInvokesWhenItLands();
    TargetInsertElsewhereIgnored();
    TargetRearrangementNeverInvokesAndExpiresNotLanded();
    TargetInsertWhileInvokingIgnoredAndCounted();
    TargetInsertsInOneFrameCoalesce();
    TargetGridFullRefusesAndCounts();
    TargetNoWindowRefusesAndDropsPending();
    TargetNoButtonRefusesAndDropsPending();
    TargetPendingOnAReplacedNodeIsDropped();
    TargetNewNodeResetsSettledCount();
    TargetRemovalLowersSettledCount();
    TargetRanNoEffectCountedOneFrameLater();
    TargetFirstRefusalReportedOncePerReason();
    TargetStatlineNamesWhatItDid();
    TargetClickInFilledInTheSameFrameInvokesOnce();
    TargetClickInFilledAFrameBeforeTheHookInvokesOnce();
    TargetInsertRightAfterAnInvokeInvokesAgain();
    TargetRearrangementRightAfterAnInvokeNeverInvokes();
    TargetRefusedItemRearrangedNeverInvokes();
    TargetNoArgsRefusesAndSaysWhy();
    TargetFirstProspectReportedOnceInThePlayersLog();
    AdapterRecordedShapeNames();
    AdapterMeasuredSessionProspectsEachInsertOnce();
    std::cout << (g_Failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return g_Failures ? 1 : 0;
}
