"""Run Pet Collects Relics' real target choice against its baseline and target.

Companion to pet_relic_collector_harness.cpp. ForgePact issue #124: while the
pet is out it fetches relics on screen and picks them up through the game's own
loot pickup, and never targets a relic the player already owns at 10/10 - the
game will not let that one be picked up, so a pet sent to it would be stuck on
it, #94's shape. The relic tick reuses the Pet Quest Collector's
PetQuestSelector (PetQuestCollectorMod.hpp); what is new lives in
plugin/include/ForgePact/PetRelicCollectorMod.hpp - FilterRelicCandidates, the
maxed-set cache and PetFetchArbiter - game-independent by contract, so both
headers are spliced in whole and the harness supplies only Out().

The baseline scenario pins the quest selector's nearest rule applied to relics
with no maxed filter, as the reference: it walks to the nearer 10/10 relic. The
target scenarios pin the maxed rule (the lower relic is taken; a screen of only
maxed relics picks none and opens no travel; a relic a collect took to 10 is
skipped once the maxed set is refreshed), the arbiter both ways, and a failed
relic held back through the selector.

The live confirmation is Live procedure 1 of workorder forgepact-124-pet-relics
(the research build's `petrelic census` and `relicfilter testmaxed`).
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QUEST_HEADER = ROOT / "plugin/include/ForgePact/PetQuestCollectorMod.hpp"
RELIC_HEADER = ROOT / "plugin/include/ForgePact/PetRelicCollectorMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines; the harness supplies
    the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class PetRelicCollectorBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/pet-relic-collector-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/pet_relic_collector_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_PETQUEST", spliceable(QUEST_HEADER.read_text(encoding="utf-8")))
        code = code.replace("// PRODUCTION_PETRELIC", spliceable(RELIC_HEADER.read_text(encoding="utf-8")))
        cpp = out / "petrelic.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("petrelic.exe" if os.name == "nt" else "petrelic")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "petrelic.obj"}"\n'
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

    # ---- baseline: the nearest rule with no maxed filter, as the reference --

    def test_baseline_nearest_rule_picks_the_maxed_relic(self):
        # The pet would walk to the nearer 10/10 relic, and back to it after
        # the hold on the refused pickup expired.
        self.assertScenario("baseline/nearest_rule_picks_the_maxed_relic")

    # ---- target: the maxed rule, the arbiter, the hold-back ----------------

    def test_target_maxed_relic_never_picked(self):
        # With a negative control: an empty maxed set picks the nearer relic.
        self.assertScenario("target/maxed_relic_never_picked")

    def test_target_only_maxed_picks_none(self):
        self.assertScenario("target/only_maxed_picks_none")

    def test_target_newly_maxed_relic_is_skipped_after_refresh(self):
        self.assertScenario("target/newly_maxed_relic_is_skipped_after_refresh")

    def test_target_quest_travel_blocks_a_relic_pick(self):
        self.assertScenario("target/quest_travel_blocks_a_relic_pick")

    def test_target_relic_travel_blocks_a_quest_pick(self):
        self.assertScenario("target/relic_travel_blocks_a_quest_pick")

    def test_target_failed_relic_is_held_back(self):
        # A refused pickup, one that left the relic, a gate refusal and a
        # timeout each hold it back; a collect that removed it holds nothing.
        self.assertScenario("target/failed_relic_is_held_back")

    def test_target_switch_line_names_the_mod(self):
        self.assertScenario("target/switch_line_names_the_mod")


if __name__ == "__main__":
    unittest.main()
