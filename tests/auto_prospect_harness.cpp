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
        + N(ForgePact::kAutoProspectMinFreeCells) + "; empty some of the grid";
    // Phase 3 S5: the grid was full of items, not materials, when this line
    // first printed, so it must not tell the player to take materials out.
    Check("target/grid_full_refuses_and_counts",
          d.action == AutoProspectAction::Refuse && d.reason == AutoProspectRefusal::GridFull && t.invokes == 0
              && mod.Refused(AutoProspectRefusal::GridFull) == 1 && !mod.HasPending()
              && line.find(want) != std::string::npos && line.find("materials") == std::string::npos,
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

// ---- the player's log: an invoke that did nothing (closing round) -----------
//
// Phase 3 S7 (player DLL) counted ran-no-effect=1, and the player build said
// nothing about it: the count reached only `autoprospect stat`, which is
// research-only. So the player build now logs the first ran-no-effect and the
// first unverified of a session, once each, like the first prospect.
//
// Observed 2026-09-18 against the round-1 core with TakeFirstRanNoEffect /
// TakeFirstUnverified returning false and both lines empty, and the grid-full
// line still ending "; take the materials out":
//   FAIL target/grid_full_refuses_and_counts action=2 reason=6 invokes=0 gridFull=1 line="autoprospect: grid-full - holding back - 5 free cells, needs 6; take the materials out"
//   FAIL target/ran_no_effect_reported_once_in_the_players_log early=0 first=0 second=0 ranNoEffect=2 line=""
//   FAIL target/unverified_reported_once_in_the_players_log early=0 first=0 second=0 unverified=2 line=""
// baseline/prospects_with_effect_log_no_nothing_happened_line PASSED against
// it, as the negative control must: a core that never reports trivially never
// reports wrongly, and the target scenarios above are what it fails.

static void BaselineProspectsWithEffectLogNoNothingHappenedLine()
{
    // Every invoke changes the grid: neither line may be taken, ever.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats = View(kNode, 2, "m-0,n-0");
    Frame(mod, View(kNode, 1, "a-14"), t, &mats);
    Frame(mod, mats, t);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView mats2 = View(kNode, 3, "m-0,n-0,o-0");
    Frame(mod, View(kNode, 3, "m-0,n-0,b-14"), t, &mats2);
    Frame(mod, mats2, t);
    const bool noEffect = mod.TakeFirstRanNoEffect();
    const bool unverified = mod.TakeFirstUnverified();
    Check("baseline/prospects_with_effect_log_no_nothing_happened_line",
          t.invokes == 2 && mod.Prospected() == 2 && !noEffect && !unverified,
          "invokes=" + N(t.invokes) + " prospected=" + N(mod.Prospected()) + " noEffect=" + N(noEffect)
              + " unverified=" + N(unverified));
}

static void TargetRanNoEffectReportedOnceInThePlayersLog()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 1, "a-14"), t);           // invoke; the grid does not change
    const bool early = mod.TakeFirstRanNoEffect();   // nothing is known until the next frame
    Frame(mod, View(kNode, 1, "a-14"), t);           // ran-no-effect
    const bool first = mod.TakeFirstRanNoEffect();
    const std::string line = mod.RanNoEffectLine();
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 2, "a-14,b-14"), t);      // invoke; unchanged again
    Frame(mod, View(kNode, 2, "a-14,b-14"), t);
    const bool second = mod.TakeFirstRanNoEffect();
    Check("target/ran_no_effect_reported_once_in_the_players_log",
          !early && first && !second && mod.RanNoEffect() == 2
              && line.rfind("autoprospect: the Prospect ran but the grid did not change - invoked=1 prospected=0 ran-no-effect=1 ", 0) == 0,
          "early=" + N(early) + " first=" + N(first) + " second=" + N(second)
              + " ranNoEffect=" + N(mod.RanNoEffect()) + " line=\"" + line + "\"");
}

static void TargetUnverifiedReportedOnceInThePlayersLog()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    Frame(mod, View(kNode, 0), t);
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 1, "a-14"), t);           // invoke
    const bool early = mod.TakeFirstUnverified();
    AutoProspectView closed;                         // the next frame cannot show the grid
    Frame(mod, closed, t);
    const bool first = mod.TakeFirstUnverified();
    const std::string line = mod.UnverifiedLine();
    Frame(mod, View(kNode, 1, "a-14"), t);           // readable again
    mod.OnInsert(kNode, true, false);
    Frame(mod, View(kNode, 2, "a-14,b-14"), t);      // invoke
    Frame(mod, closed, t);                           // unreadable again
    const bool second = mod.TakeFirstUnverified();
    Check("target/unverified_reported_once_in_the_players_log",
          !early && first && !second && mod.EffectUnread() == 2 && !mod.TakeFirstRanNoEffect()
              && line.rfind("autoprospect: the Prospect ran but the grid could not be read afterwards - invoked=1 ", 0) == 0,
          "early=" + N(early) + " first=" + N(first) + " second=" + N(second)
              + " unverified=" + N(mod.EffectUnread()) + " line=\"" + line + "\"");
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

// ---- Stage C: the previous batch goes to the materials tab first -------------
//
// Stage C's M7 (research doc, § Stage C results) moved one material from the
// ProspectGrid into the materials tab by name: look the item up by the cell's
// fingerprint, ask the game whether it stacks, add it to the stack, and clear
// the cell only when the add reported success. The core does not know what a
// material is - the adapter reads each cell's item type through the SDK and
// hands the core a per-cell flag - and does not make the calls: it decides
// WHEN the pass runs (once per landed insert, just before the invoke, with the
// sub-option on), WHICH cells it names (materials only), and what each cell's
// outcome means. `BagFrame` below is the adapter loop: Decide; on
// MoveMaterials report each named cell, re-read, and Decide again in the same
// frame; then the invoke as before.
//
// Observed 2026-09-19 against the unchanged core, shimmed with the new names
// only (the MoveMaterials action, the per-cell view and moves, the move report
// and its outcomes, the bag flag and its lines - every one inert: Decide never
// asks for a pass, BagEnabled() false, SetBagEnabled refused, every count 0,
// every line empty):
//   FAIL target/bag_on_by_default_and_kept_across_the_parent_toggle byDefault=0 afterParentToggle=0 offKept=1 onAgain=0 line="autoprospect: ON invoked=0 prospected=0 ran-no-effect=0 unverified=0 failed=0 inserts=0 (coalesced=0 while-invoking=0 elsewhere=0 while-off=0) not-landed=0 refused(no-window=0 no-grid=0 unreadable=0 node-changed=0 no-button=0 grid-full=0 no-args=0)"
//   FAIL target/move_pass_before_the_invoke passes=0 invokes=1 order=invoke moves= moved=0
//   FAIL target/non_material_never_moved passes=0 invokes=2 moves= moved=0
//   FAIL target/move_pass_only_when_an_insert_lands passes=0 early=0 invokes=1 moved=0
//   FAIL target/landed_insert_stays_landed_across_the_move_pass passes=0 invokes=1 notLanded=0 settled=3 moved=0
//   FAIL target/refused_move_leaves_the_material_and_is_logged_once passes=0 invokes=2 notStackable=0 notAdded=0 moveFailed=0 first=0 second=0 third=0 fourth=0 bagOn=0 line=""
//   FAIL target/vanished_turns_the_move_pass_off_for_the_session passes=0 invokes=2 tried=0 vanished=0 first=0 reenable=0 line="" stat=0
//   FAIL target/cell_kept_after_add_turns_the_move_pass_off_for_the_session passes=0 invokes=2 tried=0 cell-kept=0 first=0 reenable=0 line="" stat=0
//   FAIL target/first_move_reported_once_in_the_players_log early=0 afterNothingMoved=0 first=0 second=0 moved=0 line=""
// (The landed scenario's settled=3 is the shim invoking straight away, with
// the five materials still in the grid, and settling on the invoke's own read.)
// baseline/bag_off_never_moves and baseline/parent_off_never_moves PASSED
// against it, as baselines must: a core that never asks for a pass trivially
// never asks for one wrongly, and the targets above are what it fails.
//
// Round 1 re-seeded these targets on purpose. They first seeded "the previous
// batch" as materials that simply sat in the grid, which under the round-1
// rule (the batch is what the core's own invoke produced) seeds nothing; each
// now seeds it through a prospect that records it (SeedBatch), and keeps its
// name and what it asserts. The FAIL lines above are the round-0 history.
// non_material_never_moved puts its non-material inside the batch, so the
// material flag is still what keeps it in the grid.

struct Mat { std::string fp; bool material; };

// A view whose cells carry the adapter's per-cell material flag.
static AutoProspectView CellsView(int64_t node, const std::vector<Mat>& cells)
{
    std::string prints;
    AutoProspectView v = View(node, (int)cells.size());
    for (size_t i = 0; i < cells.size(); ++i) {
        ForgePact::AutoProspectCell c;
        c.row = (int)(i % 6);
        c.col = (int)(i / 6);
        c.fingerprint = cells[i].fp;
        c.material = cells[i].material;
        v.cells.push_back(c);
        prints += (i ? "," : "") + cells[i].fp;
        if (std::find(v.printList.begin(), v.printList.end(), cells[i].fp) == v.printList.end())
            v.printList.push_back(cells[i].fp);
    }
    v.fingerprints = prints;
    return v;
}

// The adapter's report for one cell, by how the calls went.
static ForgePact::AutoProspectMoveReport Report(bool canAdd, bool success, int heldAfter)
{
    ForgePact::AutoProspectMoveReport r;
    r.heldBefore = true;
    r.lookup = true;
    r.canAddRan = true;
    r.canAdd = canAdd;
    r.addRan = canAdd;
    r.success = success;
    r.clearRan = success && heldAfter != 0;
    r.heldAfter = heldAfter;
    return r;
}
static ForgePact::AutoProspectMoveReport MovedReport() { return Report(true, true, 0); }

struct BagTally {
    int passes = 0;
    int tried = 0;                 // cells the adapter attempted
    std::string order;             // "move,move,invoke"
    std::string moves;             // the fingerprints the passes named
    bool early = false;            // a pass was asked for on a frame with no landed insert
};

// One adapter frame with the move pass: Decide; on MoveMaterials report each
// named cell with `report` (stopping when the pass turns itself off), re-read
// (`afterMove`), and Decide again the same frame; then invoke as `Frame` does.
static AutoProspectDecision BagFrame(AutoProspectMod& mod, const AutoProspectView& v, Tally& t, BagTally& b,
                                     const AutoProspectView& afterMove, const ForgePact::AutoProspectMoveReport& report,
                                     const AutoProspectView* afterInvoke = nullptr)
{
    AutoProspectDecision d = mod.Decide(v);
    if (d.action == AutoProspectAction::MoveMaterials) {
        ++b.passes;
        for (const auto& c : d.moves) {
            if (!mod.MovePassOn()) break;
            ++b.tried;
            b.order += std::string(b.order.empty() ? "" : ",") + "move";
            b.moves += std::string(b.moves.empty() ? "" : ",") + c.fingerprint;
            mod.OnMoveReport(report);
        }
        d = mod.Decide(afterMove);
    }
    if (d.action == AutoProspectAction::Invoke) {
        ++t.invokes;
        b.order += std::string(b.order.empty() ? "" : ",") + "invoke";
        mod.OnInvoked(true, afterInvoke ? *afterInvoke : afterMove);
    }
    if (d.action == AutoProspectAction::Refuse) { ++t.refusals; t.last = d.reason; }
    return d;
}

// A frame with no insert landing: nothing may ask for a pass.
static void QuietFrame(AutoProspectMod& mod, const AutoProspectView& v, Tally& t, BagTally& b)
{
    const AutoProspectDecision d = BagFrame(mod, v, t, b, v, MovedReport());
    if (d.action == AutoProspectAction::MoveMaterials) b.early = true;
}

// "The previous batch", seeded the way the core records one (round 1): a
// prospect of one item on the grid as it stands (`before`), whose read
// straight after the call holds `before` plus `batch`. The seed keeps its own
// tallies, so a scenario's counts start at what the scenario itself does.
static void SeedBatch(AutoProspectMod& mod, const std::vector<Mat>& before, const std::vector<Mat>& batch,
                      int64_t node = kNode)
{
    Tally t;
    BagTally b;
    QuietFrame(mod, CellsView(node, before), t, b);
    mod.OnInsert(node, true, false);
    std::vector<Mat> in = before;
    in.push_back({ "seed-14", false });
    std::vector<Mat> after = before;
    after.insert(after.end(), batch.begin(), batch.end());
    const AutoProspectView inView = CellsView(node, in);
    const AutoProspectView afterView = CellsView(node, after);
    BagFrame(mod, inView, t, b, inView, MovedReport(), &afterView);
    QuietFrame(mod, afterView, t, b);
}

static void BaselineBagOffNeverMoves()
{
    // Parent on, bag off: exactly the Stage B core - no pass, one invoke per insert.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    mod.SetBagEnabled(false);
    Tally t;
    BagTally b;
    const AutoProspectView mats = CellsView(kNode, { { "m-0", true }, { "n-0", true } });
    QuietFrame(mod, mats, t, b);
    std::vector<Mat> grid = { { "m-0", true }, { "n-0", true } };
    for (int i = 0; i < 2; ++i) {
        mod.OnInsert(kNode, true, false);
        const bool needs = mod.NeedsMaterials();
        std::vector<Mat> withItem = grid;
        withItem.push_back({ "a" + N(i) + "-14", false });
        grid.push_back({ "o" + N(i) + "-0", true });   // the insert's own materials, beside the old ones
        const AutoProspectView in = CellsView(kNode, withItem);
        const AutoProspectView after = CellsView(kNode, grid);
        BagFrame(mod, in, t, b, in, MovedReport(), &after);
        QuietFrame(mod, after, t, b);
        if (needs) b.early = true;
    }
    Check("baseline/bag_off_never_moves",
          b.passes == 0 && b.tried == 0 && t.invokes == 2 && mod.Moved() == 0 && !b.early,
          "passes=" + N(b.passes) + " tried=" + N(b.tried) + " invokes=" + N(t.invokes) + " moved=" + N(mod.Moved())
              + " needs=" + N(b.early));
}

static void BaselineParentOffNeverMoves()
{
    // Parent off (the default), bag at its default: nothing at all.
    AutoProspectMod mod;
    Tally t;
    BagTally b;
    const AutoProspectView mats = CellsView(kNode, { { "m-0", true }, { "n-0", true } });
    QuietFrame(mod, mats, t, b);
    mod.OnInsert(kNode, true, false);
    const bool needs = mod.NeedsMaterials();
    const AutoProspectView in = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "a-14", false } });
    BagFrame(mod, in, t, b, in, MovedReport());
    QuietFrame(mod, in, t, b);
    Check("baseline/parent_off_never_moves",
          b.passes == 0 && b.tried == 0 && t.invokes == 0 && mod.Moved() == 0 && !needs,
          "passes=" + N(b.passes) + " tried=" + N(b.tried) + " invokes=" + N(t.invokes) + " moved=" + N(mod.Moved())
              + " needs=" + N(needs));
}

static void TargetBagOnByDefaultAndKeptAcrossTheParentToggle()
{
    AutoProspectMod mod;
    const bool byDefault = mod.BagEnabled();
    mod.SetEnabled(true);
    mod.SetEnabled(false);
    const bool afterParentToggle = mod.BagEnabled();
    mod.SetBagEnabled(false);
    mod.SetEnabled(true);
    mod.SetEnabled(false);
    mod.SetEnabled(true);
    const bool offKept = !mod.BagEnabled();
    const bool onAgain = mod.SetBagEnabled(true) && mod.BagEnabled();
    const std::string line = mod.StatLine();
    Check("target/bag_on_by_default_and_kept_across_the_parent_toggle",
          byDefault && afterParentToggle && offKept && onAgain && line.find(" bag=on") != std::string::npos,
          "byDefault=" + N(byDefault) + " afterParentToggle=" + N(afterParentToggle) + " offKept=" + N(offKept)
              + " onAgain=" + N(onAgain) + " line=\"" + line + "\"");
}

static void TargetMovePassBeforeTheInvoke()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    SeedBatch(mod, {}, { { "m-0", true }, { "n-0", true } });                        // the previous batch, recorded
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "a-14", false } });
    const AutoProspectView moved = CellsView(kNode, { { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "o-0", true }, { "p-0", true } });
    BagFrame(mod, in, t, b, moved, MovedReport(), &after);
    Check("target/move_pass_before_the_invoke",
          b.passes == 1 && t.invokes == 1 && b.order == "move,move,invoke" && b.moves == "m-0,n-0" && mod.Moved() == 2,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " order=" + b.order + " moves=" + b.moves
              + " moved=" + N(mod.Moved()));
}

static void TargetNonMaterialNeverMoved()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    // A non-material inside the recorded batch, beside a material: the
    // adapter's flag is what keeps it in the grid.
    SeedBatch(mod, {}, { { "x-3", false }, { "m-0", true } });
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in = CellsView(kNode, { { "x-3", false }, { "m-0", true }, { "a-14", false } });
    const AutoProspectView moved = CellsView(kNode, { { "x-3", false }, { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "x-3", false }, { "o-0", true } });
    BagFrame(mod, in, t, b, moved, MovedReport(), &after);
    QuietFrame(mod, after, t, b);
    // Nothing but non-materials beside the insert: no pass at all, the invoke as before.
    AutoProspectMod only;
    only.SetEnabled(true);
    BagTally b2;
    SeedBatch(only, {}, { { "x-3", false } });
    only.OnInsert(kNode, true, false);
    const AutoProspectView in2 = CellsView(kNode, { { "x-3", false }, { "a-14", false } });
    BagFrame(only, in2, t, b2, in2, MovedReport(), &after);
    Check("target/non_material_never_moved",
          b.passes == 1 && b.moves == "m-0" && b2.passes == 0 && t.invokes == 2 && mod.Moved() == 1,
          "passes=" + N(b.passes + b2.passes) + " invokes=" + N(t.invokes) + " moves=" + b.moves + b2.moves
              + " moved=" + N(mod.Moved()));
}

static void TargetMovePassOnlyWhenAnInsertLands()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    const AutoProspectView mats = CellsView(kNode, { { "m-0", true }, { "n-0", true } });
    SeedBatch(mod, {}, { { "m-0", true }, { "n-0", true } });
    for (int i = 0; i < 5; ++i) QuietFrame(mod, mats, t, b);               // the batch sits there; no insert
    const bool quietNeeds = mod.NeedsMaterials();
    mod.OnInsert(kNode, true, false);                                       // a rearrangement: never lands
    for (int i = 0; i < ForgePact::kAutoProspectLandFrames + 2; ++i) QuietFrame(mod, mats, t, b);
    // Round 1: the expiry forgot the batch, so a prospect records it again.
    SeedBatch(mod, {}, { { "m-0", true }, { "n-0", true } });
    mod.OnInsert(kNode, true, false);                                       // a real insert, a frame late
    QuietFrame(mod, mats, t, b);
    const AutoProspectView in = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "a-14", false } });
    const AutoProspectView moved = CellsView(kNode, { { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "o-0", true } });
    BagFrame(mod, in, t, b, moved, MovedReport(), &after);
    for (int i = 0; i < 5; ++i) QuietFrame(mod, after, t, b);               // after the invoke: nothing
    mod.SetEnabled(false);                                                  // turning off never moves
    QuietFrame(mod, after, t, b);
    Check("target/move_pass_only_when_an_insert_lands",
          b.passes == 1 && !b.early && !quietNeeds && t.invokes == 1 && mod.Moved() == 2 && mod.NotLanded() == 1,
          "passes=" + N(b.passes) + " early=" + N(b.early || quietNeeds) + " invokes=" + N(t.invokes)
              + " moved=" + N(mod.Moved()));
}

static void TargetLandedInsertStaysLandedAcrossTheMovePass()
{
    // Five materials settled; the insert makes six; the pass takes the five
    // out, so the second read (one cell) is far below the settled count. That
    // is still the landed insert, not a not-landed one or a rearrangement.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    std::vector<Mat> five;
    for (int i = 0; i < 5; ++i) five.push_back({ "m" + N(i) + "-0", true });
    SeedBatch(mod, {}, five);
    mod.OnInsert(kNode, true, false);
    std::vector<Mat> six = five;
    six.push_back({ "a-14", false });
    const AutoProspectView moved = CellsView(kNode, { { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "o-0", true }, { "p-0", true }, { "q-0", true } });
    BagFrame(mod, CellsView(kNode, six), t, b, moved, MovedReport(), &after);
    const int settled = mod.Settled();
    for (int i = 0; i < ForgePact::kAutoProspectLandFrames + 2; ++i) QuietFrame(mod, after, t, b);
    Check("target/landed_insert_stays_landed_across_the_move_pass",
          b.passes == 1 && t.invokes == 1 && mod.NotLanded() == 0 && settled == 3 && mod.Moved() == 5 && t.refusals == 0,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " notLanded=" + N(mod.NotLanded())
              + " settled=" + N(settled) + " moved=" + N(mod.Moved()));
}

static void TargetRefusedMoveLeavesTheMaterialAndIsLoggedOnce()
{
    // The game would not stack it, then did not confirm the add, then a cell
    // no longer held what the view saw: the material stays each time, the
    // prospect still runs, and each reason is named once.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    SeedBatch(mod, {}, { { "m-0", true } });
    const AutoProspectView in = CellsView(kNode, { { "m-0", true }, { "a-14", false } });
    // That prospect's batch is o and p; m stays behind and is not named again.
    const AutoProspectView after = CellsView(kNode, { { "m-0", true }, { "o-0", true }, { "p-0", true } });
    mod.OnInsert(kNode, true, false);
    BagFrame(mod, in, t, b, in, Report(false, false, 1), &after);           // not-stackable
    const AutoProspectView in2 = CellsView(kNode, { { "m-0", true }, { "o-0", true }, { "p-0", true }, { "b-14", false } });
    const AutoProspectView after2 = CellsView(kNode, { { "m-0", true }, { "o-0", true }, { "p-0", true }, { "q-0", true } });
    mod.OnInsert(kNode, true, false);
    BagFrame(mod, in2, t, b, in2, Report(true, false, 1), &after2);         // not-added, twice
    ForgePact::AutoProspectMoveReport stale = MovedReport();
    stale.heldBefore = false;                                               // the cell changed before the first call
    mod.OnMoveReport(stale);
    mod.OnMoveReport(Report(false, false, 1));                              // not-stackable again
    const auto first = mod.TakeFirstMoveProblem();
    const auto second = mod.TakeFirstMoveProblem();
    const auto third = mod.TakeFirstMoveProblem();
    const auto fourth = mod.TakeFirstMoveProblem();
    const std::string line = mod.MoveProblemLine(ForgePact::AutoProspectMoveOutcome::NotStackable);
    using O = ForgePact::AutoProspectMoveOutcome;
    Check("target/refused_move_leaves_the_material_and_is_logged_once",
          b.passes == 2 && t.invokes == 2 && mod.MoveOutcomes(O::NotStackable) == 2 && mod.MoveOutcomes(O::NotAdded) == 2
              && mod.MoveOutcomes(O::MoveFailed) == 1 && mod.Moved() == 0
              && first == O::NotStackable && second == O::NotAdded && third == O::MoveFailed && fourth == O::None
              && mod.MovePassOn() && line.rfind("autoprospect: not-stackable - ", 0) == 0
              && line.find("stays in the grid") != std::string::npos,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " notStackable=" + N(mod.MoveOutcomes(O::NotStackable))
              + " notAdded=" + N(mod.MoveOutcomes(O::NotAdded)) + " moveFailed=" + N(mod.MoveOutcomes(O::MoveFailed))
              + " first=" + N((int)first) + " second=" + N((int)second) + " third=" + N((int)third)
              + " fourth=" + N((int)fourth) + " bagOn=" + N(mod.MovePassOn()) + " line=\"" + line + "\"");
}

// Vanished and cell-kept share their shape: the first such cell stops the
// pass, the prospect still runs, the pass stays off for the session (no
// sub-option or parent toggle brings it back), and the line says so.
static bool TurnsOffForTheSession(const ForgePact::AutoProspectMoveReport& report,
                                  ForgePact::AutoProspectMoveOutcome outcome, const std::string& name, std::string& detail)
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    SeedBatch(mod, {}, { { "m-0", true }, { "n-0", true } });
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "a-14", false } });
    const AutoProspectView moved = CellsView(kNode, { { "n-0", true }, { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "n-0", true }, { "o-0", true } });
    BagFrame(mod, in, t, b, moved, report, &after);
    const auto first = mod.TakeFirstMoveProblem();
    const std::string line = mod.MoveProblemLine(outcome);
    mod.SetEnabled(false);
    mod.SetEnabled(true);
    const bool reenable = mod.SetBagEnabled(true);
    QuietFrame(mod, after, t, b);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in2 = CellsView(kNode, { { "n-0", true }, { "o-0", true }, { "b-14", false } });
    BagFrame(mod, in2, t, b, in2, MovedReport(), &after);
    const bool stat = mod.StatLine().find(" bag=off-this-session") != std::string::npos
        && mod.StatLine().find(" " + name + "=1") != std::string::npos;
    detail = "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " tried=" + N(b.tried) + " " + name + "="
        + N(mod.MoveOutcomes(outcome)) + " first=" + N((int)first) + " reenable=" + N(reenable)
        + " line=\"" + line + "\" stat=" + N(stat);
    return b.passes == 1 && b.tried == 1 && t.invokes == 2 && mod.MoveOutcomes(outcome) == 1 && first == outcome
        && !reenable && !mod.MovePassOn() && !mod.NeedsMaterials()
        && line.rfind("autoprospect: " + name + " - ", 0) == 0 && line.find("off for this session") != std::string::npos
        && stat;
}

static void TargetVanishedTurnsTheMovePassOffForTheSession()
{
    // The cell lost its fingerprint and the add did not report success.
    std::string detail;
    const bool ok = TurnsOffForTheSession(Report(true, false, 0), ForgePact::AutoProspectMoveOutcome::Vanished,
                                          "vanished", detail);
    Check("target/vanished_turns_the_move_pass_off_for_the_session", ok, detail);
}

static void TargetCellKeptAfterAddTurnsTheMovePassOffForTheSession()
{
    // The add reported success and the clear ran, but the cell still holds
    // the material: a possible duplicate, as wrong as a loss.
    std::string detail;
    const bool ok = TurnsOffForTheSession(Report(true, true, 1), ForgePact::AutoProspectMoveOutcome::CellKept,
                                          "cell-kept", detail);
    Check("target/cell_kept_after_add_turns_the_move_pass_off_for_the_session", ok, detail);
}

static void TargetFirstMoveReportedOnceInThePlayersLog()
{
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    SeedBatch(mod, {}, { { "m-0", true }, { "n-0", true } });
    const bool early = mod.TakeFirstMove();
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "o-0", true }, { "p-0", true },
                                                      { "q-0", true } });
    BagFrame(mod, in, t, b, in, Report(false, false, 1), &after);           // a pass that moved nothing
    const bool afterNothingMoved = mod.TakeFirstMove();
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in2 = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "o-0", true }, { "p-0", true },
                                                    { "q-0", true }, { "b-14", false } });
    const AutoProspectView moved2 = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "b-14", false } });
    const AutoProspectView after2 = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "r-0", true } });
    BagFrame(mod, in2, t, b, moved2, MovedReport(), &after2);               // the batch o, p, q
    const bool first = mod.TakeFirstMove();
    const std::string line = mod.FirstMoveLine();
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in3 = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "r-0", true }, { "c-14", false } });
    BagFrame(mod, in3, t, b, CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "c-14", false } }), MovedReport(),
             &after2);                                                      // the batch r
    const bool second = mod.TakeFirstMove();
    Check("target/first_move_reported_once_in_the_players_log",
          !early && !afterNothingMoved && first && !second && mod.Moved() == 4
              && line.rfind("autoprospect: first move to bag - ", 0) == 0 && line.find(" moved=3") != std::string::npos,
          "early=" + N(early) + " afterNothingMoved=" + N(afterNothingMoved) + " first=" + N(first)
              + " second=" + N(second) + " moved=" + N(mod.Moved()) + " line=\"" + line + "\"");
}

// ---- Stage C round 1: the move set is the recorded batch --------------------
//
// Phase 3 live on e63eed5 (research doc, § Stage C Phase 3 results): with two
// materials in the grid the human inserted ONE ORE, and the pass moved three
// cells - the two materials and the ore itself, which is a material too - so
// the ore went straight back to the materials tab and was never prospected
// (`moved` 3->6, `ran-no-effect=1`, ore count unchanged by eye). "Material"
// names a kind of item, and the insert can be that kind. The previous batch is
// identified by what it is instead: the fingerprints the core's own invoke
// produced, read in the invoke's frame (OnInvoked's `after` against the view
// the invoking Decide saw). Anything the core cannot account for forgets it,
// and a forgotten batch moves nothing - the materials just stay.
//
// Observed 2026-09-19 against the round-0 core (e63eed5), unchanged but for the
// one new name the scenarios need, shimmed inert (the view's `printList`,
// which that core never reads):
//   FAIL target/first_prospect_of_a_session_moves_nothing passes=1 invokes=1 needs=1 moves=m-0,n-0 moved=2
//   FAIL target/ore_insert_moves_only_the_previous_batch passes=1 invokes=1 moves=ore-0,p-0,o-0 moved=3 batchBefore=0
//   FAIL target/hand_placed_material_is_never_moved passes=1 invokes=1 moves=h-0,o-0 moved=3
//   FAIL target/batch_forgotten_on_a_new_node_or_the_parent_toggle passes=3 invokes=3 moves=o-0,o-0,o-0 moved=3
//   FAIL target/batch_forgotten_after_a_removal_or_an_unlanded_insert passes=2 invokes=2 moves=o-0,o-0,p-0 notLanded=1 moved=3
//   FAIL target/a_batch_is_moved_at_most_once passes=2 tried=4 moves=o-0,p-0,o-0,p-0 invokes=2 notStackable=4 fullPasses=2 fullMoves=o-0,o-0
//   FAIL target/success_with_an_unreadable_cell_turns_the_move_pass_off outcome=4 cellKept=0 passOn=1 line="autoprospect: cell-kept - the game confirmed the move but the material is still in the grid; moving materials to the bag is off for this session"
// (hand_placed's moved=3: that core's pass during the seeding prospect had
// already moved h-0 once; outcome=4 is move-failed.) Every one of these
// failed on the cells the pass named, which is the defect itself.

static void TargetFirstProspectOfASessionMovesNothing()
{
    // Materials put in by hand, the parent just turned on, then an insert:
    // there is no recorded batch, so no pass and no item lookup at all.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    QuietFrame(mod, CellsView(kNode, { { "m-0", true }, { "n-0", true } }), t, b);
    mod.OnInsert(kNode, true, false);
    const bool needs = mod.NeedsMaterials();
    const AutoProspectView in = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "m-0", true }, { "n-0", true }, { "o-0", true } });
    BagFrame(mod, in, t, b, in, MovedReport(), &after);
    Check("target/first_prospect_of_a_session_moves_nothing",
          b.passes == 0 && t.invokes == 1 && !needs && mod.Moved() == 0,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " needs=" + N(needs) + " moves=" + b.moves
              + " moved=" + N(mod.Moved()));
}

static void TargetOreInsertMovesOnlyThePreviousBatch()
{
    // The live defect: a prospect recorded {o, p}; the player inserts an ore,
    // which the adapter flags as a material like the batch, and the batch sits
    // at new positions. Only o and p move; the ore is prospected.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    SeedBatch(mod, {}, { { "o-0", true }, { "p-0", true } });
    const std::string stat = mod.StatLine();
    const bool batchBefore = stat.find(" batch=2") != std::string::npos;
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in = CellsView(kNode, { { "ore-0", true }, { "p-0", true }, { "o-0", true } });
    const AutoProspectView moved = CellsView(kNode, { { "ore-0", true } });
    const AutoProspectView after = CellsView(kNode, { { "q-0", true }, { "r-0", true } });
    BagFrame(mod, in, t, b, moved, MovedReport(), &after);
    Check("target/ore_insert_moves_only_the_previous_batch",
          b.passes == 1 && t.invokes == 1 && b.moves == "p-0,o-0" && mod.Moved() == 2 && batchBefore,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " moves=" + b.moves + " moved=" + N(mod.Moved())
              + " batchBefore=" + N(batchBefore));
}

static void TargetHandPlacedMaterialIsNeverMoved()
{
    // A material already in the grid before the recording invoke sits beside
    // the batch that invoke produced. Only the batch moves.
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    SeedBatch(mod, { { "h-0", true } }, { { "o-0", true } });
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in = CellsView(kNode, { { "h-0", true }, { "o-0", true }, { "a-14", false } });
    const AutoProspectView moved = CellsView(kNode, { { "h-0", true }, { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "h-0", true }, { "p-0", true } });
    BagFrame(mod, in, t, b, moved, MovedReport(), &after);
    Check("target/hand_placed_material_is_never_moved",
          b.passes == 1 && t.invokes == 1 && b.moves == "o-0" && mod.Moved() == 1,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " moves=" + b.moves + " moved=" + N(mod.Moved()));
}

static void TargetBatchForgottenOnANewNodeOrTheParentToggle()
{
    // After a new node id (the window reopened), after a frame with no window,
    // and after the parent is toggled, the next insert moves nothing.
    Tally t;
    BagTally b;
    long moved = 0;
    const std::vector<Mat> left = { { "o-0", true } };
    const std::vector<Mat> withItem = { { "o-0", true }, { "a-14", false } };
    {
        AutoProspectMod mod;
        mod.SetEnabled(true);
        SeedBatch(mod, {}, left);
        QuietFrame(mod, CellsView(kOtherNode, left), t, b);
        mod.OnInsert(kOtherNode, true, false);
        const AutoProspectView in = CellsView(kOtherNode, withItem);
        BagFrame(mod, in, t, b, CellsView(kOtherNode, { { "a-14", false } }), MovedReport(), &in);
        moved += mod.Moved();
    }
    {
        AutoProspectMod mod;
        mod.SetEnabled(true);
        SeedBatch(mod, {}, left);
        QuietFrame(mod, AutoProspectView(), t, b);                        // no window this frame
        QuietFrame(mod, CellsView(kNode, left), t, b);
        mod.OnInsert(kNode, true, false);
        const AutoProspectView in = CellsView(kNode, withItem);
        BagFrame(mod, in, t, b, CellsView(kNode, { { "a-14", false } }), MovedReport(), &in);
        moved += mod.Moved();
    }
    {
        AutoProspectMod mod;
        mod.SetEnabled(true);
        SeedBatch(mod, {}, left);
        mod.SetEnabled(false);
        mod.SetEnabled(true);
        QuietFrame(mod, CellsView(kNode, left), t, b);
        mod.OnInsert(kNode, true, false);
        const AutoProspectView in = CellsView(kNode, withItem);
        BagFrame(mod, in, t, b, CellsView(kNode, { { "a-14", false } }), MovedReport(), &in);
        moved += mod.Moved();
    }
    Check("target/batch_forgotten_on_a_new_node_or_the_parent_toggle",
          b.passes == 0 && t.invokes == 3 && moved == 0,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " moves=" + b.moves + " moved=" + N(moved));
}

static void TargetBatchForgottenAfterARemovalOrAnUnlandedInsert()
{
    // The player took a stack out (the count dips with nothing pending), or an
    // insert expired not-landed (a rearrangement or a swap): the core cannot
    // account for the grid any more, so the next insert moves nothing.
    Tally t;
    BagTally b;
    long moved = 0, notLanded = 0;
    const std::vector<Mat> batch = { { "o-0", true }, { "p-0", true } };
    {
        AutoProspectMod mod;
        mod.SetEnabled(true);
        SeedBatch(mod, {}, batch);
        QuietFrame(mod, CellsView(kNode, { { "o-0", true } }), t, b);   // p-0 taken out
        mod.OnInsert(kNode, true, false);
        const AutoProspectView in = CellsView(kNode, { { "o-0", true }, { "a-14", false } });
        BagFrame(mod, in, t, b, CellsView(kNode, { { "a-14", false } }), MovedReport(), &in);
        moved += mod.Moved();
    }
    {
        AutoProspectMod mod;
        mod.SetEnabled(true);
        SeedBatch(mod, {}, batch);
        mod.OnInsert(kNode, true, false);                                // never lands
        for (int i = 0; i < ForgePact::kAutoProspectLandFrames + 2; ++i) QuietFrame(mod, CellsView(kNode, batch), t, b);
        mod.OnInsert(kNode, true, false);
        const AutoProspectView in = CellsView(kNode, { { "o-0", true }, { "p-0", true }, { "a-14", false } });
        BagFrame(mod, in, t, b, CellsView(kNode, { { "a-14", false } }), MovedReport(), &in);
        moved += mod.Moved();
        notLanded += mod.NotLanded();
    }
    Check("target/batch_forgotten_after_a_removal_or_an_unlanded_insert",
          b.passes == 0 && t.invokes == 2 && notLanded == 1 && moved == 0 && !b.early,
          "passes=" + N(b.passes) + " invokes=" + N(t.invokes) + " moves=" + b.moves + " notLanded=" + N(notLanded)
              + " moved=" + N(moved));
}

static void TargetABatchIsMovedAtMostOnce()
{
    // A pass the game refused (not-stackable) leaves the cells where they are;
    // the next insert, whose prospect produced nothing new, names none of them
    // again. Round 0 retried a refused cell on every insert.
    using O = ForgePact::AutoProspectMoveOutcome;
    AutoProspectMod mod;
    mod.SetEnabled(true);
    Tally t;
    BagTally b;
    SeedBatch(mod, {}, { { "o-0", true }, { "p-0", true } });
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in = CellsView(kNode, { { "o-0", true }, { "p-0", true }, { "a-14", false } });
    const AutoProspectView after = CellsView(kNode, { { "o-0", true }, { "p-0", true } });   // nothing new
    BagFrame(mod, in, t, b, in, Report(false, false, 1), &after);
    QuietFrame(mod, after, t, b);
    mod.OnInsert(kNode, true, false);
    const AutoProspectView in2 = CellsView(kNode, { { "o-0", true }, { "p-0", true }, { "b-14", false } });
    BagFrame(mod, in2, t, b, in2, Report(false, false, 1), &after);
    // The same with no invoke in between: the pass is followed by a grid-full
    // refusal, and the next insert still does not name the refused cell.
    AutoProspectMod full;
    full.SetEnabled(true);
    Tally t2;
    BagTally b2;
    SeedBatch(full, {}, { { "o-0", true } });
    full.OnInsert(kNode, true, false);
    AutoProspectView fin = CellsView(kNode, { { "o-0", true }, { "a-14", false } });
    fin.empty = ForgePact::kAutoProspectMinFreeCells - 1;
    BagFrame(full, fin, t2, b2, fin, Report(false, false, 1));
    full.OnInsert(kNode, true, false);
    AutoProspectView fin2 = CellsView(kNode, { { "o-0", true }, { "a-14", false }, { "b-14", false } });
    fin2.empty = ForgePact::kAutoProspectMinFreeCells - 2;
    BagFrame(full, fin2, t2, b2, fin2, Report(false, false, 1));
    Check("target/a_batch_is_moved_at_most_once",
          b.passes == 1 && b.tried == 2 && b.moves == "o-0,p-0" && t.invokes == 2 && mod.MoveOutcomes(O::NotStackable) == 2
              && b2.passes == 1 && b2.moves == "o-0" && t2.refusals == 2,
          "passes=" + N(b.passes) + " tried=" + N(b.tried) + " moves=" + b.moves + " invokes=" + N(t.invokes)
              + " notStackable=" + N(mod.MoveOutcomes(O::NotStackable)) + " fullPasses=" + N(b2.passes)
              + " fullMoves=" + b2.moves);
}

static void TargetSuccessWithAnUnreadableCellTurnsTheMovePassOff()
{
    // The add said success and the final read could not show the cell: the
    // grid cannot show the material gone, so it is cell-kept (a possible
    // duplicate) and the pass turns off. Its line must be true of both
    // cell-kept cases, so it does not claim the material is still in the grid.
    using O = ForgePact::AutoProspectMoveOutcome;
    AutoProspectMod mod;
    mod.SetEnabled(true);
    const O outcome = mod.OnMoveReport(Report(true, true, -1));
    const std::string line = mod.MoveProblemLine(O::CellKept);
    Check("target/success_with_an_unreadable_cell_turns_the_move_pass_off",
          outcome == O::CellKept && mod.MoveOutcomes(O::CellKept) == 1 && !mod.MovePassOn()
              && line.find("still in the grid") == std::string::npos && line.find("off for this session") != std::string::npos,
          "outcome=" + N((int)outcome) + " cellKept=" + N(mod.MoveOutcomes(O::CellKept)) + " passOn=" + N(mod.MovePassOn())
              + " line=\"" + line + "\"");
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
    BaselineProspectsWithEffectLogNoNothingHappenedLine();
    TargetRanNoEffectReportedOnceInThePlayersLog();
    TargetUnverifiedReportedOnceInThePlayersLog();
    AdapterRecordedShapeNames();
    AdapterMeasuredSessionProspectsEachInsertOnce();
    BaselineBagOffNeverMoves();
    BaselineParentOffNeverMoves();
    TargetBagOnByDefaultAndKeptAcrossTheParentToggle();
    TargetMovePassBeforeTheInvoke();
    TargetNonMaterialNeverMoved();
    TargetMovePassOnlyWhenAnInsertLands();
    TargetLandedInsertStaysLandedAcrossTheMovePass();
    TargetRefusedMoveLeavesTheMaterialAndIsLoggedOnce();
    TargetVanishedTurnsTheMovePassOffForTheSession();
    TargetCellKeptAfterAddTurnsTheMovePassOffForTheSession();
    TargetFirstMoveReportedOnceInThePlayersLog();
    TargetFirstProspectOfASessionMovesNothing();
    TargetOreInsertMovesOnlyThePreviousBatch();
    TargetHandPlacedMaterialIsNeverMoved();
    TargetBatchForgottenOnANewNodeOrTheParentToggle();
    TargetBatchForgottenAfterARemovalOrAnUnlandedInsert();
    TargetABatchIsMovedAtMostOnce();
    TargetSuccessWithAnUnreadableCellTurnsTheMovePassOff();
    std::cout << (g_Failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return g_Failures ? 1 : 0;
}
