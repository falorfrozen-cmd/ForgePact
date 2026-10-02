"""Run the real `statadd` boosts in the game's place, against controlled game API responses (#114).

Companion to test_stat_add_contract.py, which covers the panel and the
commands it sends.

A Stat* script builds its result array fresh and the game reads element 0.
These scenarios call the stats the way the game does, through the real
ForgePact::StatsManager hooks, and report the value the game received beside
what was logged. The baseline scenarios pin vanilla (nothing armed, or a bonus
of 0) and the target scenarios pin what each boost must return.
"""
import os
import re
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


def stats_manager(header):
    """The real class, minus the includes the harness stands in for.

    Its constructor is private (a singleton); the harness builds a fresh
    manager per scenario, so the constructor alone is made public here.
    """
    text = "\n".join(
        line for line in header.split("\n")
        if not line.strip().startswith("#pragma once")
        and '#include "Common.hpp"' not in line
        and "#include <hs_game_sdk/reward_scope.hpp>" not in line
    )
    private_ctor = "private:\n    StatsManager() = default;"
    if text.count(private_ctor) != 1:
        raise AssertionError("StatsManager's private default constructor was not found once")
    return text.replace(private_ctor, "public:\n    StatsManager() = default;\nprivate:")


class StatAddBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        header = (ROOT / "plugin/include/ForgePact/StatsManager.hpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        helpers = "\n\n".join(implementation(plugin, signature) for signature in (
            "static std::string Lower(",
            "static std::string FirstToken(",
        ))

        out = ROOT / "build/stat-add-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/stat_add_harness.cpp").read_text(encoding="utf-8")
        code = (code.replace("// PRODUCTION_HELPERS", helpers)
                    .replace("// PRODUCTION_STATSMANAGER", stats_manager(header)))
        cpp = out / "statadd.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("statadd.exe" if os.name == "nt" else "statadd")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "statadd.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(cls.binary)]

        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)

        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
        cls.output = run.stdout
        (out / "run.log").write_text(run.stdout + run.stderr, encoding="utf-8")

    def scenario(self, label):
        for line in self.output.split("\n"):
            if line.startswith(f"SCENARIO {label} "):
                return line[len(f"SCENARIO {label} "):]
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def fields(self, label):
        return dict(piece.split("=", 1) for piece in self.scenario(label).split(" ") if "=" in piece)

    def logs(self, label):
        prefix = f"LOG {label} :: "
        return [line[len(prefix):] for line in self.output.split("\n") if line.startswith(prefix)]

    def first_call_lines(self, label):
        return [line for line in self.logs(label) if ": first boosted call " in line]

    def test_harness_ran(self):
        self.assertIn("HARNESS DONE", self.output, self.output)
        self.assertNotIn("threw=", self.output, self.output)

    # ---- baseline: vanilla stays vanilla ----------------------------------

    def test_baseline_nothing_armed_the_game_reads_its_own_values(self):
        self.assertEqual(self.fields("baseline_off"),
                         {"haste": "12", "all": "2", "fcr": "33", "installs": "0"})

    def test_baseline_a_bonus_of_zero_installs_no_hook(self):
        self.assertEqual(self.fields("zero_installs_nothing"), {"haste": "12", "all": "2", "installs": "0"})
        self.assertEqual(self.logs("zero_installs_nothing"), [
            "statadd StatSpellHaste -> +0 (native, no hook)",
            "statadd StatAllSkills -> +0 (native, no hook)",
        ])

    def test_turning_a_boost_off_makes_the_hook_a_pass_through(self):
        fields = self.fields("off_after_on")
        self.assertEqual(fields["on"], "12")
        self.assertEqual(fields["off"], "2")
        self.assertEqual(fields["builtinsWhileOff"], "0")

    # ---- target: each boost adds to element 0 -----------------------------

    def test_target_skill_haste_adds_points_on_every_call(self):
        fields = self.fields("skillhaste_target")
        self.assertEqual(fields["haste"], "112,112,112")
        self.assertEqual(fields["installs"], "1")

    def test_the_games_own_array_is_never_written(self):
        # The game may hand the same array back; adding in place would compound.
        self.assertEqual(self.fields("skillhaste_target")["built"], "[12,5,3][12,5,3][12,5,3]")
        self.assertEqual(self.fields("allskills_target")["built"], "[2,1]")

    def test_target_all_skills_adds_levels_and_keeps_the_rest_of_the_array(self):
        self.assertEqual(self.fields("allskills_target"),
                         {"all": "21", "second": "1", "length": "2", "built": "[2,1]"})

    def test_all_skills_is_whole_levels_capped_at_100(self):
        self.assertEqual(self.fields("allskills_whole_and_capped"),
                         {"a": "22", "b": "21", "c": "102", "d": "2", "e": "2"})

    def test_skill_haste_keeps_a_fraction_and_has_no_plugin_cap(self):
        self.assertEqual(self.fields("skillhaste_fraction_and_negative"),
                         {"a": "14.5", "b": "912", "c": "12"})

    def test_a_plain_number_result_is_added_to_as_well(self):
        self.assertEqual(self.fields("scalar_result"), {"haste": "112"})

    # ---- the first-call line a player build needs --------------------------

    def test_the_first_boosted_call_is_reported_once_per_arming(self):
        self.assertEqual(self.first_call_lines("first_call_once_per_arming"), [
            "statadd StatSpellHaste: first boosted call 12 -> 112",
            "statadd StatSpellHaste: first boosted call 30 -> 80",
        ])
        self.assertEqual(self.fields("first_call_once_per_arming"), {"off": "30"})
        self.assertEqual(self.first_call_lines("skillhaste_target"),
                         ["statadd StatSpellHaste: first boosted call 12 -> 112"])

    # ---- hook routes -------------------------------------------------------

    def test_a_table_only_hook_is_refused_not_armed(self):
        self.assertEqual(self.fields("table_only_refused"), {"all": "2", "installs": "1"})
        refusal = ("statadd: StatAllSkills hook is TABLE-ONLY - the game calls it directly, "
                   "so the bonus could never apply; not armed")
        self.assertEqual(self.logs("table_only_refused").count(refusal), 2, self.logs("table_only_refused"))
        self.assertEqual(self.first_call_lines("table_only_refused"), [])
        # Off needs no route: no third refusal, just the off line.
        self.assertEqual(self.logs("table_only_refused")[-1], "statadd StatAllSkills -> +0")

    def test_a_missing_script_is_refused(self):
        self.assertEqual(self.fields("not_found_refused"), {"haste": "12"})
        self.assertIn("statadd: StatSpellHaste hook is FAILED - the game calls it directly, "
                      "so the bonus could never apply; not armed", self.logs("not_found_refused"))

    # ---- names -------------------------------------------------------------

    def test_faster_cast_rate_keeps_every_old_spelling(self):
        self.assertEqual(self.fields("castrate_unchanged"), {"a": "73", "b": "83", "c": "93"})

    def test_aliases_reach_the_new_entries(self):
        self.assertEqual(self.fields("aliases"), {"a": "22", "b": "32", "c": "5"})

    def test_list_shows_every_entry_its_route_and_first_call(self):
        lines = [line for line in self.logs("list") if line.startswith("  ")]
        self.assertEqual(len(lines), 3, self.logs("list"))
        self.assertRegex(lines[0], r"^  castrate\s+\(StatFasterCastRate\s*\) \+0\s+hook=none\s+first=- calls=0$")
        self.assertRegex(lines[1], r"^  skillhaste\s+\(StatSpellHaste\s*\) \+100\s+hook=native\s+first=12->112 calls=1$")
        self.assertRegex(lines[2], r"^  allskills\s+\(StatAllSkills\s*\) \+19\s+hook=native\s+first=waiting calls=0$")

    def test_bad_input_changes_nothing(self):
        self.assertEqual(self.fields("bad_input"), {"installs": "0"})
        self.assertEqual(self.logs("bad_input"), [
            "statadd: unknown 'foo' (castrate, skillhaste, allskills, list)",
            "statadd: the bonus must be a number",
        ])


if __name__ == "__main__":
    unittest.main()
