"""Run the real ForgePact::Mark1Movement core against controlled stat calls.

The Mark 1 set's bonus halves the walk. Mod live 1 picked the route
(docs/mark1-set-research.md, "Phase 2 decision"): `speed-route: stat-model`,
because the walk's step is affine in the stat dispatcher's id-64 answer
(proportional to the answer plus about 200), so the lever answers the local
player's id-64 ReturnSpecificStat calls (x - 200) / 2 while the bonus is
active; and `penalty-route: none`, so no cast-penalty lever ships and the stat
line reads `penalty=unavailable`.

Baseline: the bonus inactive, another self, another stat id, no readable id:
the answer reaches the caller as the game gave it and nothing is counted.
Target: the half step across the measured sweep, Sorak's 151 answered -24.5,
each call answered once from its own original, a non-number left alone and
counted. The adapter in ModuleMain.cpp (the shared ReturnSpecificStat detour,
the local player, the bonus at the point of use) is pinned by
test_mark1_set_contract.py; the constants against the hub's model by
tests/test_movement_speed_model.py's LeverParity.
"""
import os
import re
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/Mark1Movement.hpp"
RESEARCH_DOC = ROOT / "docs/mark1-set-research.md"


class Mark1MovementBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = HEADER.read_text(encoding="utf-8")
        cls.header = header
        klass = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/mark1-movement-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/mark1_movement_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_MARK1MOVEMENT", klass)
        cpp = out / "mark1movement.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("mark1movement.exe" if os.name == "nt" else "mark1movement")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "mark1movement.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-I", str(ROOT / "plugin/include"), str(cpp), "-o", str(cls.binary)]
        # The compiler speaks the machine's locale; decode leniently so a
        # localized diagnostic cannot itself crash the test.
        result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
        (out / "compile.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        if result.returncode:
            raise AssertionError(result.stdout + result.stderr)
        run = subprocess.run([str(cls.binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
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

    # ---- baseline: vanilla stays vanilla ------------------------------------
    def test_baseline_inactive_bonus_leaves_every_answer_alone(self):
        self.assertScenario("baseline/inactive_answer_untouched")
        self.assertScenario("baseline/inactive_nothing_counted")

    def test_baseline_another_self_is_never_the_levers(self):
        self.assertScenario("baseline/other_self_untouched")

    def test_baseline_every_other_stat_id_is_never_the_levers(self):
        self.assertScenario("baseline/other_stat_ids_untouched")
        self.assertScenario("baseline/no_stat_id_untouched")
        self.assertScenario("baseline/scope_control")

    # ---- target: the half step ------------------------------------------------
    def test_target_sorak_walks_at_half_speed(self):
        self.assertScenario("target/sorak_answered_minus_24_5")
        self.assertScenario("target/sorak_step_halved")

    def test_target_half_step_across_the_measured_sweep(self):
        self.assertScenario("target/half_step_across_the_sweep")
        self.assertScenario("target/each_call_counted")
        self.assertScenario("target/not_the_proportional_half")
        self.assertScenario("target/no_cap")

    def test_target_each_call_answered_once_from_its_own_original(self):
        self.assertScenario("target/repeated_calls_never_compound")
        self.assertScenario("target/off_again_is_vanilla")

    def test_target_a_non_number_is_left_alone_and_counted(self):
        self.assertScenario("target/not_a_number_untouched_and_counted")

    # ---- the routes Live 1 picked -------------------------------------------
    def test_the_routes_are_the_phase_2_decisions(self):
        self.assertScenario("route/speed_is_stat_model")
        self.assertScenario("route/penalty_is_none_and_unavailable")
        self.assertScenario("route/constants")
        doc = RESEARCH_DOC.read_text(encoding="utf-8")
        speed = re.search(r"^speed-route: (\S+)\s*$", doc, re.M)
        penalty = re.search(r"^penalty-route: (\S+)\s*$", doc, re.M)
        self.assertIsNotNone(speed)
        self.assertIsNotNone(penalty)
        self.assertIn(f'inline constexpr std::string_view kSpeedRoute = "{speed.group(1)}";', self.header)
        self.assertIn(f'inline constexpr std::string_view kPenaltyRoute = "{penalty.group(1)}";', self.header)

    def test_the_core_is_game_independent(self):
        # Numbers in, numbers out: no instance, no RValue, no game call.
        for token in ("RValue", "CInstance", "g_Yytk", "HookOneScript", "#include \"Common.hpp\"", "windows.h"):
            self.assertNotIn(token, self.header, token)


if __name__ == "__main__":
    unittest.main()
