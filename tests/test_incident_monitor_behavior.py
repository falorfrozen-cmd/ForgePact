"""The incident monitor (issue #76): crash, freeze and FPS-drop detection, without a game.

tests/incident_monitor_harness.cpp compiles plugin/include/ForgePact/
IncidentMonitor.hpp whole and drives its Detector on a simulated clock the way
ModuleMain.cpp's monitor thread does: frames arrive, the monitor wakes every
250 ms and judges what it can see. Ordinary play with a zone change must
produce nothing (the baseline); a single 400 ms frame, a 3 s stretch at 2.5x
the usual frame time and 4 s without a frame must each produce exactly one
episode of the right kind (the targets). A freeze is judged once frames come
back: a gap the first frames after it explain with a room change is a load and
produces nothing, a gap that begins in a menu room is a load however long it
lasts (D17), and any other gap that never ends is reported at 15 s. The
installer's tag thunks run for real, and the username scrub and the next-load
crash check are pure functions over text.

The per-mod accounting runs on a clock the harness controls (ForgePact #165):
compiled with /DFORGEPACT_INCIDENT_HARNESS_CLOCK, the header's Qpc() reads the
harness's clock, which moves only when a scenario's Spin(ms) moves it by
exactly that much, at a fixed 10 MHz of its own rather than the host's
counter frequency. So each accounting scenario's charge is exact, and a busy
machine cannot move it: descheduled-not-charged sleeps for real inside a
scope and is charged only the controlled work beside it. That pins the
harness's clock, not the shipped accounting: on the real counter a scope is
still charged the time its thread was descheduled, and one scenario,
real-clock-control, shows it by reading the real counter through the same
seam. It is the positive control that the accounting reads the clock at all;
its bounds are lower bounds only, since preemption only ever adds time. Nothing here needs the machine to
itself, so the module runs beside the rest of the parallel suite.
The accounting charges a mod only for ForgePact's own code (the owner,
2026-10-02): the game original a hook wraps runs inside the guard and is
charged to nobody, and every row, `frame` included, is self time.

ForgePact #151 adds the install cost: three scenarios on the controlled clock
pin the setup's cost line exactly, keep an install outside the setup window
out of it, and check that its five parts add up to the `hooks` value; one on
the real clock (lower bounds only) is the thread snapshot probe's positive
control: it must see the idle threads it just started.

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
    "freeze-menu-room",
    "freeze-menu-room-never-ends",
    "unfocused-suppressed",
    "rate-limit-30s",
    "per-mod-accounting",
    "game-original-excluded",
    "own-work-charged",
    "game-original-outer-clock",
    "own-work-inside-game-original",
    "frame-self-time",
    "descheduled-not-charged",
    "real-clock-control",
    "setup-cost-line",
    "setup-cost-outside-setup",
    "setup-cost-sums",
    "thread-snapshot",
    "game-original-in-mod",
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
            f'cl /nologo /std:c++20 /EHsc /O2 /W4 /DFORGEPACT_INCIDENT_HARNESS_CLOCK /I "{include}" '
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

    # D17 (replan 5): Live 2's FREEZE was the save loading after the slot
    # click in Chose_rm, where no room change follows. Target: a gap that
    # begins in a menu room is a load on both paths, at its end and past
    # kFreezeHoldMs. Each scenario's control runs the same gap outside a
    # menu room and gets its one freeze.
    def test_target_a_gap_that_begins_in_a_menu_room_is_a_load(self):
        for name in ("freeze-menu-room", "freeze-menu-room-never-ends"):
            detail = self.scenario(name)
            self.assertTrue(detail.startswith("0 episode(s)"), detail)
            self.assertIn("in a menu room: a load, not reported", detail)
            self.assertIn("outside a menu room: 1 episode(s) [freeze", detail)

    def test_an_unfocused_game_and_a_second_hitch_are_held_back(self):
        self.scenario("unfocused-suppressed")
        self.scenario("rate-limit-30s")

    # On the controlled clock the charges are exact: density's sixteen 1 ms
    # calls, one timed and counted sixteen times, are 16.0 and not 1.0, and the
    # nested drops scope is counted once.
    def test_the_per_mod_table_orders_by_cost_and_scales_the_sampled_scope(self):
        detail = self.scenario("per-mod-accounting")
        self.assertTrue(detail.startswith(
            "3 frames: density 16.0/16.0 gems 8.0/8.0 miner 2.0/2.0 drops 1.0/1.0 | "), detail)
        self.assertIn("| top density 16.0 ms/frame", detail)

    # The owner, 2026-10-02: a mod is charged only for ForgePact's own code.
    # Baseline: own work around the game's original is charged. Targets: the
    # original is not, whichever clock it would have run on; a hook the game
    # calls from inside it times its own code; every row is self time.
    def test_baseline_a_hooks_own_work_is_charged(self):
        self.assertEqual(self.scenario("own-work-charged"), "frame 15.0 ms: hudlabels 5.0")
        self.scenario("per-mod-accounting")

    def test_target_the_game_original_a_hook_wraps_is_not_charged(self):
        self.assertEqual(self.scenario("game-original-excluded"), "frame 10.0 ms: hudlabels 0.0")
        self.assertEqual(self.scenario("game-original-outer-clock"), "frame 10.0 ms: density 0.0")
        self.assertEqual(self.scenario("own-work-inside-game-original"), "frame 10.0 ms: drops 2.0")

    def test_target_every_row_is_self_time_frame_included(self):
        self.assertEqual(self.scenario("frame-self-time"),
                         "frame 6.0 ms: frame 2.0 density 4.0 | sampled frame 10.0 ms: frame 2.0 density 8.0"
                         " | top frame 2.0 ms/frame")

    # ForgePact #165. Target: the harness's controlled clock is immune to a
    # deschedule. A real Sleep inside a timed scope, beside 5 ms of controlled
    # work, does not reach it, so a busy machine cannot move the accounting
    # scenarios. The shipped accounting reads the real counter and still
    # charges that time (the real-clock control below).
    def test_target_the_controlled_clock_ignores_a_real_sleep_in_a_scope(self):
        detail = self.scenario("descheduled-not-charged")
        self.assertTrue(detail.startswith("frame 5.0 ms: gems 5.0 | slept "), detail)

    # Baseline and positive control: on the real clock the accounting charges
    # the scope at least the time it slept, so it reads the real counter.
    def test_baseline_the_real_clock_charges_at_least_the_time_away(self):
        detail = self.scenario("real-clock-control")
        self.assertIn(" ms on the real clock", detail)
        self.assertNotIn("gems 0.0 |", detail)

    # ForgePact #151. Target: the setup's cost line splits every hook install
    # into its parts, exactly, on the controlled clock: what the live session
    # reads to say whether the detours are the 2.5 s.
    def test_target_the_setup_cost_line_names_every_part(self):
        detail = self.scenario("setup-cost-line")
        self.assertTrue(detail.startswith(
            "incident: setup installs 3, detours 2: resolve 4.0 ms, detour 215.0 ms (worst 120.0 ms fp_tip_draw_text),"
            " log 1.5 ms, rest 1.0 ms, outside installers 8.5 ms | "), detail)
        self.assertIn("| before: incident: setup installs not measured yet |", detail)
        self.assertIn("detours 0: ", detail)
        self.assertIn("(worst 0.0 ms none)", detail)

    # Target: an install after the setup (an on-demand `dropmult`) reaches the
    # session totals and leaves the setup line alone.
    def test_target_an_install_outside_the_setup_counts_only_for_the_session(self):
        detail = self.scenario("setup-cost-outside-setup")
        self.assertIn("incident: setup installs 1, detours 1: ", detail)
        self.assertIn("incident: installs since load 2, detours 2, detour 245.0 ms total, worst 150.0 ms fp_before_setup",
                      detail)
        self.assertIn("| after: incident: installs since load 3, detours 3, detour 445.0 ms total, worst 200.0 ms"
                      " fp_drop_relic", detail)

    def test_target_the_five_parts_add_up_to_the_hooks_value(self):
        self.assertIn("of hooks 117.3", self.scenario("setup-cost-sums"))

    # Positive control for the live split: the snapshot probe, on the real
    # clock, sees the threads this process just started. Lower bounds only.
    def test_baseline_the_thread_snapshot_sees_this_process_threads(self):
        detail = self.scenario("thread-snapshot")
        self.assertTrue(detail.startswith("incident: thread snapshot "), detail)
        self.assertIn(" threads system-wide, ", detail)
        self.assertIn(" in this process | before ", detail)
        self.assertIn("started 4", detail)

    def test_target_a_freeze_inside_a_game_original_says_so(self):
        detail = self.scenario("game-original-in-mod")
        self.assertIn("| in-mod hudlabels (game original)", detail)
        self.assertIn("| after hudlabels | out none", detail)

    def test_the_installer_thunk_tags_the_hook_and_leaves_the_mod_alone(self):
        detail = self.scenario("hook-tag-thunk")
        self.assertIn("inside harness_triple | after none", detail)
        self.assertIn("full table: 128 tagged, 1 untagged", detail)

    def test_the_stat_line_says_what_was_judged(self):
        self.assertIn("overall worst 400.0 ms judged no | worst judged 300.0 ms | slow judged frames 1",
                      self.scenario("worst-judged-vs-overall"))
        detail = self.scenario("stat-line-prefix")
        self.assertTrue(detail.startswith("incident: frames "))
        self.assertIn("| window yes | menu yes | in-hook ", detail)

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
