"""Run the real number guard (`numguard`, #230) in the game's place, against controlled game API responses.

Companion to test_number_guard_contract.py, which reads the source.

The game's number shortener (num_compressSteam, which calls
SteamCompressNumber in PAS_Steam_GMS_x64.dll) writes into 8 bytes and ends
the process for any number of 100 trillion or more, a million or more below
zero, or NaN. tests/number_guard_harness.cpp plays that shortener, the
Modifiers levers that arm the guard (the real StatsManager) and ModuleMain's
real `numguard` dispatch, and reports what the shortener was handed and what
it showed. It is compiled twice: the player build (FORGEPACT_RELEASE) and the
research build.

Scenario names (criterion 3 of the workorder):
  baseline/not_armed_passes_through, baseline/in_range_unchanged_when_armed,
  target/positive_overflow_capped, target/negative_overflow_capped,
  target/nan_capped_to_zero, target/caller_argument_never_written,
  target/arms_from_levers_once, target/table_only_install_not_armed,
  target/first_cap_logged_once, research/forced_off_passes_through,
  research/trace_line_before_original, research/trace_negative_after_positive

Added by the guard polish (before Live 2):
  target/failed_install_retried_on_next_arm, target/calls_counted_in_player_build,
  research/trace_reply_names_end_damage_route
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SDK_INCLUDE = ROOT.parent / "hs-game-sdk/cpp/include"

LARGEST = "99999000000000"
SMALLEST = "-999999"
NOT_CAPPED = "numguard: num_compressSteam hook is {route} - numbers past the display limit are not capped"
TABLE_ONLY = ("numguard: num_compressSteam hook is table-only - only calls through the script table are capped;"
              " the game's calls through its method value are not (static reading)")
FORCED_OFF = "numguard: forced off - every number reaches the shortener as given, still counted and traced"
FORCED_AUTO = "numguard: forced auto - a number past the display limit is capped while armed"


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


def number_guard(header):
    return "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))


def stats_manager(header):
    """The real class, minus the includes the harness stands in for or splices first."""
    text = "\n".join(
        line for line in header.split("\n")
        if not line.strip().startswith("#pragma once")
        and '#include "Common.hpp"' not in line
        and "#include <hs_game_sdk/reward_scope.hpp>" not in line
        and '#include "CombatTextHook.hpp"' not in line
        and '#include "NumberGuard.hpp"' not in line
    )
    private_ctor = "private:\n    StatsManager() = default;"
    if text.count(private_ctor) != 1:
        raise AssertionError("StatsManager's private default constructor was not found once")
    return text.replace(private_ctor, "public:\n    StatsManager() = default;\nprivate:")


def build_and_run(name, code, release):
    """Compile `code` (with FORGEPACT_RELEASE when `release`) and return its stdout."""
    out = ROOT / "build/number-guard-behavior" / name
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / "numguard.cpp"
    cpp.write_text(code, encoding="utf-8")
    binary = out / ("numguard.exe" if os.name == "nt" else "numguard")
    define = "/DFORGEPACT_RELEASE " if release else ""
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
            f'cl /nologo /std:c++20 /EHsc /O2 {define}/I "{SDK_INCLUDE}" "{cpp}" '
            f'/Fe:"{binary}" /Fo:"{out / "numguard.obj"}"\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd", "/d", "/c", str(batch)]
    else:
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
        command = [compiler, "-std=c++20", "-O2", f"-I{SDK_INCLUDE}", str(cpp), "-o", str(binary)]
        if release:
            command.insert(3, "-DFORGEPACT_RELEASE")
    # The compiler speaks the machine's locale; decode leniently.
    result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    run = subprocess.run([str(binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    return run.stdout


class Output:
    def __init__(self, text):
        self.text = text

    def scenario(self, label):
        for line in self.text.split("\n"):
            if line.startswith(f"SCENARIO {label} "):
                return line[len(f"SCENARIO {label} "):]
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.text}")

    def fields(self, label):
        return dict(piece.split("=", 1) for piece in self.scenario(label).split(" ") if "=" in piece)

    def logs(self, label):
        prefix = f"LOG {label} :: "
        return [line[len(prefix):] for line in self.text.split("\n") if line.startswith(prefix)]

    def labels(self):
        return [line.split(" ")[1] for line in self.text.split("\n") if line.startswith("SCENARIO ")]


class NumberGuardBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        guard = (ROOT / "plugin/include/ForgePact/NumberGuard.hpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        stats = (ROOT / "plugin/include/ForgePact/StatsManager.hpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        helpers = "\n\n".join(implementation(plugin, signature) for signature in (
            "static std::string Lower(",
            "static std::string FirstToken(",
        ))
        code = (ROOT / "tests/number_guard_harness.cpp").read_text(encoding="utf-8")
        code = (code.replace("// PRODUCTION_HELPERS", helpers)
                    .replace("// PRODUCTION_NUMBERGUARD", number_guard(guard))
                    .replace("// PRODUCTION_STATSMANAGER", stats_manager(stats))
                    .replace("// PRODUCTION_NUMGUARDCMD", implementation(plugin, "static void NumGuardCmd(")))
        cls.player = Output(build_and_run("player", code, release=True))
        cls.research = Output(build_and_run("research", code, release=False))
        cls.builds = (("player", cls.player), ("research", cls.research))

    def test_harness_ran_in_both_builds(self):
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertIn("HARNESS DONE", out.text, out.text)
                self.assertNotIn("threw=", out.text, out.text)

    def test_research_scenarios_exist_only_in_the_research_build(self):
        self.assertFalse([label for label in self.player.labels() if label.startswith("research/")])
        self.assertIn("research/forced_off_passes_through", self.research.labels())
        self.assertIn("research/trace_line_before_original", self.research.labels())

    # ---- baseline: vanilla stays vanilla ----------------------------------

    def test_baseline_not_armed_passes_through(self):
        # baseline/not_armed_passes_through: every lever at vanilla hooks
        # nothing, and the shortener gets the number as given - including one
        # past the limit, which it fails on (the harness's positive control).
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("baseline/not_armed_passes_through"), {
                    "shown": "50000B|FAILFAST", "handed": "50000000000000|200000000000000",
                    "failfast": "1", "installs": "0", "written": "no",
                })

    def test_baseline_in_range_unchanged_when_armed(self):
        # baseline/in_range_unchanged_when_armed: armed, every number the
        # shortener can show reaches it unchanged and shows what it would
        # with no mod; a text goes through as given.
        expected = "0|1234|12,3K|999,9K|123,4M|99999B|99999,9B|-5|-999999|42|50000B|abc"
        for name, out in self.builds:
            with self.subTest(build=name):
                fields = out.fields("baseline/in_range_unchanged_when_armed")
                self.assertEqual(fields["shown"], fields["vanilla"])
                self.assertEqual(fields["shown"], expected)
                self.assertEqual(fields["handed"],
                                 "0|1234.7|12345|999999|123456789|99999000000000|99999999999999|-5|-999999|42|50000000000000")
                self.assertEqual(fields["failfast"], "0")
                self.assertEqual(fields["written"], "no")
                self.assertEqual([l for l in out.logs("baseline/in_range_unchanged_when_armed")
                                  if l.startswith("numguard: capped ")], [])

    # ---- target: past the limit, the largest the shortener can show --------

    def test_target_positive_overflow_capped(self):
        # 1e14 exactly, just past it, past the 32-bit range, huge, +inf, and an INT64.
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/positive_overflow_capped"), {
                    "shown": "|".join(["99999B"] * 6), "handed": "|".join([LARGEST] * 6), "failfast": "0",
                })

    def test_target_negative_overflow_capped(self):
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/negative_overflow_capped"), {
                    "shown": "|".join([SMALLEST] * 5), "handed": "|".join([SMALLEST] * 5), "failfast": "0",
                })

    def test_target_nan_capped_to_zero(self):
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/nan_capped_to_zero"),
                                 {"shown": "0", "handed": "0", "failfast": "0"})

    def test_target_caller_argument_never_written(self):
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/caller_argument_never_written"), {
                    "written": "no", "second": "7", "shown": SMALLEST,
                    "handed": f"{LARGEST}|{LARGEST}|0|{SMALLEST}",
                })

    def test_target_arms_from_levers_once(self):
        # Vanilla values arm nothing; each lever off vanilla arms; the detour
        # goes in once; a lever its own hook refused arms nothing.
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/arms_from_levers_once"), {
                    "vanilla": "0", "damage": "1", "all": "1", "shown": "99999B", "armed": "yes",
                    "critchance": "1", "statadd": "1", "refusedstatadd": "0", "failedstat": "0",
                })

    def test_target_table_only_install_not_armed(self):
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/table_only_install_not_armed"),
                                 {"shown": "FAILFAST", "failfast": "1", "installs": "1"})
                logs = out.logs("target/table_only_install_not_armed")
                self.assertEqual(logs.count(TABLE_ONLY), 1, logs)
                self.assertEqual([l for l in logs if l.startswith("numguard: num_compressSteam hook is ")],
                                 [TABLE_ONLY], logs)
                self.assertTrue(logs[-1].startswith("numguard: not armed route=table-only clamped=0 first=- calls=0"),
                                logs)
                self.assertEqual([l for l in logs if l.startswith("numguard: capped ")], [])

    def test_target_failed_install_not_armed(self):
        for name, out in self.builds:
            with self.subTest(build=name):
                # Two armings, two attempts (a failed install is retried),
                # one "not capped" line (once per route result).
                self.assertEqual(out.fields("target/failed_install_not_armed"), {"installs": "2"})
                logs = out.logs("target/failed_install_not_armed")
                self.assertEqual(logs.count(NOT_CAPPED.format(route="failed")), 1, logs)
                self.assertTrue(logs[-1].startswith("numguard: not armed route=failed clamped=0 first=- calls=0"), logs)

    def test_target_failed_install_retried_on_next_arm(self):
        # target/failed_install_retried_on_next_arm: a route that came up
        # failed is tried again at the next arming (the first guard kept a
        # failed first resolve for the whole session and never armed); the
        # "not capped" line is written once per route result, not per
        # attempt; a native route is kept and never reinstalled.
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/failed_install_retried_on_next_arm"), {
                    "failed": "2", "retried": "3", "after": "3", "shown": "99999B", "failfast": "0",
                    "armed": "yes",
                })
                logs = out.logs("target/failed_install_retried_on_next_arm")
                self.assertEqual(logs.count(NOT_CAPPED.format(route="failed")), 1, logs)
                self.assertEqual([l for l in logs if l.startswith("numguard: num_compressSteam hook is ")],
                                 [NOT_CAPPED.format(route="failed")], logs)
                self.assertTrue(logs[-1].startswith(
                    "numguard: armed route=native clamped=1 first=200000000000000 calls=1"), logs)

    def test_target_calls_counted_in_player_build(self):
        # target/calls_counted_in_player_build: the player build counts the
        # calls the detour saw too, so `calls=0` (it never ran) and `calls=2
        # clamped=0` (nothing needed capping) read differently. Exactly one
        # `calls=` field, after `first=`, in each build.
        self.assertEqual(self.player.fields("target/calls_counted_in_player_build"), {"failfast": "0"})
        self.assertEqual(self.player.logs("target/calls_counted_in_player_build")[-1],
                         "numguard: armed route=native clamped=1 first=200000000000000 calls=2")
        self.assertEqual(self.research.logs("target/calls_counted_in_player_build")[-1],
                         "numguard: armed route=native clamped=1 first=200000000000000 calls=2"
                         " forced=auto trace=off largest=200000000000000")
        for name, out in self.builds:
            with self.subTest(build=name):
                status = out.logs("target/calls_counted_in_player_build")[-1]
                self.assertEqual(status.count(" calls="), 1, status)
                self.assertLess(status.index(" first="), status.index(" calls="), status)

    def test_target_first_cap_logged_once(self):
        for name, out in self.builds:
            with self.subTest(build=name):
                self.assertEqual(out.fields("target/first_cap_logged_once"), {"failfast": "0"})
                logs = out.logs("target/first_cap_logged_once")
                self.assertEqual([l for l in logs if l.startswith("numguard: capped ")],
                                 [f'numguard: capped 200000000000000 -> {LARGEST} (shown "99999B")'])
        self.assertEqual(self.player.logs("target/first_cap_logged_once")[-1],
                         "numguard: armed route=native clamped=3 first=200000000000000 calls=4")
        self.assertEqual(self.research.logs("target/first_cap_logged_once")[-1],
                         "numguard: armed route=native clamped=3 first=200000000000000 calls=4"
                         " forced=auto trace=off largest=3e+15")

    # ---- the command -------------------------------------------------------

    def test_player_build_answers_status_and_names_the_research_verbs(self):
        status = "numguard: not armed route=none clamped=0 first=- calls=0"
        self.assertEqual(self.player.logs("target/command_surface"), [
            status, status,
            "numguard: off is available in the research build only",
            "numguard: trace is available in the research build only",
            "numguard: auto is available in the research build only",
        ])
        self.assertEqual(self.player.fields("target/command_surface"), {"installs": "0", "enddamage": "0"})

    def test_research_build_status_and_verbs(self):
        status = "numguard: not armed route=none clamped=0 first=- calls=0 forced=auto trace=off largest=0"
        logs = self.research.logs("target/command_surface")
        self.assertEqual(logs[:3], [status, status, FORCED_OFF])
        self.assertIn("numguard trace -> on (num_compressSteam hook native, CalculateEndDamage native)", logs)
        self.assertEqual(logs[-1], FORCED_AUTO)
        self.assertEqual(self.research.fields("target/command_surface"), {"installs": "1", "enddamage": "1"})

    # ---- research build only ------------------------------------------------

    def test_research_forced_off_passes_through(self):
        fields = self.research.fields("research/forced_off_passes_through")
        self.assertEqual(fields, {"off": "FAILFAST", "auto": "99999B", "failfast": "1",
                                  "handed": f"200000000000000|{LARGEST}"})
        logs = self.research.logs("research/forced_off_passes_through")
        self.assertIn(FORCED_OFF, logs)
        statuses = [l for l in logs if l.startswith("numguard: armed ")]
        self.assertEqual(statuses, [
            "numguard: armed route=native clamped=0 first=- calls=1 forced=off trace=off largest=200000000000000",
            "numguard: armed route=native clamped=1 first=200000000000000 calls=2 forced=auto trace=off"
            " largest=200000000000000",
        ])

    def test_research_trace_line_before_original(self):
        fields = self.research.fields("research/trace_line_before_original")
        self.assertEqual(fields, {"installs": "1", "enddamage": "1", "native": "1234", "scaled": "13574",
                                  "failfast": "0"})
        logs = self.research.logs("research/trace_line_before_original")
        # The value's line is the last thing written before the shortener runs.
        game = logs.index(f"GAME num_compressSteam {LARGEST}")
        self.assertEqual(logs[game - 1], "numguard trace: num_compressSteam 200000000000000")
        for value in ("5", "50", "5000"):
            at = logs.index(f"GAME num_compressSteam {value}")
            self.assertEqual(logs[at - 1], f"numguard trace: num_compressSteam {value}")
        # A line per decade reached, not per call.
        self.assertEqual([l for l in logs if l.startswith("numguard trace: num_compressSteam ")], [
            "numguard trace: num_compressSteam 5",
            "numguard trace: num_compressSteam 50",
            "numguard trace: num_compressSteam 5000",
            "numguard trace: num_compressSteam 200000000000000",
        ])
        self.assertEqual([l for l in logs if l.startswith("numguard trace: CalculateEndDamage.")], [
            "numguard trace: CalculateEndDamage.native 1234",
            "numguard trace: CalculateEndDamage.returned 1234",
            "numguard trace: CalculateEndDamage.returned 13574",
        ])
        self.assertIn("numguard trace -> off (7 lines written)", logs)
        self.assertEqual(logs[-1], "numguard: armed route=native clamped=1 first=200000000000000"
                                   " calls=6 forced=auto trace=off largest=200000000000000")

    def test_research_trace_negative_after_positive(self):
        # research/trace_negative_after_positive: with the cap forced off, a
        # failing negative of the same magnitude as an earlier positive still
        # writes its trace line, and that line is the last one before the
        # shortener fails on it. A single magnitude mark for both signs (the
        # trace as first written) saw -2e9 as no new decade after 1e9 and wrote
        # nothing, so the last line named 1e9 instead of the fatal value.
        self.assertIn("research/trace_negative_after_positive", self.research.labels())
        fields = self.research.fields("research/trace_negative_after_positive")
        self.assertEqual(fields, {"shown": "FAILFAST", "handed": "1000000000|-2000000000", "failfast": "1"})
        logs = self.research.logs("research/trace_negative_after_positive")
        self.assertEqual([l for l in logs if l.startswith("numguard trace: num_compressSteam ")], [
            "numguard trace: num_compressSteam 1000000000",
            "numguard trace: num_compressSteam -2000000000",
        ])
        fatal = logs.index("GAME num_compressSteam -2000000000")
        self.assertEqual(logs[fatal - 1], "numguard trace: num_compressSteam -2000000000", logs)

    def test_research_trace_reply_names_end_damage_route(self):
        # research/trace_reply_names_end_damage_route: `numguard trace on`
        # names the route CalculateEndDamage's hook took (the first guard said
        # only "hooked", which a table-only hook, blind to the game's direct
        # calls, also satisfied) - whether the trace installed it or `stat
        # damage` did first.
        self.assertIn("research/trace_reply_names_end_damage_route", self.research.labels())
        self.assertEqual(self.research.fields("research/trace_reply_names_end_damage_route"), {
            "fresh": "table-only", "freshinstalls": "1", "prior": "table-only", "priorinstalls": "1",
            "failed": "failed", "native": "native",
        })
        logs = self.research.logs("research/trace_reply_names_end_damage_route")
        self.assertIn("numguard trace -> on (num_compressSteam hook native, CalculateEndDamage native)", logs)
        self.assertFalse([l for l in logs if "hooked" in l], logs)

    def test_research_trace_stops_at_400_lines(self):
        self.assertEqual(self.research.fields("research/trace_limit"), {"lines": "400", "limit": "1"})


if __name__ == "__main__":
    unittest.main()
