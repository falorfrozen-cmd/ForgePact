"""The incident monitor (issue #76): crash, freeze and FPS-drop detection, without a game.

tests/incident_monitor_harness.cpp compiles plugin/include/ForgePact/
IncidentMonitor.hpp whole and drives its Detector on a simulated clock the way
ModuleMain.cpp's monitor thread does: frames arrive, the monitor wakes every
250 ms and judges what it can see. Ordinary play with a zone change must
produce nothing (the baseline); a single 400 ms frame, a 3 s stretch at 2.5x
the usual frame time and 4 s without a frame must each produce exactly one
episode of the right kind (the targets). A freeze is judged once frames come
back: a gap the first frames after it explain with a room change is a load and
produces nothing, and a gap that never ends is reported at 15 s. The per-mod
accounting and the installer's tag thunks run for real against the clock, and
the username scrub and the next-load crash check are pure functions over text.

tests/incident_shutdown_probe.cpp is a DLL that arms the clean-shutdown marker
and starts the monitor's thread, as the plugin does. The harness loads it in a
child that ends through ExitProcess (the marker's second writer, the static
destructor, must write it) or TerminateProcess (it must not, as after a
crash). The probe is built with /MD /LD, as the plugin is.

tests/fixtures/incident/ holds the exit.json and panel.json the panel writes;
the harness reads them through the plugin's own parsers.

Each scenario prints `<name> | <pass|fail> | <detail>`. Skips, with a message,
only where there is no MSVC. Writes only under build/incident-monitor-behavior.
"""
import json
import os
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build" / "incident-monitor-behavior"
FIXTURES = ROOT / "tests" / "fixtures" / "incident"

EXPECTED = (
    "steady-60fps-with-zone-change",
    "single-400ms-frame",
    "sustained-2.5x-3s",
    "freeze-4s-in-hook",
    "freeze-4s-no-hook",
    "freeze-load-room-change",
    "freeze-never-ends",
    "unfocused-suppressed",
    "rate-limit-30s",
    "per-mod-accounting",
    "worst-judged-vs-overall",
    "hook-tag-thunk",
    "stat-line-prefix",
    "scrub-username",
    "exit-clean",
    "exit-terminated",
    "marker-once",
    "crash-check",
    "bundle-retention",
    "exit-json-fixture",
    "report-json",
)


def find_msvc():
    finder = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    if not finder.is_file():
        return None
    install = subprocess.check_output(
        [str(finder), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        text=True).strip()
    return install or None


class IncidentMonitorBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name != "nt":
            raise unittest.SkipTest("the monitor and the exit under test are Win32's")
        install = find_msvc()
        if not install:
            raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
        OUTPUT.mkdir(parents=True, exist_ok=True)
        include = ROOT / "plugin" / "include"
        cls.harness = OUTPUT / "incident_monitor_harness.exe"
        cls.probe = OUTPUT / "incident_shutdown_probe.dll"
        vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
        script = OUTPUT / "compile.cmd"
        script.write_text(
            f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{include}" '
            f'"{ROOT / "tests/incident_monitor_harness.cpp"}" /Fe:"{cls.harness}" '
            f'/Fo:"{OUTPUT / "incident_monitor_harness.obj"}" /link /INCREMENTAL:NO\n'
            'if errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /MD /LD /I "{include}" '
            f'"{ROOT / "tests/incident_shutdown_probe.cpp"}" /Fe:"{cls.probe}" '
            f'/Fo:"{OUTPUT / "incident_shutdown_probe.obj"}"\n'
            'exit /b %errorlevel%\n', encoding="utf-8")
        result = subprocess.run(["cmd", "/d", "/c", str(script)], cwd=OUTPUT, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        (OUTPUT / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError("compile failed:\n" + result.stdout + result.stderr)
        run = subprocess.run([str(cls.harness), str(cls.probe), str(OUTPUT / "work"), str(FIXTURES)], capture_output=True,
                             text=True, encoding="utf-8", errors="replace", timeout=180)
        (OUTPUT / "run.log").write_text(f"exit {run.returncode}\n" + run.stdout + run.stderr, encoding="utf-8")
        cls.output = run.stdout
        cls.returncode = run.returncode
        cls.results = {}
        for line in run.stdout.splitlines():
            parts = [p.strip() for p in line.split(" | ", 2)]
            if len(parts) == 3:
                cls.results[parts[0]] = (parts[1], parts[2])

    def scenario(self, name):
        self.assertIn(name, self.results, f"{name} did not run:\n{self.output}")
        verdict, detail = self.results[name]
        self.assertEqual(verdict, "pass", f"{name}: {detail}")
        return detail

    def test_every_scenario_ran_and_passed(self):
        self.assertEqual(sorted(self.results), sorted(EXPECTED), self.output)
        for name, (verdict, detail) in self.results.items():
            self.assertEqual(verdict, "pass", f"{name}: {detail}")
        self.assertEqual(self.returncode, 0, self.output)
        self.assertIn("RESULT OK", self.output)

    # Baseline: normal play, with a zone change's loading frames, reports nothing.
    def test_baseline_steady_play_with_a_zone_change_is_quiet(self):
        self.assertIn("0 episode(s)", self.scenario("steady-60fps-with-zone-change"))

    # Targets: one episode each, of the right kind.
    def test_target_a_single_slow_frame_is_one_hitch(self):
        self.assertIn("PERF hitch 400 ms frame", self.scenario("single-400ms-frame"))

    def test_target_a_sustained_slowdown_is_one_episode_and_no_hitch(self):
        self.assertIn("PERF sustained", self.scenario("sustained-2.5x-3s"))

    def test_target_a_freeze_names_the_hook_and_the_mod_the_frame_thread_was_in(self):
        detail = self.scenario("freeze-4s-in-hook")
        self.assertIn("in-hook harness_hook | in-mod mapreveal", detail)
        # Judged when frames came back, not while the gap lasted.
        self.assertIn("reported while frozen 0", detail)
        self.assertIn("in-hook none | in-mod none", self.scenario("freeze-4s-no-hook"))

    # Baseline: a load's gap, explained by the room change the first frames
    # after it see, is no freeze. Target: a gap that never ends is reported.
    def test_a_load_is_not_a_freeze_and_a_freeze_that_never_ends_is_reported(self):
        self.assertIn("after a room change: a load, not reported", self.scenario("freeze-load-room-change"))
        self.assertIn("1 episode(s) [freeze", self.scenario("freeze-never-ends"))

    def test_an_unfocused_game_and_a_second_hitch_are_held_back(self):
        self.scenario("unfocused-suppressed")
        self.scenario("rate-limit-30s")

    def test_the_per_mod_table_orders_by_cost_and_scales_the_sampled_scope(self):
        self.assertIn("top density", self.scenario("per-mod-accounting"))

    def test_the_installer_thunk_tags_the_hook_and_leaves_the_mod_alone(self):
        detail = self.scenario("hook-tag-thunk")
        self.assertIn("inside harness_triple | after none", detail)
        self.assertIn("full table: 128 tagged, 1 untagged", detail)

    def test_the_stat_line_says_what_was_judged(self):
        self.assertIn("overall worst 400.0 ms judged no | worst judged 300.0 ms | slow judged frames 1",
                      self.scenario("worst-judged-vs-overall"))
        self.assertTrue(self.scenario("stat-line-prefix").startswith("incident: frames "))

    def test_the_username_never_reaches_a_report(self):
        self.scenario("scrub-username")

    # Target: ExitProcess leaves the marker. Its control: TerminateProcess,
    # after the same arming, does not. The probe has no exit hook, so the
    # line is the static destructor's, which names its route.
    def test_target_a_clean_exit_writes_the_marker_and_a_terminated_one_does_not(self):
        self.assertIn("marker written", self.scenario("exit-clean"))
        self.assertIn("marker absent", self.scenario("exit-terminated"))
        marker = OUTPUT / "work" / "exit" / "exit-clean.txt"
        self.assertEqual(marker.read_text(encoding="utf-8").strip(), "==== clean shutdown (detach) ====")

    def test_the_marker_is_written_once_by_whichever_route_comes_first(self):
        detail = self.scenario("marker-once")
        self.assertIn("route exit hook: written, detach after it: nothing", detail)
        marker = OUTPUT / "work" / "marker" / "marker-once.txt"
        self.assertEqual(marker.read_text(encoding="utf-8").splitlines(), ["==== clean shutdown ===="])

    def test_the_next_load_check_finds_the_previous_session(self):
        self.scenario("crash-check")
        self.scenario("bundle-retention")

    def test_the_plugin_reads_the_panels_files_from_the_shared_fixture(self):
        self.assertIn("exit_code 0xC0000005, faulting_module KERNELBASE.dll, version 2.2.0, pid 4242",
                      self.scenario("exit-json-fixture"))

    def test_the_report_files_are_json(self):
        paths = self.scenario("report-json").split(";")
        reports = {Path(p).stem: json.loads(Path(p).read_text(encoding="utf-8")) for p in paths}
        self.assertEqual(sorted(reports), ["crash", "freeze", "perf"])
        perf = reports["perf"]
        self.assertEqual(perf["kind"], "perf")
        self.assertEqual(perf["episode"], "sustained")
        self.assertEqual(perf["frames"]["room"], 'Act "01"\\01')
        self.assertEqual(perf["frames"]["monsters"], 78)
        self.assertEqual(perf["modsEpisode"][0], {"mod": "density", "avgMs": 3.25, "worstMs": 6.5})
        self.assertIsNone(perf["panelVersion"])
        # A PERF episode names no tag: the per-mod tables answer that.
        self.assertIsNone(perf["frames"]["inHook"])
        self.assertIsNone(perf["frames"]["inMod"])
        freeze = reports["freeze"]
        self.assertEqual(freeze["kind"], "freeze")
        self.assertEqual(freeze["frames"]["inMod"], "mapreveal")
        self.assertEqual(freeze["frames"]["inHook"], "none")
        self.assertEqual(freeze["panelVersion"], "2.2.0")
        crash = reports["crash"]
        self.assertEqual(crash["kind"], "crash")
        # Found at the next load, where neither tag is knowable.
        self.assertEqual(crash["frames"]["inHook"], "unknown")
        self.assertEqual(crash["frames"]["inMod"], "unknown")
        self.assertEqual(crash["exit"]["exit_code"], "0xC0000005")
        self.assertTrue(crash["exit"]["event_probe"]["queried"])
        self.assertTrue(crash["previousSession"].startswith("==== BloodPact plugin loaded ===="))


if __name__ == "__main__":
    unittest.main()
