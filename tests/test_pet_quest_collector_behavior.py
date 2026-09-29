"""Run the Pet Quest Collector's real target selection against its baseline and target.

Companion to pet_quest_collector_harness.cpp. ForgePact issue #94: with many
quest items on screen the pet circled one of them instead of collecting them
one after another. The tick in ModuleMain.cpp chose its target with no memory
of failure - a collect that left the item in place, a gate refusal at arrival
or a travel timeout each made the same item the next target - and read at most
64 family instances per tick from index 0, so an item past that was never a
candidate. The decision now lives in plugin/include/ForgePact/PetQuestCollectorMod.hpp
as PetQuestSelector, which is game-independent by contract and is spliced in
whole; the harness supplies only Out().

Baseline scenarios pin the pre-fix rule as the reference (nearest, no memory,
re-picks a failed item at once; the walk never reaches past its budget) and
that the selector is still that nearest rule when nothing has failed. Target
scenarios pin the fix: a failed target is held back, the hold expires, every
candidate held back picks none, the family cursor reaches past the budget
within two ticks, a travelling target is never swapped, and the hold set stays
bounded.

No automated session measures #94; the owner's manual test on the final player
DLL is the live confirmation (workorder forgepact-dev2-bug-batch, Live
procedure 3).
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/PetQuestCollectorMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines; the harness supplies
    the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class PetQuestCollectorBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/pet-quest-collector-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/pet_quest_collector_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_PETQUEST", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "petquest.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("petquest.exe" if os.name == "nt" else "petquest")
        if os.name == "nt":
            vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
            if not vswhere.is_file():
                raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
            install = subprocess.check_output(
                [str(vswhere), "-latest", "-products", "*", "-requires",
                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                text=True).strip()
            if not install:
                raise unittest.SkipTest("Visual Studio C++ compiler is required for native behavior tests")
            vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
            batch = out / "compile.cmd"
            batch.write_text(
                f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "petquest.obj"}"\n'
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

    def test_every_scenario_is_a_baseline_or_a_target(self):
        labels = [line.split(" ")[1] for line in self.output.split("\n") if line.startswith(("PASS ", "FAIL "))]
        self.assertTrue(any(l.startswith("baseline/") for l in labels), labels)
        self.assertTrue(any(l.startswith("target/") for l in labels), labels)
        for label in labels:
            self.assertTrue(label.startswith(("baseline/", "target/")), label)

    # ---- baseline: the pre-fix rule, as the reference ----------------------

    def test_baseline_nearest_repicks_a_failed_target(self):
        # Also pins that the real selector, with nothing failed, is still the
        # nearest rule: a successful collect holds nothing back.
        self.assertScenario("baseline/nearest_repicks_a_failed_target")

    def test_baseline_walk_never_reaches_past_the_budget(self):
        self.assertScenario("baseline/walk_never_reaches_past_the_budget")

    # ---- target: memory of failure and a family cursor ---------------------

    def test_target_failed_target_is_held_back(self):
        # No effect, a gate refusal, a timeout and a refused call each hold
        # the item back; a collect that removed it, a lost target and an
        # abandoned travel (the negative controls) hold nothing.
        self.assertScenario("target/failed_target_is_held_back")

    def test_target_hold_expires(self):
        self.assertScenario("target/hold_expires")

    def test_target_all_held_back_picks_none(self):
        self.assertScenario("target/all_held_back_picks_none")

    def test_target_cursor_reaches_past_the_budget(self):
        self.assertScenario("target/cursor_reaches_past_the_budget")

    def test_target_travel_target_is_kept(self):
        self.assertScenario("target/travel_target_is_kept")

    def test_target_hold_set_is_bounded(self):
        self.assertScenario("target/hold_set_is_bounded")
        self.assertScenario("target/note_without_a_target_holds_nothing")


if __name__ == "__main__":
    unittest.main()
