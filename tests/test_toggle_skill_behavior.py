"""Run the real toggle-skill indicator read against a controlled game API.

Companion to toggle_skill_harness.cpp and test_toggle_skill_contract.py (which
asserts on source text). This file proves the READ ITSELF - what
ToggleIndicatorRead() and ToggleIndicatorModel::Decide() decide from a
counted enumeration - end to end, before any drawing code exists (P1b;
issue #11, Track B). Ownership is decided from each scanned instance's own
`isMyClient`, not by comparing against the local player: session 3 measured
that `Player_obj` has no `playerNumber` at all (docs/toggle-skills-research.md,
"Co-op / ownership after session 3: isMyClient"). A second pass over the same
evidence - `ToggleIndicatorModel::Decide(detail, requireMarker=true)` - is the
marker-required decision session 4 controls ("Plain-cast flash (R10) and the
Purgatory marker").
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def implementation(source, signature):
    """The full text of `signature`'s definition (last occurrence wins)."""
    start = source.rfind(signature)
    if start < 0:
        raise AssertionError(f"not found: {signature}")
    brace = source.index("{", start)
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated: {signature}")


def declaration(source, prefix):
    """One whole single-line declaration, so its value is never restated here."""
    for line in source.split("\n"):
        if line.strip().startswith(prefix):
            return line
    raise AssertionError(f"not found: {prefix}")


class ToggleSkillBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")
        header = (ROOT / "plugin/include/ForgePact/ToggleSkillMod.hpp").read_text(encoding="utf-8")
        skilltimer_header = (ROOT / "plugin/include/ForgePact/SkillTimerMod.hpp").read_text(encoding="utf-8")

        # The real class/struct/enum, verbatim, minus the include of
        # Common.hpp (the harness supplies the stand-ins Common.hpp would
        # have pulled in) - same shape as test_relic_filter_behavior.py.
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        # Issue #55: SkillTimerMod.hpp, spliced the same way.
        skilltimer_klass = "\n".join(
            line for line in skilltimer_header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        constants = "\n".join([
            declaration(cls.plugin, "static constexpr int kToggleIndicatorScanCap"),
            declaration(cls.plugin, "static constexpr int kToggleIndicatorTalentId"),
            # S (the shipped marker, D-U12/D-U13): the band count, the deepred
            # triple and the derived-box offset, spliced rather than restated
            # so a scenario asserts against the shipped numbers themselves.
            # The ` =` keeps `kToggleMarkerB` from matching `kToggleMarkerBoxDX`,
            # which is an earlier line with the same prefix.
            declaration(cls.plugin, "static constexpr int kToggleMarkerBands ="),
            declaration(cls.plugin, "static constexpr double kToggleMarkerR ="),
            declaration(cls.plugin, "static constexpr double kToggleMarkerG ="),
            declaration(cls.plugin, "static constexpr double kToggleMarkerB ="),
            declaration(cls.plugin, "static constexpr double kToggleMarkerBoxDX ="),
            declaration(cls.plugin, "static constexpr double kToggleMarkerBoxDY ="),
            declaration(cls.plugin, "static constexpr double kToggleMarkerBoxDW ="),
            declaration(cls.plugin, "static constexpr double kToggleMarkerBoxDH ="),
            # Issue #55 (skilltimer): the four looks' own constants, pinned
            # equal to the research instrument's defaults by
            # test_toggle_skill_contract.py, spliced here the same way so a
            # scenario asserts against the shipped numbers themselves.
            declaration(cls.plugin, "static constexpr double kSkillTimerColourR ="),
            declaration(cls.plugin, "static constexpr int kSkillTimerBands ="),
            declaration(cls.plugin, "static constexpr double kSkillTimerBarGap ="),
            declaration(cls.plugin, "static constexpr double kSkillTimerTextOffsetDx ="),
        ])
        production = "\n".join([
            # S: the row-parameterised read and its row-0 aliases, in the
            # order the plugin defines them (the discriminator helper leans on
            # ToggleIndicatorReadTruth).
            implementation(cls.plugin, "static bool ToggleIndicatorResolveRowObject("),
            implementation(cls.plugin, "static bool ToggleIndicatorResolveAoeObject("),
            implementation(cls.plugin, "static bool ToggleIndicatorReadTruth("),
            implementation(cls.plugin, "static void ToggleIndicatorCountMark("),
            implementation(cls.plugin, "static ForgePact::ToggleIndicatorState ToggleIndicatorReadRow("),
            implementation(cls.plugin, "static ForgePact::ToggleIndicatorState ToggleIndicatorRead("),
            # S: the runtime-resolved talent ids the border and the guard key
            # off. The walk itself is not spliced (it needs the talent-map
            # helpers); test_toggle_skill_contract.py pins that.
            implementation(cls.plugin, "struct ToggleTableIds {") + ";",
            declaration(cls.plugin, "static ToggleTableIds g_ToggleTableIds"),
            declaration(cls.plugin, "static volatile long g_ToggleResolveWalks"),
            implementation(cls.plugin, "static int ToggleTableUnresolvedRows("),
            implementation(cls.plugin, "static int ToggleTableRowForTalentId("),
            implementation(cls.plugin, "static std::string ToggleTableRowsLine("),
            # Session 8: the countdown's own table's ids, filled by the same
            # walk (not spliced, as above), so a scenario can resolve a
            # countdown row independently of the toggle table.
            implementation(cls.plugin, "struct SkillTimerTableIds {") + ";",
            declaration(cls.plugin, "static SkillTimerTableIds g_SkillTimerTableIds"),
            implementation(cls.plugin, "static int SkillTimerTableUnresolvedRows("),
            # P2 (the shipped indicator): the draw itself, and the slot
            # lookup it calls. `g_ToggleBorderOn`/the counters are plain
            # globals, spliced verbatim so a scenario can drive/inspect them
            # the same way it drives `world` - toggleborder is off by
            # default, same as in the plugin.
            declaration(cls.plugin, "static std::atomic<bool> g_ToggleBorderOn"),
            declaration(cls.plugin, "static volatile long g_TibDrawn"),
            # S (the review follow-up): the per-row counters and the line that
            # names a row by its own `abilityId`, so a scenario can assert
            # which row an outcome was charged to.
            implementation(cls.plugin, "struct ToggleBorderRowCounters {") + ";",
            declaration(cls.plugin, "static ToggleBorderRowCounters g_TibRow"),
            implementation(cls.plugin, "static std::string ToggleBorderRowCountersLine("),
            implementation(cls.plugin, "static void ToggleIndicatorMarkerBox("),
            # The shared slot lookup's failure reason (issue #55 follow-up):
            # reported to the caller rather than counted inside FindSlot
            # itself, so toggleborder and skilltimer each charge their own
            # counters from the same lookup.
            declaration(cls.plugin, "enum class ToggleSlotFailReason"),
            implementation(cls.plugin, "static bool ToggleIndicatorFindSlot("),
            implementation(cls.plugin, "static void ToggleIndicatorDrawMarker("),
            implementation(cls.plugin, "static void ToggleIndicatorDraw("),
            # Issue #55 (skilltimer): the countdown itself, spliced from
            # right after ToggleBorderStats in the plugin (between the
            # border and the guard sections). N1Numeric is spliced here,
            # ahead of SkillTimerReadRow's own use of it, rather than at its
            # later Round-2 splice point below.
            implementation(cls.plugin, "static bool N1Numeric("),
            declaration(cls.plugin, "static std::atomic<ForgePact::SkillTimerStyle> g_SkillTimerStyle"),
            declaration(cls.plugin, "static ForgePact::SkillTimerRowState g_SkillTimerRowState"),
            implementation(cls.plugin, "struct SkillTimerRowCounters {") + ";",
            declaration(cls.plugin, "static SkillTimerRowCounters g_StRow"),
            declaration(cls.plugin, "static volatile long g_StDrawExc"),
            implementation(cls.plugin, "static bool SkillTimerResolveRowObject("),
            implementation(cls.plugin, "static int SkillTimerToggleTwin("),
            implementation(cls.plugin, "static void SkillTimerReadRow("),
            # Issue #55 follow-up (D-S4): the rule map's own runtime state and
            # draw-path helpers. The map-building walk itself is NOT spliced
            # (same reason as ToggleTableResolveIds above - it needs the
            # talent-map helpers; test_toggle_skill_contract.py pins that), so
            # a rule/* scenario populates g_SkillTimerRuleEntries/
            # g_SkillTimerRuleCount directly, the same way skilltimer/*
            # scenarios drive g_SkillTimerTableIds.
            declaration(cls.plugin, "static ForgePact::SkillTimerRuleEntry g_SkillTimerRuleEntries"),
            declaration(cls.plugin, "static volatile long g_SkillTimerRuleCount"),
            declaration(cls.plugin, "static volatile long g_RuleDrawn"),
            # Round 1 (replan #1): the walk itself, spliced - Lower() (the
            # key-match helper) and the talent-map helpers it and the walk
            # share with the rest of the plugin, the walk's own room/style
            # bookkeeping, and ToggleTableResolveDue/ToggleTableResolveIds
            # themselves. rule/walk_* scenarios drive these against a harness
            # stand-in talent map (below), a positive control the round-0
            # rule/* scenarios above did not have.
            implementation(cls.plugin, "static std::string Lower("),
            implementation(cls.plugin, "static bool N1GetTalentStruct("),
            implementation(cls.plugin, "static bool N1GetTalentMap("),
            declaration(cls.plugin, "static bool g_ToggleResolveWalked"),
            declaration(cls.plugin, "static bool g_ToggleResolveWalkedRuleOff"),
            declaration(cls.plugin, "static bool g_ToggleResolveRoomKnown"),
            declaration(cls.plugin, "static int64_t g_ToggleResolveRoomKey"),
            declaration(cls.plugin, "static constexpr long kToggleTableWalkCap"),
            declaration(cls.plugin, "static volatile long g_SkillTimerRuleDenied"),
            implementation(cls.plugin, "static bool ToggleTableResolveDue("),
            implementation(cls.plugin, "static bool ToggleTableResolveIds("),
            implementation(cls.plugin, "struct SkillTimerHotbarSlot {") + ";",
            implementation(cls.plugin, "static bool SkillTimerEnumerateHotbar("),
            implementation(cls.plugin, "static bool SkillTimerRuleResolveObject("),
            implementation(cls.plugin, "static void SkillTimerRuleReadEntry("),
            implementation(cls.plugin, "static RValue SkillTimerColour("),
            implementation(cls.plugin, "static void SkillTimerDrawRectOutlineFraction("),
            implementation(cls.plugin, "static void SkillTimerDrawArc("),
            implementation(cls.plugin, "static void SkillTimerDrawBar("),
            implementation(cls.plugin, "static void SkillTimerDrawNumber("),
            implementation(cls.plugin, "static void SkillTimerDrawFade("),
            implementation(cls.plugin, "static void SkillTimerDrawStyle("),
            implementation(cls.plugin, "static void SkillTimerDraw("),
            implementation(cls.plugin, "static std::string SkillTimerRowCountersLine("),
            implementation(cls.plugin, "static std::string SkillTimerAggregateCountersLine("),
            # SkillTimerStats() is not spliced, same as ToggleBorderStats()/
            # ToggleGuardStats() above it - it calls Out(), which this
            # harness (like the rest of ModuleMain.cpp's IPC output) does
            # not stand in for; test_toggle_skill_contract.py pins its body.
            # T1 (issue #11, Track A): the re-cast guard's real hook, its
            # trampoline slot, counters and cached object index, verbatim,
            # and the shared object-index predicate it reads the caller with
            # (VALUE_REF-aware, flag bits masked).
            implementation(cls.plugin, "static bool N1NearlyEqual("),
            implementation(cls.plugin, "static bool N1ObjectIndex("),
            declaration(cls.plugin, "static PFUNC_YYGMLScript g_OrigTalentUseClass"),
            declaration(cls.plugin, "static volatile long g_TgdRefused"),
            declaration(cls.plugin, "static std::atomic<long> g_ToggleGuardDcObjIdx"),
            # S (D-P3): the sub-talent read the refusal is gated on, verbatim,
            # so every fail-open shape is exercised against the real code.
            declaration(cls.plugin, "enum class ToggleSubTalentState"),
            # Which `global.subTalentMap` index answered, so a scenario can
            # assert the index was SELECTED rather than assumed.
            declaration(cls.plugin, "static std::atomic<int> g_TgdSubIndex"),
            implementation(cls.plugin, "static ToggleSubTalentState ToggleReadSubTalent("),
            implementation(cls.plugin, "static RValue& HookTalentUseClass("),
            # R (issue #11 generalisation, research build only): the
            # candidate table's generalised read, its row-0 comparison and
            # the timer sampler - the pure parts of `tgprobe tgl`, spliced
            # from inside the tgprobe block. A struct's definition ends at
            # its closing brace, so its `;` is added back here.
            implementation(cls.plugin, "struct TgTglReadResult {") + ";",
            implementation(cls.plugin, "struct TgTglTimer {") + ";",
            implementation(cls.plugin, "static bool TgProbeTglResolveObject("),
            implementation(cls.plugin, "static ForgePact::ToggleIndicatorState TgProbeTglRead("),
            implementation(cls.plugin, "static bool TgProbeTglSameDetail("),
            implementation(cls.plugin, "static std::string TgProbeTglNumber("),
            implementation(cls.plugin, "static void TgProbeTglTimerNote("),
            implementation(cls.plugin, "static std::string TgProbeTglTimerLine("),
            # Round 2: the sampler itself (its on/off gate and the field
            # snapshot's throttle) and the snapshot of the read's own
            # instance, with the table they walk. (N1Numeric is spliced
            # earlier now, ahead of SkillTimerReadRow's own use of it.)
            declaration(cls.plugin, "static constexpr int kTgTglCap"),
            declaration(cls.plugin, "static constexpr int kTgTglFieldCap"),
            declaration(cls.plugin, "static constexpr double kTgTglPredictedInfinite"),
            declaration(cls.plugin, "static constexpr long kTgTglSnapshotEveryDraws"),
            declaration(cls.plugin, "static bool g_TgTglSamplerOn"),
            implementation(cls.plugin, "struct TgTglFieldSample {") + ";",
            implementation(cls.plugin, "struct TgTglRow {") + ";",
            implementation(cls.plugin, "struct TgTglSeed {") + ";",
            implementation(cls.plugin, "static const TgTglSeed kTgTglSeeds[] = {") + ";",
            declaration(cls.plugin, "static std::vector<TgTglRow> g_TgTgl"),
            declaration(cls.plugin, "static bool g_TgTglSeeded"),
            declaration(cls.plugin, "static long g_TgTglAgree"),
            declaration(cls.plugin, "static bool g_TgTglRoomKeyKnown"),
            declaration(cls.plugin, "static int64_t g_TgTglRoomKey"),
            implementation(cls.plugin, "static void TgProbeTglSeed("),
            implementation(cls.plugin, "static void TgProbeTglSnapshot("),
            implementation(cls.plugin, "static std::string TgProbeTglFieldsText("),
            implementation(cls.plugin, "static void TgProbeTglAfterDraw("),
            # Session 8 (duration sweep, research build only): the six-root
            # sampler and its per-object record update, spliced from inside
            # the tgprobe block the same way as the `tgl` sampler above.
            # `show` is not spliced - it calls Out(); the contract test pins it.
            implementation(cls.plugin, "static const HeroSiege::Objects::GameObject kTgSweepRoots[] = {") + ";",
            declaration(cls.plugin, "static constexpr int kTgSweepRootCount"),
            declaration(cls.plugin, "static constexpr long kTgSweepScanCap"),
            declaration(cls.plugin, "static constexpr size_t kTgSweepRecordCap"),
            declaration(cls.plugin, "static bool g_TgSweepOn"),
            implementation(cls.plugin, "struct TgSweepObs {") + ";",
            implementation(cls.plugin, "struct TgSweepRecord {") + ";",
            declaration(cls.plugin, "static std::map<int, TgSweepRecord> g_TgSweep"),
            declaration(cls.plugin, "static long g_TgSweepDraws"),
            declaration(cls.plugin, "static long g_TgSweepCappedDraws"),
            declaration(cls.plugin, "static long g_TgSweepLastCount"),
            declaration(cls.plugin, "static double g_TgSweepLastIdx"),
            declaration(cls.plugin, "static int g_TgSweepRootsResolved"),
            implementation(cls.plugin, "static void TgProbeSweepNote("),
            implementation(cls.plugin, "static const char* TgProbeSweepOwnText("),
            implementation(cls.plugin, "static void TgProbeSweepAfterDraw("),
            # Session 12 (`tgprobe buffwatch`, buff-carried skills): the
            # nesting-depth globals TgProbeDetourBody writes (not spliced -
            # it needs the whole g_TgRows/TGPROBE_SCRIPTS machinery; the
            # contract test pins its two branches by source text instead),
            # the record/note/sampler, and the BuffAdd note's own testable
            # core - `talentUseNative`/`talentUseClassNative` are parameters
            # precisely so this function never needs g_TgRows either. `show`
            # is not spliced (it calls Out()); the contract test pins it.
            declaration(cls.plugin, "static volatile long g_TgTalentUseDepth"),
            declaration(cls.plugin, "static volatile long g_TgTalentUseClassDepth"),
            declaration(cls.plugin, "static double g_TgTalentUseClassA0"),
            declaration(cls.plugin, "static bool g_TgBuffWatchOn"),
            implementation(cls.plugin, "struct TgBuffWatchVar {") + ";",
            implementation(cls.plugin, "struct TgBuffWatchRecord {") + ";",
            declaration(cls.plugin, "static std::map<int, TgBuffWatchRecord> g_TgBuffWatch"),
            declaration(cls.plugin, "static long g_TgBuffWatchDraws"),
            implementation(cls.plugin, "static void TgProbeBuffWatchCaptureVars("),
            implementation(cls.plugin, "static void TgProbeBuffWatchNote("),
            implementation(cls.plugin, "static void TgProbeBuffWatchAfterDraw("),
            implementation(cls.plugin, "static void TgProbeBuffWatchOnBuffAdd("),
            # Round 1 (owner-requested hardening): the visibility/note-text
            # pair `show` uses to stop hiding the mismatch-only and
            # added-only shapes - pure (no Out()), so this pair is spliced
            # and testable the same way TgProbeSweepOwnText is; `show` itself
            # stays unspliced (it calls Out()), pinned by the contract test.
            implementation(cls.plugin, "static bool TgProbeBuffWatchVisible("),
            implementation(cls.plugin, "static std::string TgProbeBuffWatchNoteText("),
        ])

        out = ROOT / "build/toggle-skill-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/toggle_skill_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_CONSTANTS", constants)
        code = code.replace("// PRODUCTION_TOGGLESKILL", klass)
        code = code.replace("// PRODUCTION_SKILLTIMER", skilltimer_klass)
        code = code.replace("// PRODUCTION_FUNCTIONS", production)
        cpp = out / "toggleskill.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("toggleskill.exe" if os.name == "nt" else "toggleskill")
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not vswhere.is_file():
                raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
            install = subprocess.check_output(
                [str(vswhere), "-latest", "-products", "*", "-requires",
                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                text=True).strip()
            if not install:
                raise unittest.SkipTest("Visual Studio C++ toolchain not installed")
            vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
            batch = out / "compile.cmd"
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "toggleskill.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(cls.binary)]

        result = subprocess.run(command, cwd=out, capture_output=True, text=True)
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

        run = subprocess.run([str(cls.binary)], capture_output=True, text=True)
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def line(self, label):
        for line in self.output.split("\n"):
            if line.split(" ")[1:2] == [label]:
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def assertScenario(self, label):
        self.assertTrue(self.line(label).startswith("PASS "), self.line(label))

    def test_harness_ran(self):
        self.assertIn("RESULT OK", self.output, self.output)

    # ---- ownership (isMyClient) --------------------------------------------

    def test_no_aoe_is_off(self):
        self.assertScenario("read/no_aoe_is_off")

    def test_object_unresolved_is_unreadable(self):
        # A different, stronger failure than "resolved but zero instances".
        self.assertScenario("read/object_unresolved_is_unreadable")

    def test_instance_number_throw_is_unreadable(self):
        # A threw instance_number call is a failed read, not a measured zero
        # - the catch's `d.n = 0` fallback must not decide a real, cheap Off.
        self.assertScenario("read/instance_number_throw_is_unreadable")
        self.assertScenario("read/instance_number_throw_is_unreadable/countReadFailed")

    def test_own_bool_true_is_on(self):
        self.assertScenario("read/own_bool_true_is_on")
        self.assertScenario("read/own_bool_true_is_on/mine")

    def test_own_real_one_is_on(self):
        # isMyClient a nonzero numeric, not a VALUE_BOOL - still counts true.
        self.assertScenario("read/own_real_one_is_on")

    def test_foreign_bool_false_is_off(self):
        self.assertScenario("read/foreign_bool_false_is_off")
        self.assertScenario("read/foreign_bool_false_is_off/others")

    def test_own_and_foreign_is_on(self):
        self.assertScenario("read/own_and_foreign_is_on")
        self.assertScenario("read/own_and_foreign_is_on/mine")
        self.assertScenario("read/own_and_foreign_is_on/others")

    def test_two_own_is_on(self):
        self.assertScenario("read/two_own_is_on")
        self.assertScenario("read/two_own_is_on/mine")

    def test_unattributed_only_is_unreadable(self):
        # A foreign AOE fails toward "absent"; an unattributed one (its own
        # isMyClient could not be read) must not be guessed either way.
        self.assertScenario("read/unattributed_only_is_unreadable")
        self.assertScenario("read/unattributed_only_is_unreadable/unattributed")

    def test_scan_is_capped(self):
        self.assertScenario("read/scan_is_capped")
        self.assertScenario("read/scan_is_capped/capped")
        self.assertScenario("read/scan_is_capped/n")
        self.assertScenario("read/scan_is_capped/mine")

    def test_reread_every_call(self):
        # No caching across calls - the same point-of-use rule as the guide's
        # Known Limitations item 13.
        self.assertScenario("read/reread_every_call/first_off")
        self.assertScenario("read/reread_every_call")

    def test_as_foreign_excludes_own(self):
        # `spurn as foreign`: the non-mutating negative control.
        self.assertScenario("read/as_foreign_excludes_own")
        self.assertScenario("read/as_foreign_excludes_own/others")
        self.assertScenario("read/as_foreign_excludes_own/mine")

    def test_no_player_lookup(self):
        # The read makes no player-resolving call at all, in any scenario
        # above - Known Limitations item 7's kind-check bug had a different
        # root cause than this workorder's, but the fix here is the same
        # shape: read the thing itself, not something read off another
        # object first.
        self.assertScenario("read/no_player_lookup")

    # ---- the Purgatory marker (session 4's discriminator) ------------------

    def test_marked_own_on_when_required(self):
        self.assertScenario("marker/marked_own_on_when_required/markedMine")
        self.assertScenario("marker/marked_own_on_when_required")

    def test_unmarked_own_off_when_required(self):
        self.assertScenario("marker/unmarked_own_off_when_required/unmarkedMine")
        self.assertScenario("marker/unmarked_own_off_when_required")

    def test_unmarked_own_on_when_not_required(self):
        # The SAME unmarked own instance: the plain ownership read (no
        # marker required) does not consult purgatory at all.
        self.assertScenario("marker/unmarked_own_on_when_not_required")

    def test_unreadable_marker_unreadable_when_required(self):
        self.assertScenario("marker/unreadable_marker_unreadable_when_required/markUnreadableMine")
        self.assertScenario("marker/unreadable_marker_unreadable_when_required")

    def test_foreign_marker_ignored(self):
        # A foreign instance's own purgatory is never read for the marker
        # split; only own instances count.
        self.assertScenario("marker/foreign_marker_ignored/markedMine")
        self.assertScenario("marker/foreign_marker_ignored")

    # ---- the shipped indicator (P2): ToggleIndicatorDraw() -----------------

    def test_indicator_off_makes_no_runtime_call(self):
        self.assertScenario("indicator_off/no_runtime_calls")

    def test_indicator_on_own_on_outlines_slot(self):
        self.assertScenario("indicator_on/own_on_outlines_slot")
        self.assertScenario("indicator_on/own_on_outlines_slot/rectangles")

    def test_indicator_on_off_draws_nothing(self):
        self.assertScenario("indicator_on/off_draws_nothing")
        self.assertScenario("indicator_on/off_draws_nothing/rectangles")

    def test_indicator_on_foreign_only_draws_nothing(self):
        self.assertScenario("indicator_on/foreign_only_draws_nothing")
        self.assertScenario("indicator_on/foreign_only_draws_nothing/counter")

    def test_indicator_on_unreadable_draws_nothing_and_counts(self):
        self.assertScenario("indicator_on/unreadable_draws_nothing_and_counts")
        self.assertScenario("indicator_on/unreadable_draws_nothing_and_counts/counter")

    def test_indicator_on_slot_not_found_draws_nothing_and_counts(self):
        self.assertScenario("indicator_on/slot_not_found_draws_nothing_and_counts")
        self.assertScenario("indicator_on/slot_not_found_draws_nothing_and_counts/counter")

    def test_indicator_on_unmarked_own_draws_nothing(self):
        # Session 4 measured the plain-cast flash (D-R2): the marker is
        # required, so an unmarked own instance must not light the outline.
        self.assertScenario("indicator_on/unmarked_own_draws_nothing")

    def test_indicator_on_state_reread_every_draw(self):
        self.assertScenario("indicator_on/state_reread_every_draw/first_off")
        self.assertScenario("indicator_on/state_reread_every_draw")

    def test_indicator_on_draw_colour_and_alpha_restored(self):
        self.assertScenario("indicator_on/draw_colour_and_alpha_restored/colour")
        self.assertScenario("indicator_on/draw_colour_and_alpha_restored/alpha")

    # ---- follow-ups from the indicator's reviews (issue #11, Track B) ------

    def test_indicator_on_draw_exception_counts(self):
        # A throwing draw_rectangle stub: the catch after the outline loop
        # must count the exception rather than swallow it uncounted.
        self.assertScenario("indicator_on/draw_exception_counts/drawn")
        self.assertScenario("indicator_on/draw_exception_counts")

    def test_indicator_on_slot_failures_are_split(self):
        # ToggleIndicatorFindSlot's old single noSlot counter split into
        # three that mean something different, each leaving the other two
        # untouched.
        self.assertScenario("indicator_on/slot_failures_are_split/noHud")
        self.assertScenario("indicator_on/slot_failures_are_split/noHud/others_zero")
        self.assertScenario("indicator_on/slot_failures_are_split/noRow0")
        self.assertScenario("indicator_on/slot_failures_are_split/noRow0/others_zero")
        self.assertScenario("indicator_on/slot_failures_are_split/noTalent")
        self.assertScenario("indicator_on/slot_failures_are_split")

    # ---- S: the D-U13 marker, the D-U12 box and the per-row discriminators --

    def test_border_marker_colour_is_deepred(self):
        for suffix in ("/r", "/g", "/b", ""):
            self.assertScenario("border/marker_colour_is_deepred" + suffix)

    def test_border_marker_alpha_ramps_outwards(self):
        for suffix in ("/strictly_decreasing", "/innermost", "/outermost", ""):
            self.assertScenario("border/marker_alpha_ramps_outwards" + suffix)

    def test_border_marker_box_is_derived_and_whole_pixel(self):
        # D-U12's worked example end to end: a fractional navBbox read becomes
        # the accepted whole-pixel box, derived rather than hardcoded.
        for suffix in ("/x1", "/y1", "/x2", "/y2", ""):
            self.assertScenario("border/marker_box_is_derived_and_whole_pixel" + suffix)

    def test_border_soul_spurn_marker_unchanged_semantics(self):
        self.assertScenario("border/soul_spurn_marker_unchanged_semantics/drawn")
        self.assertScenario("border/soul_spurn_marker_unchanged_semantics")

    def test_border_two_rows_two_borders(self):
        for suffix in ("/drawn", "/slot_lookups", ""):
            self.assertScenario("border/two_rows_two_borders" + suffix)

    def test_border_no_discriminator_row_lights_on_any_own(self):
        self.assertScenario("border/no_discriminator_row_lights_on_any_own/requireMarker")
        self.assertScenario("border/no_discriminator_row_lights_on_any_own")

    def test_border_ownership_none_counts_every_instance_own(self):
        # D-N3: the row names no ownership field, so the read never asks and a
        # throwing `isMyClient` cannot turn the row Unreadable.
        self.assertScenario("border/ownership_none_counts_every_instance_own")

    def test_border_timer_discriminator(self):
        self.assertScenario("border/timer_at_infinite_draws_full")
        self.assertScenario("border/timer_counting_down_draws_nothing/off")
        self.assertScenario("border/timer_counting_down_draws_nothing")
        self.assertScenario("border/timer_other_negative_draws_nothing")
        for suffix in ("/undefined", "/throws", ""):
            self.assertScenario("border/timer_unreadable_draws_nothing_and_counts" + suffix)

    def test_table_unresolved_row_skipped_and_counted(self):
        for suffix in ("/drawn", "/no_enumeration", ""):
            self.assertScenario("table/unresolved_row_skipped_and_counted" + suffix)

    # ---- session 9: Meteor Storm and Bushido (D-B1) -------------------------

    def test_border_meteor_storm(self):
        self.assertScenario("border/meteor_storm_bool_true_lights_slot")
        self.assertScenario("border/meteor_storm_positive_number_lights_slot")
        self.assertScenario("border/meteor_storm_plain_cast_real_zero_draws_nothing")
        self.assertScenario("border/meteor_storm_bool_false_draws_nothing")
        self.assertScenario("border/meteor_storm_unreadable_marker_draws_nothing_and_counts")
        self.assertScenario("border/meteor_storm_every_instance_counts_own")

    def test_border_bushido(self):
        self.assertScenario("border/bushido_on_lights_slot")
        self.assertScenario("border/bushido_foreign_only_draws_nothing")
        self.assertScenario("border/bushido_no_instance_draws_nothing")

    # ---- T1: the re-cast guard, HookTalentUseClass (issue #11, Track A) ----

    def test_guard_off_proc_passes_and_no_runtime_call(self):
        # Baseline: off by default, the hook is one atomic load and the
        # original - no builtin is called at all.
        self.assertScenario("guard_off/proc_passes_and_no_runtime_call/runtime_calls")
        self.assertScenario("guard_off/proc_passes_and_no_runtime_call")

    def test_guard_on_proc_of_guarded_talent_refused(self):
        self.assertScenario("guard_on/proc_of_guarded_talent_refused/result_untouched")
        self.assertScenario("guard_on/proc_of_guarded_talent_refused/refused")
        self.assertScenario("guard_on/proc_of_guarded_talent_refused")

    def test_guard_on_proc_of_other_talent_passes(self):
        self.assertScenario("guard_on/proc_of_other_talent_passes/procSeen")
        self.assertScenario("guard_on/proc_of_other_talent_passes/refused")
        self.assertScenario("guard_on/proc_of_other_talent_passes")

    def test_guard_on_player_cast_passes(self):
        self.assertScenario("guard_on/player_cast_passes/refused")
        self.assertScenario("guard_on/player_cast_passes")

    def test_guard_on_player_chain_passes(self):
        self.assertScenario("guard_on/player_chain_passes")

    def test_guard_on_self_unreadable_passes_and_counts(self):
        self.assertScenario("guard_on/self_unreadable_passes_and_counts/selfUnreadable")
        self.assertScenario("guard_on/self_unreadable_passes_and_counts")

    def test_guard_on_double_cast_object_unresolved_passes_and_counts(self):
        # A failed resolve is never cached: the next call resolves again.
        self.assertScenario("guard_on/double_cast_object_unresolved_passes_and_counts/objUnresolved")
        self.assertScenario("guard_on/double_cast_object_unresolved_passes_and_counts/trampoline")
        self.assertScenario("guard_on/double_cast_object_unresolved_passes_and_counts/re_resolved")
        self.assertScenario("guard_on/double_cast_object_unresolved_passes_and_counts/cached")
        self.assertScenario("guard_on/double_cast_object_unresolved_passes_and_counts")

    def test_guard_on_state_not_consulted(self):
        # D-N1: the toggle's state cannot tell "just turned off" from "never
        # on", so the guard never reads it.
        self.assertScenario("guard_on/state_not_consulted")

    def test_guard_on_counters(self):
        self.assertScenario("guard_on/counters/refused")
        self.assertScenario("guard_on/counters/passed")
        self.assertScenario("guard_on/counters/procSeen")
        self.assertScenario("guard_on/counters")

    def test_guard_on_self_object_index_kinds(self):
        # This runner returns object_index as VALUE_REF; a guard that only
        # trusted plain number kinds would fail open on every live call.
        for kind in ("ref", "real", "int32", "int64", "ref_flagged"):
            label = f"guard_on/self_object_index_kinds/{kind}"
            self.assertScenario(label + "/selfUnreadable")
            self.assertScenario(label + "/refused")
            self.assertScenario(label)

    def test_guard_on_self_object_index_not_an_index_passes(self):
        # Negative control: widening the accepted kinds is not accepting
        # anything.
        for kind in ("undefined", "string", "bool"):
            label = f"guard_on/self_object_index_not_an_index_passes/{kind}"
            self.assertScenario(label + "/selfUnreadable")
            self.assertScenario(label + "/refused")
            self.assertScenario(label)

    # ---- S: the border's per-row counters ----------------------------------

    def test_draw_exception_still_restores_colour_and_alpha(self):
        # Re-review finding (P2): counting the exception is not isolating it.
        # The marker's colour and alpha are already set when the rectangle
        # throws, so a HUD that keeps drawing after us would inherit them.
        for suffix in ("/drawn", "", "/colour_restored", "/alpha_restored"):
            self.assertScenario("indicator_on/draw_exception_counts" + suffix)

    def test_border_per_row_counters_name_the_row(self):
        # Phase S review follow-up: the aggregate counters sum all five rows,
        # so one draw with row 0 ON, row 1 resolved-but-absent and the rest
        # unresolved has to land in three different rows' counters, and a
        # row's line has to name it.
        for suffix in ("/row0_on", "/row0_drawn", "/row0_off", "/row1_off", "/row1_on",
                       "/row2_unresolved", "/row0_named", "/row1_named", "/row0_noSlot", ""):
            self.assertScenario("border/per_row_counters_name_the_row" + suffix)

    # ---- S: the guard's sub-talent gate (D-P3) -----------------------------

    def test_guard_on_table_talent_with_subtalent_refused(self):
        for suffix in ("/refused", "/subOff", ""):
            self.assertScenario("guard_on/table_talent_with_subtalent_refused" + suffix)

    def test_guard_on_table_talent_without_subtalent_passes(self):
        # Session 6's measured unallocated form: `s12` reads 0.000000, the key
        # present. The proc is then a plain cast and must not be eaten.
        for suffix in ("/subOff", "/refused", ""):
            self.assertScenario("guard_on/table_talent_without_subtalent_passes" + suffix)

    def test_guard_on_subtalent_unreadable_passes_and_counts(self):
        for shape in ("global_absent", "not_an_array", "index_out_of_range",
                      "talent_key_absent", "slot_non_numeric", "read_throws"):
            label = f"guard_on/subtalent_unreadable_passes_and_counts/{shape}"
            self.assertScenario(label + "/subUnreadable")
            self.assertScenario(label + "/refused")
            self.assertScenario(label)
        self.assertScenario("guard_on/subtalent_unreadable_passes_and_counts")

    def test_guard_on_subtalent_other_index_answers(self):
        # Phase S review follow-up: session 6 measured the sub-talent map index
        # on one character on one build, so the index is SELECTED (the one
        # whose `t<talentId>` struct is really there), not assumed. A fixed
        # index that turned out to be a character slot would leave the guard
        # inert with nothing but a counter to show for it.
        for suffix in ("/refused", "/subUnreadable", "/index", ""):
            self.assertScenario("guard_on/subtalent_other_index_answers" + suffix)

    def test_guard_on_subtalent_measured_index_struct_absent_falls_back(self):
        for suffix in ("/subOff", "/refused", "/index", ""):
            self.assertScenario(
                "guard_on/subtalent_measured_index_struct_absent_falls_back" + suffix)

    def test_guard_on_subtalent_bad_entry_costs_one_index(self):
        # Re-review follow-up: a junk entry earlier in the array (a number
        # where a struct should be, or an entry whose read throws) must cost
        # one index, not the whole walk - otherwise the guard is inert again
        # and `subIndex=none` is the only symptom.
        for suffix in ("/refused", "/subUnreadable", "/index", ""):
            self.assertScenario("guard_on/subtalent_bad_entry_costs_one_index" + suffix)

    def test_guard_on_subtalent_no_index_answers(self):
        # The negative control beside the two positives: nothing anywhere in
        # the array carries this talent, so the read is unreadable, the call
        # passes, and `subIndex=` reports none rather than a number.
        for suffix in ("/subUnreadable", "/refused", "/index", ""):
            self.assertScenario("guard_on/subtalent_no_index_answers" + suffix)

    def test_guard_on_non_table_talent_passes(self):
        for suffix in ("/refused", "/sub_not_read", ""):
            self.assertScenario("guard_on/non_table_talent_passes" + suffix)

    def test_guard_on_unresolved_row_passes(self):
        for suffix in ("/refused", "", "/unnamed_talent_never_matches"):
            self.assertScenario("guard_on/unresolved_row_passes" + suffix)

    # ---- session 9: Meteor Storm's sub-talent gate and Bushido's D-B1 -------

    def test_guard_on_meteor_storm_subtalent_gate(self):
        self.assertScenario("guard_on/meteor_storm_proc_refused_when_subtalent_allocated")
        self.assertScenario("guard_on/meteor_storm_proc_passes_without_subtalent")

    def test_guard_on_base_form_row_refused_without_reading_the_map(self):
        # D-B1: Bushido's base-form row is refused unconditionally and never
        # reads global.subTalentMap at all - the control alongside it
        # (subtalent_row_still_reads_the_map) shows the same counter DOES
        # move for a row that has a real sub-talent to read.
        for suffix in ("/baseForm", "/no_map_read", ""):
            self.assertScenario("guard_on/base_form_row_proc_refused_without_subtalent_read" + suffix)
        self.assertScenario("guard_on/base_form_row_player_cast_passes")
        self.assertScenario("guard_off/base_form_row_proc_passes")
        self.assertScenario("guard_on/subtalent_row_still_reads_the_map")

    # ---- R: the research table (`tgprobe tgl`, issue #11 generalisation) ----

    def test_table_generalised_read_matches_shipped_read_on_row0(self):
        # The live agree=/disagree= control, proven here first: row 0 through
        # the generalised read decides what the shipped read decides, and the
        # comparison itself can see a difference (negative control).
        self.assertScenario("table/generalised_read_matches_shipped_read_on_row0/cases")
        self.assertScenario("table/generalised_read_matches_shipped_read_on_row0")
        self.assertScenario("table/generalised_read_matches_shipped_read_on_row0/control_detects_difference")

    def test_table_marker_none_lights_on_any_own(self):
        self.assertScenario("table/marker_none_lights_on_any_own/markedMine")
        self.assertScenario("table/marker_none_lights_on_any_own")
        self.assertScenario("table/marker_none_lights_on_any_own/foreign_stays_off")
        self.assertScenario("table/ownership_none_counts_every_instance_own/unattributed")
        self.assertScenario("table/ownership_none_counts_every_instance_own")

    def test_table_entry_object_unresolved_is_unreadable(self):
        self.assertScenario("table/entry_object_unresolved_is_unreadable/resolved")
        self.assertScenario("table/entry_object_unresolved_is_unreadable/no_enumeration")
        self.assertScenario("table/entry_object_unresolved_is_unreadable")

    def test_table_timer_sample_reads_first_and_last(self):
        # A discriminator instrument, not a border input (D-U9, D-P5).
        for suffix in ("/first", "/last", "/min_max", "/unreadable_zero",
                       "/held_at_predicted", "/foreign_not_read", ""):
            self.assertScenario("table/timer_sample_reads_first_and_last" + suffix)

    def test_table_timer_unreadable_is_reported_not_defaulted(self):
        for suffix in ("/count", "/atPredicted", "/first", "/last", ""):
            self.assertScenario("table/timer_unreadable_is_reported_not_defaulted" + suffix)

    def test_table_sampler_off_calls_no_builtin(self):
        # Round 2: `tgprobe tgl` is off by default and costs nothing while
        # off; switched on it reads (positive control).
        for suffix in ("/default_off", "", "/control_on_reads"):
            self.assertScenario("table/sampler_off_calls_no_builtin" + suffix)

    def test_table_sampler_on_snapshots_at_most_every_30_draws(self):
        for suffix in ("/per_row", "/timer_per_draw", ""):
            self.assertScenario("table/sampler_on_snapshots_at_most_every_30_draws" + suffix)

    def test_table_fields_snapshot_uses_own_instance(self):
        for suffix in ("/own_fields", "", "/no_own_reads_nothing", "/no_own_stores_nothing", "/no_own_line"):
            self.assertScenario("table/fields_snapshot_uses_own_instance" + suffix)

    # ---- session 8: the duration sweep (`tgprobe sweep`, research only) ----

    def test_sweep_off_makes_no_runtime_calls(self):
        for suffix in ("/default_off", "", "/control_on_reads"):
            self.assertScenario("sweep/off_makes_no_runtime_calls" + suffix)

    def test_sweep_appearance_counts_rising_edge(self):
        for suffix in ("", "/absent_draw_clears_present", "/draws_is_current_appearance",
                       "/totalDraws", "/maxInst_not_summed_across_roots"):
            self.assertScenario("sweep/appearance_counts_rising_edge" + suffix)

    def test_sweep_first_is_first_readable_of_appearance(self):
        for suffix in ("/unset_after_unreadable", "", "/last", "/min", "/max", "/timerUnreadable",
                       "/restarts", "/restarts_unreadable"):
            self.assertScenario("sweep/first_is_first_readable_of_appearance" + suffix)

    def test_sweep_unreadable_never_defaults(self):
        # The negative (undefined, throw, string, bool) beside its positive
        # control (an int64 reading counts), plus an unreadable object_index.
        for suffix in ("", "/count", "/draws", "/control_int64_reads", "/index_unreadable_counted"):
            self.assertScenario("sweep/unreadable_never_defaults" + suffix)

    def test_sweep_largest_reading_of_draw_wins(self):
        for suffix in ("", "/foreign_not_taken", "/other_object_separate", "/maxInst",
                       "/foreign_only_is_unreadable"):
            self.assertScenario("sweep/largest_reading_of_draw_wins" + suffix)

    def test_sweep_ownership_readability_counted_per_draw(self):
        for suffix in ("/readable", "/mixed", "/unreadable", "", "/unattributed_timer_read",
                       "/all_readable", "/all_unreadable"):
            self.assertScenario("sweep/ownership_readability_counted_per_draw" + suffix)

    # ---- session 12: buff-carried skills (`tgprobe buffwatch`, research only) --

    def test_buffwatch_first_sight_starts_appearance(self):
        for suffix in ("/app", "/present", "", "/max"):
            self.assertScenario("buffwatch/first_sight_starts_appearance" + suffix)

    def test_buffwatch_identity_mismatch_counted_not_recorded(self):
        for suffix in ("/app", "", "/global"):
            self.assertScenario("buffwatch/identity_mismatch_counted_not_recorded" + suffix)

    def test_buffwatch_refresh_rise_is_kept_as_max(self):
        for suffix in ("", "/last", "/first_unchanged"):
            self.assertScenario("buffwatch/refresh_rise_is_kept_as_max" + suffix)

    def test_buffwatch_removal_ends_appearance(self):
        for suffix in ("", "/app_unchanged"):
            self.assertScenario("buffwatch/removal_ends_appearance" + suffix)

    def test_buffwatch_buffadd_note_records_frames_and_nesting(self):
        for suffix in ("/adds", "", "/player", "/inUse", "/useTalent",
                       "/inUse_not_native", "/useTalent_not_native"):
            self.assertScenario("buffwatch/buffadd_note_records_frames_and_nesting" + suffix)

    # ---- Round 1 (owner-requested hardening): `show`'s visibility/note ----

    def test_buffwatch_seen_present_is_shown(self):
        self.assertScenario("buffwatch/seen_present_is_shown/visible")
        self.assertScenario("buffwatch/seen_present_is_shown/no_note")

    def test_buffwatch_mismatch_only_is_shown(self):
        for suffix in ("/app", "/visible", "/note"):
            self.assertScenario("buffwatch/mismatch_only_is_shown" + suffix)

    def test_buffwatch_added_only_is_shown(self):
        for suffix in ("/app", "/visible", "/note"):
            self.assertScenario("buffwatch/added_only_is_shown" + suffix)

    def test_buffwatch_neither_is_hidden(self):
        self.assertScenario("buffwatch/neither_is_hidden")

    def test_buffwatch_playerbuff_not_array(self):
        self.assertScenario("buffwatch/playerbuff_not_array/no_new_records")

    # ---- issue #55: the timed-skill countdown (`skilltimer`) --------------

    def test_skilltimer_off_makes_no_runtime_calls(self):
        self.assertScenario("skilltimer/off_makes_no_runtime_calls")

    def test_skilltimer_no_instance_draws_nothing(self):
        self.assertScenario("skilltimer/no_instance_draws_nothing")
        self.assertScenario("skilltimer/no_instance_draws_nothing/rects")

    def test_skilltimer_unreadable_timer_draws_nothing(self):
        self.assertScenario("skilltimer/unreadable_timer_draws_nothing")
        self.assertScenario("skilltimer/unreadable_timer_draws_nothing/rects")

    def test_skilltimer_foreign_instance_not_counted(self):
        self.assertScenario("skilltimer/foreign_instance_not_counted")
        self.assertScenario("skilltimer/foreign_instance_not_counted/not_unreadable")

    def test_skilltimer_toggle_on_suppresses(self):
        self.assertScenario("skilltimer/toggle_on_suppresses")
        self.assertScenario("skilltimer/toggle_on_suppresses/rects")

    def test_skilltimer_unresolved_row_skipped(self):
        self.assertScenario("skilltimer/unresolved_row_skipped")
        self.assertScenario("skilltimer/unresolved_row_skipped/rects")

    def test_skilltimer_slot_miss_not_charged_to_toggleborder(self):
        self.assertScenario("skilltimer/slot_miss_not_charged_to_toggleborder")
        self.assertScenario("skilltimer/slot_miss_not_charged_to_toggleborder/tibNoHud")
        self.assertScenario("skilltimer/slot_miss_not_charged_to_toggleborder/tibRowNoSlot")

    def test_skilltimer_first_sight_latches_full(self):
        self.assertScenario("skilltimer/first_sight_latches_full/outcome")
        self.assertScenario("skilltimer/first_sight_latches_full")
        self.assertScenario("skilltimer/first_sight_latches_full/latched")

    def test_skilltimer_fraction_is_remaining_over_latch(self):
        self.assertScenario("skilltimer/fraction_is_remaining_over_latch")
        self.assertScenario("skilltimer/fraction_is_remaining_over_latch/not_relatched")

    def test_skilltimer_rise_relatches(self):
        self.assertScenario("skilltimer/rise_relatches")
        self.assertScenario("skilltimer/rise_relatches/latched")

    def test_skilltimer_instance_gone_unlatches(self):
        self.assertScenario("skilltimer/instance_gone_unlatches/outcome")
        self.assertScenario("skilltimer/instance_gone_unlatches")
        self.assertScenario("skilltimer/instance_gone_unlatches/state_cleared")

    def test_skilltimer_non_positive_draws_nothing_and_never_latches(self):
        self.assertScenario("skilltimer/non_positive_draws_nothing_and_never_latches/outcome")
        self.assertScenario("skilltimer/non_positive_draws_nothing_and_never_latches")
        self.assertScenario("skilltimer/non_positive_draws_nothing_and_never_latches/latch_untouched")

    def test_skilltimer_bar_geometry(self):
        for suffix in ("/count", "/x0", "/y0", "/x1", "", "/colour_r", "/colour_g", "/colour_b"):
            self.assertScenario("skilltimer/bar_geometry" + suffix)

    def test_skilltimer_bar_subpixel_draws_nothing(self):
        self.assertScenario("skilltimer/bar_subpixel_draws_nothing")

    def test_skilltimer_number_text_and_anchor(self):
        for suffix in ("/count", "/x", "/y", "/valign_top", ""):
            self.assertScenario("skilltimer/number_text_and_anchor" + suffix)

    def test_skilltimer_number_zero_percent_draws_nothing(self):
        self.assertScenario("skilltimer/number_zero_percent_draws_nothing")
        self.assertScenario("skilltimer/number_zero_percent_draws_nothing/no_font_set")

    def test_skilltimer_number_restores_draw_state(self):
        for suffix in ("/font", "/halign", "/valign", "/colour", ""):
            self.assertScenario("skilltimer/number_restores_draw_state" + suffix)

    def test_skilltimer_arc_traces_fraction_of_perimeter(self):
        for suffix in ("/count", "/x0", "/y0", "/x1", ""):
            self.assertScenario("skilltimer/arc_traces_fraction_of_perimeter" + suffix)

    def test_skilltimer_fade_scales_band_alpha(self):
        self.assertScenario("skilltimer/fade_scales_band_alpha")
        self.assertScenario("skilltimer/fade_scales_band_alpha/rects")

    def test_skilltimer_draw_throw_restores_and_counts(self):
        for suffix in ("/drawn", "", "/colour_restored", "/alpha_restored"):
            self.assertScenario("skilltimer/draw_throw_restores_and_counts" + suffix)

    # ---- session 8: the countdown's own table (kSkillTimerRows) -----------

    def test_skilltimer_non_toggle_row_makes_no_toggle_read(self):
        for suffix in ("/drawn", "", "/no_toggle_count"):
            self.assertScenario("skilltimer/non_toggle_row_makes_no_toggle_read" + suffix)

    def test_skilltimer_toggle_row_still_suppressed_when_on(self):
        for suffix in ("", "/not_expired", "/rects", "/toggle_object_read"):
            self.assertScenario("skilltimer/toggle_row_still_suppressed_when_on" + suffix)

    def test_skilltimer_unresolved_countdown_row_skipped(self):
        for suffix in ("", "/rects", "/no_instance_read"):
            self.assertScenario("skilltimer/unresolved_countdown_row_skipped" + suffix)

    def test_skilltimer_rows_keep_separate_latches(self):
        for suffix in ("/first", "/second", "", "/fraction"):
            self.assertScenario("skilltimer/rows_keep_separate_latches" + suffix)

    # ---- issue #55 follow-up (D-S4): rule-based coverage of untested skills

    def test_rule_eligible_duration_and_cooldown(self):
        self.assertScenario("rule/eligible_duration_and_cooldown")

    def test_rule_cooldown_at_floor_is_ineligible(self):
        self.assertScenario("rule/cooldown_at_floor_is_ineligible")

    def test_rule_cooldown_just_above_floor_is_eligible(self):
        self.assertScenario("rule/cooldown_just_above_floor_is_eligible")

    def test_rule_duration_zero_is_ineligible(self):
        self.assertScenario("rule/duration_zero_is_ineligible")

    def test_rule_unreadable_field_is_ineligible_and_counted(self):
        self.assertScenario("rule/unreadable_field_is_ineligible_and_counted")

    def test_rule_deny_list_wins_over_the_rule(self):
        self.assertScenario("rule/deny_list_wins_over_the_rule")

    def test_rule_explicit_row_wins_over_the_rule(self):
        self.assertScenario("rule/explicit_row_wins_over_the_rule")

    def test_rule_companion_never_enters_the_table(self):
        self.assertScenario("rule/companion_never_enters_the_table")

    def test_rule_slot_off_hotbar_costs_no_instance_scan(self):
        for suffix in ("", "/no_object_resolve", "/no_outcome_counted"):
            self.assertScenario("rule/slot_off_hotbar_costs_no_instance_scan" + suffix)

    def test_rule_no_object_by_name_is_counted_not_drawn(self):
        for suffix in ("", "/not_drawn", "/no_instance_scan"):
            self.assertScenario("rule/no_object_by_name_is_counted_not_drawn" + suffix)

    def test_rule_toggle_twin_is_suppressed_when_on(self):
        for suffix in ("", "/not_drawn", "/object_never_resolved"):
            self.assertScenario("rule/toggle_twin_is_suppressed_when_on" + suffix)

    def test_rule_entries_keep_separate_latches(self):
        for suffix in ("/first", "/second", ""):
            self.assertScenario("rule/entries_keep_separate_latches" + suffix)

    # ---- round 1 (replan #1): the walk itself, spliced ---------------------

    def test_rule_walk_denies_before_lookup(self):
        for suffix in ("", "/denied_counted"):
            self.assertScenario("rule/walk_denies_before_lookup" + suffix)

    def test_rule_walk_matches_camelcase_id_to_lowercase_key(self):
        for suffix in ("", "/right_index"):
            self.assertScenario("rule/walk_matches_camelcase_id_to_lowercase_key" + suffix)

    def test_rule_walk_counts_eligible_talent_with_no_key_as_rule_no_name(self):
        for suffix in ("", "/no_entry"):
            self.assertScenario("rule/walk_counts_eligible_talent_with_no_key_as_rule_no_name" + suffix)

    def test_rule_walk_does_not_count_ineligible_talent_with_no_key(self):
        self.assertScenario("rule/walk_does_not_count_ineligible_talent_with_no_key")

    def test_rule_walk_counts_unreadable_field(self):
        for suffix in ("", "/no_entry", "/not_rule_no_name"):
            self.assertScenario("rule/walk_counts_unreadable_field" + suffix)

    def test_rule_walk_never_enters_an_explicit_row(self):
        for suffix in ("", "/row_resolved", "/nothing_else_counted"):
            self.assertScenario("rule/walk_never_enters_an_explicit_row" + suffix)

    def test_rule_walk_style_off_builds_no_rule_map(self):
        self.assertScenario("rule/walk_style_off_builds_no_rule_map")

    def test_rule_walk_due_again_when_style_turns_on(self):
        for suffix in ("/first_due_while_off", "/walk_recorded_off", ""):
            self.assertScenario("rule/walk_due_again_when_style_turns_on" + suffix)

    def test_rule_walk_not_due_when_off_and_rows_resolved(self):
        self.assertScenario("rule/walk_not_due_when_off_and_rows_resolved")


if __name__ == "__main__":
    unittest.main()
