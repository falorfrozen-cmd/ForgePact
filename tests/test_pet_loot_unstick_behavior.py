"""Run the real stuck-target decision of `petunstick` against its baseline and target.

Companion to pet_loot_unstick_harness.cpp. ForgePact issue #94: with lots of
loot on the ground the companion stays on one item it cannot pick up. In the
game (a static reading, docs/pet-loot-stuck-research.md) the pet's
`lootTarget` is replaced only when that instance ceases to exist, so an item
whose pickup keeps failing pins the pet. The mod's decision lives in
plugin/include/ForgePact/PetLootUnstickMod.hpp as PetLootStuckWatch, which is
game-independent by contract and is spliced in whole; the harness supplies
only Out().

Baseline scenarios pin the game's rule as the reference (a surviving target is
kept for 600 frames while the scan offers another) and that the real mod,
off by default, asks for nothing. Target scenarios pin the watch: the same
target within the radius is given up at exactly kPetLootStuckFrames and not a
frame earlier; a travel beyond the radius never counts; a different target, no
target or a skipped frame restarts; a target is given up once until another
(or none) has been seen; a target that vanishes first asks for nothing; and
the singleton's toggle lines and stat line.

No automated session measures #94; Live procedure 1 of workorder
forgepact-pet-loot-stuck is the live confirmation.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/PetLootUnstickMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines; the harness supplies
    the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class PetLootUnstickBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/pet-loot-unstick-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/pet_loot_unstick_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_PETUNSTICK", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "petunstick.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("petunstick.exe" if os.name == "nt" else "petunstick")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "petunstick.obj"}"\n'
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

    # ---- baseline: the game's rule, and the mod while off -------------------

    def test_baseline_game_keeps_a_surviving_target(self):
        # The reference: a target that survives is kept for 600 frames while
        # the scan offers another; the negative control takes the other once
        # the target ceases to exist.
        self.assertScenario("baseline/game_keeps_a_surviving_target")

    def test_baseline_mod_off_never_asks(self):
        self.assertScenario("baseline/mod_off_never_asks")

    # ---- target: the stuck-target watch -------------------------------------

    def test_target_constants(self):
        self.assertScenario("target/constants")

    def test_target_stuck_target_is_given_up_at_exactly_the_count(self):
        self.assertScenario("target/stuck_target_is_given_up_at_exactly_the_count")

    def test_target_travelling_target_never_counts(self):
        # Negative control beside the positive one: beyond the radius (and an
        # unreadable distance) nothing counts; the count then starts on arrival.
        self.assertScenario("target/travelling_target_never_counts")

    def test_target_different_or_no_target_restarts(self):
        self.assertScenario("target/different_target_restarts")
        self.assertScenario("target/no_target_restarts")
        self.assertScenario("target/skipped_frame_restarts")

    def test_target_given_up_once_per_target(self):
        self.assertScenario("target/given_up_once_per_target")

    def test_target_vanished_target_asks_nothing(self):
        self.assertScenario("target/vanished_target_asks_nothing")

    def test_target_mod_on_asks_and_counts(self):
        self.assertScenario("target/mod_on_asks_and_counts")


if __name__ == "__main__":
    unittest.main()
