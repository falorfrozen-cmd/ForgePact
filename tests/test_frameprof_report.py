"""tools/frameprof_report.py: a `frameprof` capture turned into a summary and a page.

The capture files come from the plugin (tests/test_frame_profiler.py covers
those); these tests pin what the tool makes of them: the call tree behind the
icicle chart, how a frame's label picks its colour, the summary lines, that
names from the game are escaped on the page, and that without a path it finds
the newest capture of the game the panel is configured for.
"""
import html
import importlib.util
import json
import os
import tempfile
import time
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location("frameprof_report", ROOT / "tools" / "frameprof_report.py")
report_tool = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(report_tool)


def sample_report(**overrides):
    report = {
        "schema": "forgepact-frameprof/1",
        "tool": "ForgePact test",
        "finished": "2026-09-28T12:00:00",
        "game": {"module": "Hero_Siege.exe", "bytes": 1, "gmlEntries": 20925, "gmlNamed": 20000, "builtinsNamed": 300},
        "capture": {"requestedSeconds": 30.0, "seconds": 30.0, "hz": 250, "hzLowest": 250, "rateCuts": 0,
                    "pauseBudgetPercent": 3.0, "samples": 7500, "effectiveHz": 250.0, "missed": 0,
                    "stackOutside": 0, "truncatedStacks": 0, "pauseUsAvg": 70.0, "pauseUsMax": 400.0,
                    "pausePercentOfTime": 1.75, "timer": "high-resolution", "uniqueStacks": 900,
                    "walkEnds": {"complete": 7500}, "endedEarly": ""},
        "frames": {"count": 1500, "fps": 50.0, "msMean": 20.0, "msP50": 18.0, "msP90": 25.0, "msP95": 30.0,
                   "msP99": 60.0, "msMax": 180.0, "over33ms": 40, "over50ms": 6, "over100ms": 2, "over250ms": 0},
        "time": {"workingPercent": 80.0, "waitingPercent": 20.0},
        "buckets": [
            {"key": "game", "name": "game code", "samples": 4500, "percent": 60.0},
            {"key": "graphics", "name": "graphics driver", "samples": 750, "percent": 10.0},
            {"key": "runtime", "name": "GameMaker runtime", "samples": 750, "percent": 10.0},
            {"key": "idle", "name": "idle (frame limiter)", "samples": 1500, "percent": 20.0},
            {"key": "mods", "name": "mods (plugins)", "samples": 0, "percent": 0.0},
        ],
        "gmlTotal": [{"name": "Enemy_Health_Bar_Parent_obj Draw GUI", "samples": 900, "percent": 12.0, "percentOfWorking": 15.0},
                     {"name": "DrawEnemyHealthBars", "samples": 850, "percent": 11.3, "percentOfWorking": 14.2}],
        "gmlSelf": [{"name": "DrawEnemyHealthBars", "samples": 700, "percent": 9.3, "percentOfWorking": 11.7}],
        "events": [{"name": "Enemy_Health_Bar_Parent_obj Draw GUI", "samples": 900, "percent": 12.0, "percentOfWorking": 15.0},
                   {"name": "<script>alert(1)</script>", "samples": 10, "percent": 0.1, "percentOfWorking": 0.2}],
        "builtins": [{"name": "draw_sprite_ext()", "samples": 300, "percent": 4.0, "percentOfWorking": 5.0}],
        "leafFunctions": [{"name": "Hero_Siege.exe!0x1A2B3C", "samples": 500, "percent": 6.7, "percentOfWorking": 8.3}],
        "leafModules": [{"name": "Hero_Siege.exe", "samples": 5000, "percent": 66.7, "percentOfWorking": 83.3}],
        "hitches": [{"atSeconds": 12.5, "ms": 180.0, "room": "Act_01_01", "samples": 45,
                     "gmlTotal": [{"name": "ZoneGenPopulatePresetObjects", "samples": 40}],
                     "events": [], "buckets": []}],
        "timeline": [{"second": 0, "frames": 50, "msMean": 20.0, "msMax": 40.0, "samples": 250, "workingPercent": 80.0,
                      "room": "Act_01_01", "instances": 3000, "monsters": 400},
                     {"second": 1, "frames": 49, "msMean": 20.4, "msMax": 180.0, "samples": 250, "workingPercent": 85.0}],
        "threads": {"frameThreadPercentOfCore": 82.0, "otherThreadsPercentOfCore": 30.0, "profilerThreadPercentOfCore": 2.0,
                    "allThreadsPercentOfCore": 114.0,
                    "top": [{"id": 1, "name": "", "percentOfCore": 82.0, "frameThread": True, "profiler": False},
                            {"id": 2, "name": "ForgePact frame profiler", "percentOfCore": 2.0, "frameThread": False, "profiler": True}]},
        "files": {"stacks": "cap.stacks.txt", "text": "cap.txt"},
    }
    report.update(overrides)
    return report


STACKS = (
    "RtlUserThreadStart;Controller_obj Step;EnemyStepHandleNew;Hero_Siege.exe!0x10 30\n"
    "RtlUserThreadStart;Controller_obj Step;EnemyStepHandleNew 10\n"
    "RtlUserThreadStart;Enemy_Health_Bar_Parent_obj Draw GUI;DrawEnemyHealthBars;draw_sprite_ext() 20\n"
    "(no frames) 1\n"
    "not a stack line\n"
)


class ReportToolTests(unittest.TestCase):
    def test_the_call_tree_adds_up(self):
        tree = report_tool.build_tree(report_tool.parse_stacks(STACKS))
        self.assertEqual(tree["value"], 61)
        start = tree["children"]["RtlUserThreadStart"]
        self.assertEqual(start["value"], 60)
        step = start["children"]["Controller_obj Step"]
        self.assertEqual(step["value"], 40)
        self.assertEqual(step["children"]["EnemyStepHandleNew"]["value"], 40)
        self.assertEqual(step["children"]["EnemyStepHandleNew"]["children"]["Hero_Siege.exe!0x10"]["value"], 30)
        self.assertEqual(tree["children"]["(no frames)"]["value"], 1)

    def test_labels_pick_their_colour(self):
        kind = report_tool.frame_kind
        self.assertEqual(kind("Enemy_Health_Bar_Parent_obj Draw GUI"), "event")
        self.assertEqual(kind("Controller_obj Begin Step"), "event")
        self.assertEqual(kind("Some_obj User Event 2"), "event")
        self.assertEqual(kind("DrawEnemyHealthBars"), "script")
        self.assertEqual(kind("draw_sprite_ext()"), "builtin")
        self.assertEqual(kind("Hero_Siege.exe!0x1A2B"), "runtime")
        self.assertEqual(kind("nvwgf2umx.dll!0x10"), "graphics")
        self.assertEqual(kind("d3d11.dll!0x10"), "graphics")
        self.assertEqual(kind("BloodPactPlugin.dll!0x10"), "mod")
        self.assertEqual(kind("ntdll.dll!NtDelayExecution"), "system")
        self.assertEqual(kind("unknown code"), "system")

    def test_summary_lines(self):
        lines = report_tool.summary_lines(sample_report())
        text = "\n".join(lines)
        self.assertIn("frames: 1500 at 50.0 fps; median 18.0 ms, 95% under 30.0 ms, worst 180.0 ms; 6 over 50 ms", text)
        self.assertIn("frame thread 82% of one core, every other game thread together 30% of one core", text)
        self.assertIn("heaviest events: Enemy_Health_Bar_Parent_obj Draw GUI 12.0%", text)
        self.assertIn("slow frame 180 ms at 12.5 s in Act_01_01: game code: ZoneGenPopulatePresetObjects (40)", text)
        self.assertNotIn("slowed", text)
        gpu = sample_report()
        gpu["hitches"][0]["buckets"] = [{"name": "waiting for the GPU / display", "samples": 36}]
        self.assertIn("mostly waiting for the GPU / display (36 of 45 samples); game code:",
                      "\n".join(report_tool.summary_lines(gpu)))
        cut = sample_report()
        cut["capture"].update(rateCuts=2, hzLowest=62)
        self.assertIn("sampling slowed to 62/s", "\n".join(report_tool.summary_lines(cut)))

    def test_the_page_escapes_names_and_has_every_section(self):
        page = report_tool.build_html(sample_report(), report_tool.parse_stacks(STACKS), "Frame profile - test")
        self.assertNotIn("<script>", page)
        self.assertIn("&lt;script&gt;alert(1)&lt;/script&gt;", page)
        text = html.unescape(page)
        for section in ("Where the frame thread's time goes", "Heaviest events", "Heaviest game code",
                        "Game code's own time", "Heaviest built-ins", "Per second", "Slowest frames",
                        "Call stacks", "CPU per thread"):
            self.assertIn(section, text)
        self.assertIn("(frame thread)", page)
        self.assertNotIn("http", page.split("<body>", 1)[1], "the page must load nothing from the network")

    def test_an_unknown_monster_count_is_left_out_of_the_line(self):
        svg = report_tool.timeline_svg(sample_report()["timeline"])
        dashed = [p for p in svg.split("<polyline")[1:] if "stroke-dasharray" in p.split("/>")[0]]
        self.assertEqual(len(dashed), 1)
        points = dashed[0].split("points='")[1].split("'")[0].split()
        self.assertEqual(len(points), 1, "only the second that carried a reading is plotted")

    def test_it_refuses_a_file_that_is_not_a_capture(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "other.json"
            path.write_text(json.dumps({"schema": "something-else"}), encoding="utf-8")
            with self.assertRaises(SystemExit):
                report_tool.load(path)

    def test_without_a_path_it_takes_the_newest_capture_of_the_configured_game(self):
        with tempfile.TemporaryDirectory() as tmp:
            local = Path(tmp) / "local"
            (local / "Hero_Siege").mkdir(parents=True)
            game = Path(tmp) / "game" / "bin"
            perf = game / "bp_ipc" / "perf"
            perf.mkdir(parents=True)
            (local / "Hero_Siege" / "forgepact.json").write_text(
                json.dumps({"game_exe": str(game / "Hero_Siege.exe")}), encoding="utf-8")
            older = perf / "frameprof-20260928-100000.json"
            newer = perf / "frameprof-20260928-110000.json"
            older.write_text("{}", encoding="utf-8")
            newer.write_text("{}", encoding="utf-8")
            now = time.time()
            os.utime(older, (now - 100, now - 100))
            os.utime(newer, (now, now))
            with mock.patch.dict(os.environ, {"LOCALAPPDATA": str(local)}):
                self.assertEqual(report_tool.default_capture(), newer)

    def test_main_writes_the_page_beside_the_capture(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture = Path(tmp) / "cap.json"
            capture.write_text(json.dumps(sample_report()), encoding="utf-8")
            (Path(tmp) / "cap.stacks.txt").write_text(STACKS, encoding="utf-8")
            with mock.patch("sys.stdout"):
                self.assertEqual(report_tool.main([str(capture)]), 0)
            page = (Path(tmp) / "cap.html").read_text(encoding="utf-8")
            self.assertIn("Enemy_Health_Bar_Parent_obj Draw GUI", page)
            self.assertIn("<svg", page)


PAGE_SECTIONS = ("Where the frame thread's time goes", "Heaviest events", "Heaviest game code",
                 "Game code's own time", "Heaviest built-ins", "Per second", "Slowest frames",
                 "Call stacks", "CPU per thread")


class PhaseBaselineTests(unittest.TestCase):
    """What the tool printed and drew before the runner-phase split: the split
    adds a block after these lines and a section to the page, and changes
    neither."""

    def test_summary_lines_are_unchanged(self):
        self.assertEqual(report_tool.summary_lines(sample_report()), [
            "capture: 30.0 s, 7500 samples at 250/s",
            "frames: 1500 at 50.0 fps; median 18.0 ms, 95% under 30.0 ms, worst 180.0 ms; 6 over 50 ms, 2 over 100 ms",
            "frame thread: working 80.0%, waiting 20.0%; CPU: frame thread 82% of one core, "
            "every other game thread together 30% of one core",
            "time split: game code 60.0%; graphics driver 10.0%; GameMaker runtime 10.0%; idle (frame limiter) 20.0%",
            "heaviest events: Enemy_Health_Bar_Parent_obj Draw GUI 12.0% | <script>alert(1)</script> 0.1%",
            "heaviest game code: Enemy_Health_Bar_Parent_obj Draw GUI 12.0% | DrawEnemyHealthBars 11.3%",
            "game code's own time: DrawEnemyHealthBars 9.3%",
            "heaviest built-ins: draw_sprite_ext() 4.0%",
            "slow frame 180 ms at 12.5 s in Act_01_01: game code: ZoneGenPopulatePresetObjects (40)",
            "profiler cost: at most 70 us per sample, at most 1.75% of the frame thread's time",
        ])

    def test_the_page_keeps_every_section(self):
        text = html.unescape(report_tool.build_html(sample_report(), report_tool.parse_stacks(STACKS), "t"))
        positions = [text.index(section) for section in PAGE_SECTIONS]
        self.assertEqual(positions, sorted(positions), "the sections keep their order")


# A capture shaped like the game's: a frame loop whose per-frame function has
# a step branch and a draw branch, and whose other branch holds the frame
# limiter (spinning, and reading the clock) and a wait on the display. 200
# samples, so every share below is a round number.
LOOP = "unknown code;Hero_Siege.exe!0x100;Hero_Siege.exe!0x200"
PER_FRAME = LOOP + ";Hero_Siege.exe!0x300"
PHASE_STACKS = "".join(f"{line}\n" for line in (
    f"{PER_FRAME};Hero_Siege.exe!0x400;Hero_Siege.exe!0x500;A_obj Step;ScriptA;Hero_Siege.exe!0x510 40",
    f"{PER_FRAME};Hero_Siege.exe!0x400;Hero_Siege.exe!0x500;Hero_Siege.exe!0x530 10",
    f"{PER_FRAME};Hero_Siege.exe!0x400;Hero_Siege.exe!0x520 30",
    f"{PER_FRAME};Hero_Siege.exe!0x600;Hero_Siege.exe!0x700;B_obj Draw GUI;DrawScript;d3d11.dll!0x10 20",
    f"{PER_FRAME};Hero_Siege.exe!0x600;Hero_Siege.exe!0x700;B_obj Draw GUI;DrawScript 20",
    f"{PER_FRAME};Hero_Siege.exe!0x600;Hero_Siege.exe!0x720 20",
    f"{LOOP};Hero_Siege.exe!0x800;Hero_Siege.exe!0x900 30",
    f"{LOOP};Hero_Siege.exe!0x800;Hero_Siege.exe!0x900;ntdll.dll!RtlQueryPerformanceCounter 10",
    f"{LOOP};Hero_Siege.exe!0x800;dxgi.dll!0x20;ntdll.dll!NtWaitForSingleObject 20",
))
PHASE_BUCKETS = {"game": 60, "game_wait": 0, "graphics": 20, "gpu_wait": 20, "mods": 0, "runtime": 60,
                 "idle": 0, "spin": 40, "unknown": 0}


def phase_report(**counts):
    """sample_report() with the bucket counts the plugin would have written for
    PHASE_STACKS, any of them overridden, and its frame limiter."""
    buckets = dict(PHASE_BUCKETS, **counts)
    total = sum(buckets.values())
    return sample_report(
        buckets=[{"key": k, "name": k, "samples": v, "percent": 100.0 * v / total} for k, v in buckets.items()],
        spinWait={"function": "Hero_Siege.exe!0x900", "samples": buckets["spin"], "percent": 20.0})


class PhaseTargetTests(unittest.TestCase):
    """The runner-phase split: where the runner's step phase and draw phase
    start on the stacks, what each costs, and what lies outside both."""

    def test_each_stack_gets_the_plugins_bucket(self):
        bucket = report_tool.stack_bucket
        spin = "Hero_Siege.exe!0x900"
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", "A_obj Step", "ScriptA", "draw_sprite_ext()"], spin), "game")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", "ScriptA", "ntdll.dll!NtWaitForSingleObject"], spin), "game_wait")
        self.assertEqual(bucket(["A_obj Draw", "Hero_Siege.exe!0x1", "nvwgf2umx.dll!0x10"], spin), "graphics")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", "dxgi.dll!0x20", "KERNELBASE.dll!WaitForSingleObjectEx"], spin),
                         "gpu_wait")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", "BloodPactPlugin.dll!0x10"], spin), "mods")
        # Game code under a mod's hook is still game code.
        self.assertEqual(bucket(["BloodPactPlugin.dll!0x10", "A_obj Step", "Hero_Siege.exe!0x1"], spin), "game")
        self.assertEqual(bucket(["unknown code", "Hero_Siege.exe!0x1"], spin), "runtime")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", "ntdll.dll!NtDelayExecution"], spin), "idle")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", spin], spin), "spin")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", spin, "ntdll.dll!RtlQueryPerformanceCounter"], spin), "spin")
        self.assertEqual(bucket(["(no frames)"], spin), "unknown")
        # Negative controls: a clock read is the limiter's only under the
        # limiter and with nothing but runtime on the stack; a wait export in
        # a module that is not Windows' own does not wait; with no limiter in
        # the capture nothing spins.
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", "ntdll.dll!RtlQueryPerformanceCounter"], spin), "runtime")
        self.assertEqual(bucket(["A_obj Step", spin, "ntdll.dll!RtlQueryPerformanceCounter"], spin), "game")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", "Hero_Siege.exe!WaitForThing"], spin), "runtime")
        self.assertEqual(bucket(["Hero_Siege.exe!0x1", spin], None), "runtime")

    def test_both_phases_are_found_with_their_shares_and_callees(self):
        lines = report_tool.phase_lines(phase_report(), report_tool.parse_stacks(PHASE_STACKS))
        self.assertEqual(lines, [
            "runner phases:",
            "  step phase: Hero_Siege.exe!0x400, called from Hero_Siege.exe!0x300: 40.0% of samples "
            "(game 20.0%, runtime 20.0%)",
            "    Hero_Siege.exe!0x500 25.0%, runtime only 5.0%",
            "    Hero_Siege.exe!0x520 15.0%, runtime only 15.0%",
            "  draw phase: Hero_Siege.exe!0x600, called from Hero_Siege.exe!0x300: 30.0% of samples "
            "(game 10.0%, graphics 10.0%, runtime 10.0%)",
            "    Hero_Siege.exe!0x700 20.0%, runtime only 0.0%",
            "    Hero_Siege.exe!0x720 10.0%, runtime only 10.0%",
            "  outside the phases: 30.0% of samples (gpu_wait 10.0%, spin 20.0%)",
            "runner phases: buckets agree with the capture",
        ])

    def test_the_derived_buckets_total_the_captures(self):
        derived = report_tool.derived_buckets(phase_report(), report_tool.parse_stacks(PHASE_STACKS))
        self.assertEqual(derived, PHASE_BUCKETS)

    def test_a_bucket_that_disagrees_is_named(self):
        lines = report_tool.phase_lines(phase_report(runtime=57), report_tool.parse_stacks(PHASE_STACKS))
        self.assertEqual(lines[-1], "runner phases: buckets differ from the capture by 3 samples "
                                    "(runtime tool 60 capture 57)")

    def test_a_mixed_node_is_no_dispatcher(self):
        # Under 0x400 the events are 90% step and 10% draw: below the 95%
        # rule, so the step phase is not found there or anywhere, while the
        # pure draw branch beside it still is.
        def stacks(step, draw):
            return report_tool.parse_stacks(
                f"{LOOP};Hero_Siege.exe!0x400;Hero_Siege.exe!0x500;A_obj Step;ScriptA {step}\n"
                f"{LOOP};Hero_Siege.exe!0x400;Hero_Siege.exe!0x500;C_obj Draw;ScriptC {draw}\n"
                f"{LOOP};Hero_Siege.exe!0x600;B_obj Draw GUI;DrawScript 100\n")
        lines = report_tool.phase_lines(phase_report(), stacks(90, 10))
        self.assertIn("  step phase: not found", lines)
        self.assertTrue(any(l.startswith("  draw phase: Hero_Siege.exe!0x600, called from Hero_Siege.exe!0x200:")
                            for l in lines), lines)
        # Positive control: at exactly 95% the same node is the step phase.
        lines = report_tool.phase_lines(phase_report(), stacks(95, 5))
        self.assertTrue(any(l.startswith("  step phase: Hero_Siege.exe!0x400, called from Hero_Siege.exe!0x200:")
                            for l in lines), lines)
        self.assertEqual(report_tool.DISPATCHER_PURITY_PERCENT, 95)
        self.assertEqual(report_tool.DISPATCHER_MIN_PERCENT, 1)

    def test_a_phase_inside_the_other_is_counted_once(self):
        # A draw dispatcher whose few step events sit under one callee: that
        # callee is the step phase too, and its samples are inside the phases
        # once, so nothing is left outside.
        stacks = report_tool.parse_stacks(
            f"{LOOP};Hero_Siege.exe!0x600;B_obj Draw GUI;DrawScript 96\n"
            f"{LOOP};Hero_Siege.exe!0x600;Hero_Siege.exe!0x610;A_obj Step;ScriptA 4\n")
        lines = report_tool.phase_lines(phase_report(), stacks)
        self.assertIn("  step phase: Hero_Siege.exe!0x610, called from Hero_Siege.exe!0x600: 4.0% of samples "
                      "(game 4.0%)", lines)
        self.assertIn("  outside the phases: 0.0% of samples", lines)

    def test_events_straight_under_the_thread_start_find_nothing(self):
        lines = report_tool.phase_lines(sample_report(), report_tool.parse_stacks(STACKS))
        self.assertEqual(lines[0], "runner phases: not found (no step or draw dispatcher on the stacks)")
        self.assertNotIn("step phase", "\n".join(lines))

    def test_no_stacks_file_finds_nothing(self):
        self.assertEqual(report_tool.phase_lines(phase_report(), None),
                         ["runner phases: not found (no stacks file)"])

    def test_the_page_has_a_runner_phases_section(self):
        page = html.unescape(report_tool.build_html(phase_report(), report_tool.parse_stacks(PHASE_STACKS), "t"))
        section = page[page.index("Runner phases"):]
        for text in ("Hero_Siege.exe!0x400", "Hero_Siege.exe!0x600", "Outside the phases",
                     "buckets agree with the capture"):
            self.assertIn(text, section)
        empty = html.unescape(report_tool.build_html(sample_report(), [], "t"))
        self.assertIn("not found (no stacks file)", empty[empty.index("Runner phases"):])

    def test_main_prints_the_block_after_the_summary(self):
        with tempfile.TemporaryDirectory() as tmp:
            capture = Path(tmp) / "cap.json"
            capture.write_text(json.dumps(phase_report()), encoding="utf-8")
            (Path(tmp) / "cap.stacks.txt").write_text(PHASE_STACKS, encoding="utf-8")
            with mock.patch("builtins.print") as printed:
                self.assertEqual(report_tool.main([str(capture), "--no-html"]), 0)
            out = [c.args[0] for c in printed.call_args_list]
            summary = report_tool.summary_lines(phase_report())
            self.assertEqual(out[:len(summary)], summary)
            self.assertEqual(out[len(summary)], "runner phases:")
            self.assertEqual(out[-1], "runner phases: buckets agree with the capture")
            (Path(tmp) / "cap.stacks.txt").unlink()
            with mock.patch("builtins.print") as printed:
                report_tool.main([str(capture), "--no-html"])
            self.assertEqual(printed.call_args_list[-1].args[0], "runner phases: not found (no stacks file)")


if __name__ == "__main__":
    unittest.main()
