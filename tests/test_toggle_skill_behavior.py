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

        # The real class/struct/enum, verbatim, minus the include of
        # Common.hpp (the harness supplies the stand-ins Common.hpp would
        # have pulled in) - same shape as test_relic_filter_behavior.py.
        klass = "\n".join(
            line for line in header.split("\n")
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
            implementation(cls.plugin, "static bool ToggleIndicatorFindSlot("),
            implementation(cls.plugin, "static void ToggleIndicatorDrawMarker("),
            implementation(cls.plugin, "static void ToggleIndicatorDraw("),
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
            # instance, with the table they walk.
            implementation(cls.plugin, "static bool N1Numeric("),
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
        ])

        out = ROOT / "build/toggle-skill-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/toggle_skill_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_CONSTANTS", constants)
        code = code.replace("// PRODUCTION_TOGGLESKILL", klass)
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


if __name__ == "__main__":
    unittest.main()
