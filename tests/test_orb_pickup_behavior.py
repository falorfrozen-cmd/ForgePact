"""Run the real globe pull against a controlled game API.

Companion to orb_pickup_harness.cpp. `orbpickup` enumerated every instance of
every globe type on every frame - up to ~196 CallBuiltins per frame in the
worst case - to move a handful of objects that barely change between frames.

The scan is now throttled and the pull is not, which is the part worth
testing: the constant-speed glide was itself a fix to a user report
(2026-09-10, "make the orbs come a little slower"), so the baseline scenarios
pin the movement maths exactly - kGlobePullSpeed px per frame, and the last
step landing on the player rather than past it - while the target scenarios
pin the call counts and the bounded lateness that pays for them.
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


class OrbPickupBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")

        constants = "\n".join([
            declaration(cls.plugin, "static constexpr double kGlobeBaseRadius"),
            declaration(cls.plugin, "static constexpr double kOrbPickupFactor"),
            declaration(cls.plugin, "static constexpr double kGlobePullSpeed"),
            declaration(cls.plugin, "static const uint32_t kOrbScanFrames"),
            declaration(cls.plugin, "static constexpr double kOrbApproachMargin"),
            declaration(cls.plugin, "static std::vector<RValue> g_OrbCachedGlobes"),
            declaration(cls.plugin, "static uint32_t g_OrbNextScanFrame"),
            declaration(cls.plugin, "static int64_t g_OrbCacheRoom"),
            declaration(cls.plugin, "static double g_OrbScanPlayerX"),
            declaration(cls.plugin, "static bool g_OrbScanPlayerValid"),
        ])
        production = "\n".join([
            implementation(cls.plugin, "static int64_t CurrentRoomKey()"),
            implementation(cls.plugin, "static void OrbCacheReset()"),
            implementation(cls.plugin, "static bool OrbCacheableHandle("),
            implementation(cls.plugin, "static void PullOneGlobe("),
            implementation(cls.plugin, "static void OrbScan()"),
            implementation(cls.plugin, "static void OrbPickupTick("),
        ])

        out = ROOT / "build/orb-pickup-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/orb_pickup_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_CONSTANTS", constants)
        code = code.replace("// PRODUCTION_ORB", production)
        cpp = out / "orbpickup.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("orbpickup.exe" if os.name == "nt" else "orbpickup")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "orbpickup.obj"}"\n'
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

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_the_glide_is_a_constant_speed(self):
        # The user report this fixed: the old proportional step sped up as the
        # globe closed in and snapped the last stretch in one frame, which
        # "read as an unnatural teleport right before pickup".
        self.assertScenario("pull/one_frame_step")
        self.assertScenario("pull/two_frame_step")

    def test_the_last_step_lands_exactly_on_the_player(self):
        self.assertScenario("pull/no_overshoot_x")
        self.assertScenario("pull/no_overshoot_y")

    def test_a_globe_outside_reach_is_never_moved(self):
        self.assertScenario("out_of_reach/not_moved")
        self.assertScenario("out_of_reach/not_pulled")

    def test_no_player_position_moves_nothing_and_says_so(self):
        # Known Limitation 7: `seen=176993 noplayer=176993` is what turned a
        # feature that reported itself ON into a diagnosis. The counter has to
        # keep incrementing after the scan moved.
        self.assertScenario("no_player/not_moved")
        self.assertScenario("no_player/globes_seen")
        self.assertScenario("no_player/noplayer_counted")

    def test_the_enumeration_is_throttled_but_the_pull_is_not(self):
        # Both numbers measured in the same run, the pre-change shape against
        # the shipped one.
        self.assertScenario("scan15/instance_number_before")
        self.assertScenario("scan15/instance_find_before")
        self.assertScenario("scan15/instance_number_after")
        self.assertScenario("scan15/instance_find_after")
        self.assertScenario("scan15/still_pulled_every_frame")

    def test_a_globe_that_comes_into_reach_between_scans_is_still_pulled(self):
        # The accepted cost: up to kOrbScanFrames frames later than before.
        # Never missed, and the glide itself is identical.
        #
        # `band/was_pulled` is here because `pulled_within_scan_period` alone
        # cannot tell a bounded pull from no pull at all (-1 <= 15 prints
        # PASS). It is a guard, NOT a witness: it does not fail against the
        # unthrottled code, where the teleport is picked up by the next scan
        # and the whole scenario passes. Its failing direction is reachable -
        # `got=0 want=1` with the cache disabled - which is what makes it worth
        # keeping, but do not read it as evidence that the throttle works.
        self.assertScenario("band/not_cached_when_far")
        self.assertScenario("band/was_pulled")
        self.assertScenario("band/pulled_within_scan_period")
        self.assertScenario("band/no_approach_not_moved")
        self.assertScenario("band/no_approach_not_pulled")

    def test_a_globe_collected_between_scans_is_not_written_to(self):
        self.assertScenario("destroyed/not_written")
        self.assertScenario("destroyed/others_still_pulled")

    def test_a_room_change_drops_the_cache(self):
        self.assertScenario("room_change/rescanned")
        self.assertScenario("room_change/new_globe_pulled")

    def test_the_player_resolving_again_forces_a_rescan(self):
        # Observed failing against the round-1 code at
        # `playervalid/pulled_on_next_frame got=16 want=2`: a scan that ran
        # while the player was unresolved cached nothing, and nothing forced
        # another until the frame counter came round. This lands on the one
        # feature whose documented failure mode is intermittent player
        # resolution (Known Limitation 7), and the stat line cannot show it.
        self.assertScenario("playervalid/not_pulled_while_unresolved")
        self.assertScenario("playervalid/was_pulled")
        self.assertScenario("playervalid/pulled_on_next_frame")

    def test_a_handle_that_cannot_outlive_the_frame_is_refused_and_counted(self):
        # The cache is the one cross-frame instance lifetime this change
        # introduces, and its safety rested on a measured property of this
        # runner that nothing in the code enforced. Observed failing against
        # the round-1 code at `uncacheable/refusal_counted got=0 want=1` with a
        # VALUE_OBJECT handle cached and written to.
        #
        # The refusal covers the CACHE, not the work: the globe is still pulled
        # on the scan frame (100 -> 94), just never held across frames. A
        # refusal that skipped the pull would leave orb pickup inert on such a
        # runner while reporting itself on, which is the Known Limitation 7
        # shape the predicate exists to avoid - so the pull is asserted, not
        # merely the absence of a cross-frame write.
        self.assertScenario("uncacheable/pulled_in_scan")
        self.assertScenario("uncacheable/refusal_counted")
        self.assertScenario("uncacheable/still_seen")
        # ...and the negative control beside it: the refusal must not narrow
        # what is accepted to the one kind this runner returns today. These two
        # passed before the refusal existed as well - they are the bound that
        # says it did not go too far, not evidence of the fix.
        self.assertScenario("bareid/pulled")
        self.assertScenario("bareid/not_refused")


class OrbPickupSourceConstraintTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")

    def test_the_seen_counter_is_not_wrapped_in_a_release_guard(self):
        # `orbpickup` is in kPlayerCommands and `orbpickup stat` is handled in
        # every build, so these counters must keep working in the player
        # binary. BP_DIAG_INCREMENT's own comment scopes it to telemetry on hot
        # combat/drop paths; a player-facing stat command is not that, and
        # Known Limitation 7 depends on this output existing in a bug report.
        self.assertIn("InterlockedIncrement(&g_OrbGlobesSeen)", self.plugin)
        self.assertNotIn("BP_DIAG_INCREMENT(&g_OrbGlobesSeen)", self.plugin)
        self.assertNotIn("BP_DIAG_INCREMENT(g_OrbGlobesSeen)", self.plugin)

    def test_the_stat_line_says_what_each_counter_counts(self):
        # The counters under one label have to be counted the same way, or a
        # pasted `orbpickup stat` cannot be read as the ratio Known Limitation
        # 7 was diagnosed from. `pulled` is the one that is still per frame,
        # and the line has to say so rather than leave it to be assumed.
        body = implementation(self.plugin, "static void OrbPickupStats()")
        self.assertIn("scan", body)
        self.assertIn("not per frame", body)
        self.assertIn("pulled=%ld (per frame)", body)

    def test_all_the_scan_clock_counters_are_incremented_in_the_scan(self):
        scan = implementation(self.plugin, "static void OrbScan()")
        pull = implementation(self.plugin, "static void PullOneGlobe(")
        for counter in ("g_OrbGlobesSeen", "g_OrbNoPlayer", "g_OrbOutOfReach"):
            self.assertIn(f"InterlockedIncrement(&{counter})", scan)
            self.assertNotIn(counter, pull)

    def test_the_cache_is_dropped_on_every_event_that_invalidates_it(self):
        # Two moments: the mod being switched off, and a room change (which
        # clears in place, because the new room key has to be kept). Asset
        # re-resolution is deliberately NOT a third - g_OrbAssetsResolved is
        # never cleared, so a reset there could only ever run against an empty
        # cache, and a call that reads as an invalidation point while being
        # unreachable is worse than its absence.
        # Brace-matched, not a fixed [:400] window: a paragraph of comment
        # added to the command handler slides its own code out of a fixed
        # window and the assertion starts measuring the comment instead. The
        # rest of this change moved four files off that pattern; this was the
        # one left behind.
        handler = implementation(self.plugin, "static void RunCommand(const std::string& line)")
        orb = handler[handler.index("g_OrbPickupRadius.store(enable);"):]
        self.assertIn("OrbCacheReset();", orb[:orb.index('} else if (lc == "')])
        self.assertNotIn("OrbCacheReset();", implementation(self.plugin, "static void ResolveOrbAssets()"))
        tick = implementation(self.plugin, "static void OrbPickupTick(")
        self.assertIn("CurrentRoomKey()", tick)
        self.assertIn("g_OrbCachedGlobes.clear();", tick)
        self.assertIn("instance_exists", tick)

    def test_the_scan_is_forced_by_distance_travelled_not_only_by_frames(self):
        # The band is a budget the player can outrun: the panel's Movement
        # Speed control goes to 10x. Without this the mod's widened radius
        # applies only intermittently at speed, and `orbpickup stat` cannot
        # show it - seen>0 and pulled>0 both stay healthy.
        tick = implementation(self.plugin, "static void OrbPickupTick(")
        self.assertIn("g_OrbScanPlayerValid", tick)
        self.assertIn("kOrbScanFrames * kOrbApproachMargin", tick)
        self.assertIn("g_OrbNextScanFrame = frame;", tick)
        self.assertIn("g_OrbScanPlayerX = g_PlayerX;", implementation(self.plugin, "static void OrbScan()"))

    def test_the_scan_is_also_forced_by_the_player_resolving_again(self):
        # The other thing the frame counter cannot see. Behaviour is pinned by
        # the `playervalid/*` scenarios; this pins that the condition is a
        # transition and not, say, a plain `g_PlayerPosValid` test that would
        # force a scan on every frame the player is resolved.
        tick = implementation(self.plugin, "static void OrbPickupTick(")
        self.assertIn("if (!g_OrbScanPlayerValid && g_PlayerPosValid.load()) g_OrbNextScanFrame = frame;", tick)

    def test_a_cached_handle_kind_is_validated_where_it_is_stored(self):
        # And accepted as a SET. Every accessor the cache feeds
        # (instance_exists, variable_instance_get) takes any instance-handle
        # kind straight through, so the check may decide only whether a handle
        # can be STORED - a kind check that decides whether the work happens at
        # all is Known Limitation 7's bug.
        predicate = implementation(self.plugin, "static bool OrbCacheableHandle(")
        for kind in ("VALUE_REF", "VALUE_REAL", "VALUE_INT32", "VALUE_INT64"):
            self.assertIn(kind, predicate)
        self.assertNotIn("VALUE_OBJECT", predicate)
        scan = implementation(self.plugin, "static void OrbScan()")
        self.assertIn("OrbCacheableHandle(inst)", scan)
        self.assertIn("InterlockedIncrement(&g_OrbUncacheableKind)", scan)
        # ...and the refusal is a printed number, not silence.
        self.assertIn("uncacheable=%ld", implementation(self.plugin, "static void OrbPickupStats()"))

    def test_the_stat_line_does_not_promise_a_fixed_scan_period(self):
        # The travel-forced rescan makes a scan fire sooner than
        # kOrbScanFrames - measured at 5 scans per 15 frames at ~10x movement
        # speed - so "once per 15-frame scan" would be wrong by up to 5x in
        # exactly the case the fix exists for. The ratio between the counters
        # is what the decision tree reads and it is still sound; the period is
        # not, and the line must not imply one.
        body = implementation(self.plugin, "static void OrbPickupStats()")
        self.assertNotIn("%u-frame scan", body)
        self.assertIn("at most every %u frames", body)

    def test_the_arming_line_does_not_claim_the_mod_is_driven_per_frame(self):
        # Found in a real session log, not by a test: the arming line still read
        # "driven per frame" after the enumeration was throttled. The pull is
        # per frame; the scan that finds globes is not. Same defect as the stat
        # line above - a diagnostic describing work the code no longer does -
        # and the stat line was corrected twice before anyone read this one.
        self.assertNotIn("driven per frame", self.plugin)
        self.assertIn("pulled every frame, found by a scan at most every %u frames", self.plugin)

    def test_the_movement_maths_is_untouched(self):
        body = implementation(self.plugin, "static void PullOneGlobe(")
        self.assertIn("(kGlobePullSpeed >= d) ? 1.0 : (kGlobePullSpeed / d)", body)


if __name__ == "__main__":
    unittest.main()
