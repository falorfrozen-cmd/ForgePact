// Behavioral harness for the prospect window sizing core (ForgePact issue #9).
//
// The Python runner splices the REAL plugin/include/ForgePact/ProspectWindowMod.hpp
// in at the marker below (its #pragma and #include lines removed), so every
// factor and cap here is read from the header's own declarations and no
// number is restated. The header is game-independent by contract - it names no
// runtime interface at all - which is what lets it compile here with no
// runtime stub.
//
// Stage A: the mechanism that sizes the window is not yet measured
// (docs/prospect-window-research.md, Phase 0 pending), so these scenarios pin
// only the decision: off reproduces vanilla and applies nothing; on scales by
// the declared factor, clamps to the declared cap, never shrinks, applies once
// per window instance, refuses nonsense and says what it did. Stage B adds the
// adapter/* scenarios for whichever shape the live session establishes.
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

// PRODUCTION_PROSPECTWINDOW

using ForgePact::GridSize;
using ForgePact::ProspectWindowMod;

static int g_Failures = 0;

static void Check(const std::string& label, bool ok, const std::string& detail)
{
    if (ok) std::cout << "PASS " << label << "\n";
    else { std::cout << "FAIL " << label << " " << detail << "\n"; ++g_Failures; }
}

static std::string Str(GridSize g) { return std::to_string(g.cols) + "x" + std::to_string(g.rows); }
static bool Same(GridSize a, GridSize b) { return a.cols == b.cols && a.rows == b.rows; }

// What the declared constants say a vanilla size should become - computed
// from the header's constants, never from a literal, so a Stage B retune does
// not change what these scenarios mean.
static GridSize Expected(GridSize v)
{
    auto axis = [](int vanilla, int factor, int cap) {
        long long scaled = (long long)vanilla * factor;
        long long capped = std::min<long long>(scaled, cap);
        return (int)std::max<long long>(capped, vanilla);
    };
    return { axis(v.cols, ForgePact::kProspectColsFactor, ForgePact::kProspectMaxCols),
             axis(v.rows, ForgePact::kProspectRowsFactor, ForgePact::kProspectMaxRows) };
}

// ---- baseline: the mod off is vanilla ------------------------------------

static void BaselineOffDecideIsIdentity()
{
    ProspectWindowMod mod;
    const GridSize vanilla{ 4, 4 };
    GridSize out{ -1, -1 };
    const bool changed = mod.Decide(vanilla, out);
    Check("baseline/off_decide_is_identity", !changed && Same(out, vanilla),
          "returned=" + std::to_string(changed) + " out=" + Str(out) + " want=" + Str(vanilla));
}

static void BaselineOffNeverApplies()
{
    ProspectWindowMod mod;
    const bool first = mod.ShouldApplyTo(100);
    const bool second = mod.ShouldApplyTo(101);
    Check("baseline/off_never_applies", !first && !second && mod.Applied() == 0,
          "first=" + std::to_string(first) + " second=" + std::to_string(second)
          + " applied=" + std::to_string(mod.Applied()));
}

// ---- target: the mod on --------------------------------------------------
//
// Observed failing 2026-09-17 against a stub core that declared the constants
// but returned vanilla for everything (Decide -> false, ShouldApplyTo -> false,
// counters 0, empty StatLine), before Decide/ShouldApplyTo were written:
//   FAIL target/on_scales_by_factor returned=0 got=3x2 want=6x4
//   FAIL target/on_caps_at_max returned=0 got=23x23 want=24x24
//   FAIL target/once_per_instance first=0 again=0 other=0 skippedAlready=0
//   FAIL target/latch_is_bounded oldestAgain=0 newestAgain=0 size=0
//   FAIL target/invalid_vanilla_refused_and_counted ra=0 rb=0 a=0x4 b=4x-1 refusedInvalid=0
//   FAIL target/disable_clears_latch again=0 skippedAlready=0 applied=0 (counters must survive a disable)
//   FAIL target/statline_names_what_it_did line="" off=""
// `target/on_never_shrinks` PASSED against that stub - identity never shrinks
// anything. It is a bound on the cap, not a witness that scaling works; the
// two scenarios above it are the witnesses.

static void TargetOnScalesByFactor()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    const GridSize vanilla{ 3, 2 };
    GridSize out{ -1, -1 };
    const bool changed = mod.Decide(vanilla, out);
    const GridSize want = Expected(vanilla);
    const bool scaled = out.cols == 3 * ForgePact::kProspectColsFactor && out.rows == 2 * ForgePact::kProspectRowsFactor;
    Check("target/on_scales_by_factor", changed && Same(out, want) && scaled,
          "returned=" + std::to_string(changed) + " got=" + Str(out) + " want=" + Str(want));
}

static void TargetOnCapsAtMax()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    const GridSize vanilla{ ForgePact::kProspectMaxCols - 1, ForgePact::kProspectMaxRows - 1 };
    GridSize out{ -1, -1 };
    const bool changed = mod.Decide(vanilla, out);
    Check("target/on_caps_at_max",
          changed && out.cols == ForgePact::kProspectMaxCols && out.rows == ForgePact::kProspectMaxRows,
          "returned=" + std::to_string(changed) + " got=" + Str(out) + " want="
          + std::to_string(ForgePact::kProspectMaxCols) + "x" + std::to_string(ForgePact::kProspectMaxRows));
}

static void TargetOnNeverShrinks()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    // A vanilla grid already above the cap: the cap must not make it smaller.
    const GridSize vanilla{ ForgePact::kProspectMaxCols + 5, ForgePact::kProspectMaxRows + 3 };
    GridSize out{ -1, -1 };
    mod.Decide(vanilla, out);
    Check("target/on_never_shrinks", out.cols >= vanilla.cols && out.rows >= vanilla.rows,
          "got=" + Str(out) + " vanilla=" + Str(vanilla));
}

static void TargetOncePerInstance()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    const bool first = mod.ShouldApplyTo(7);
    const bool again = mod.ShouldApplyTo(7);
    const bool other = mod.ShouldApplyTo(8);
    Check("target/once_per_instance", first && !again && other && mod.SkippedAlready() == 1,
          "first=" + std::to_string(first) + " again=" + std::to_string(again)
          + " other=" + std::to_string(other) + " skippedAlready=" + std::to_string(mod.SkippedAlready()));
}

static void TargetLatchIsBounded()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    // Fill the latch past its bound; the oldest id is evicted, so it applies
    // again, while a recent one is still remembered.
    const int64_t n = (int64_t)ForgePact::kProspectLatchCapacity + 1;
    for (int64_t id = 1; id <= n; ++id) mod.ShouldApplyTo(id);
    const bool oldestAgain = mod.ShouldApplyTo(1);
    const bool newestAgain = mod.ShouldApplyTo(n);
    Check("target/latch_is_bounded", oldestAgain && !newestAgain && mod.LatchSize() <= ForgePact::kProspectLatchCapacity,
          "oldestAgain=" + std::to_string(oldestAgain) + " newestAgain=" + std::to_string(newestAgain)
          + " size=" + std::to_string(mod.LatchSize()));
}

static void TargetInvalidVanillaRefusedAndCounted()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    GridSize a{ -1, -1 }, b{ -1, -1 };
    const bool ra = mod.Decide({ 0, 4 }, a);
    const bool rb = mod.Decide({ 4, -1 }, b);
    Check("target/invalid_vanilla_refused_and_counted",
          !ra && !rb && Same(a, { 0, 4 }) && Same(b, { 4, -1 }) && mod.RefusedInvalid() == 2,
          "ra=" + std::to_string(ra) + " rb=" + std::to_string(rb) + " a=" + Str(a) + " b=" + Str(b)
          + " refusedInvalid=" + std::to_string(mod.RefusedInvalid()));
}

static void TargetDisableClearsLatch()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    mod.ShouldApplyTo(7);
    mod.ShouldApplyTo(7);                    // skippedAlready = 1
    mod.RecordApplied({ 4, 4 }, Expected({ 4, 4 }));
    mod.SetEnabled(false);
    mod.SetEnabled(true);
    const bool again = mod.ShouldApplyTo(7);
    Check("target/disable_clears_latch",
          again && mod.SkippedAlready() == 1 && mod.Applied() == 1,
          "again=" + std::to_string(again) + " skippedAlready=" + std::to_string(mod.SkippedAlready())
          + " applied=" + std::to_string(mod.Applied()) + " (counters must survive a disable)");
}

static void TargetStatlineNamesWhatItDid()
{
    ProspectWindowMod mod;
    mod.SetEnabled(true);
    const GridSize vanilla{ 4, 4 };
    GridSize out{ -1, -1 };
    mod.Decide(vanilla, out);
    if (mod.ShouldApplyTo(42)) mod.RecordApplied(vanilla, out);
    mod.ShouldApplyTo(42);
    GridSize junk{};
    mod.Decide({ 0, 0 }, junk);
    const std::string line = mod.StatLine();
    const std::string last = "last=" + Str(vanilla) + "->" + Str(out);
    const bool ok = line.rfind("prospectsize: ON ", 0) == 0
        && line.find("applied=1") != std::string::npos
        && line.find(last) != std::string::npos
        && line.find("skipped(already=1 disabled=0)") != std::string::npos
        && line.find("refused(invalid=1)") != std::string::npos;
    mod.SetEnabled(false);
    const bool offOk = mod.StatLine().rfind("prospectsize: off ", 0) == 0;
    Check("target/statline_names_what_it_did", ok && offOk, "line=\"" + line + "\" off=\"" + mod.StatLine() + "\"");
}

int main()
{
    BaselineOffDecideIsIdentity();
    BaselineOffNeverApplies();
    TargetOnScalesByFactor();
    TargetOnCapsAtMax();
    TargetOnNeverShrinks();
    TargetOncePerInstance();
    TargetLatchIsBounded();
    TargetInvalidVanillaRefusedAndCounted();
    TargetDisableClearsLatch();
    TargetStatlineNamesWhatItDid();
    std::cout << (g_Failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return g_Failures ? 1 : 0;
}
