"""Run the real prospect window sizing core against its baseline and target.

Companion to prospect_window_harness.cpp. ForgePact issue #9 ("the prospect
window is way too small for the amount of items players can hold") is in its
research stage: what sizes the window in the game is not yet measured
(docs/prospect-window-research.md, Phase 0 pending). What CAN be pinned before
the live session is the decision the mod will make once the mechanism is
known, in plugin/include/ForgePact/ProspectWindowMod.hpp - a header that is
game-independent by contract, so it is spliced in whole with no runtime stub.

Baseline scenarios pin that the mod off reproduces vanilla and applies
nothing. Target scenarios pin the scaling, the cap, never shrinking, once per
window instance, the refusal of nonsense sizes, and a stat line that names
what the mod did rather than what it is (Known Limitations item 7).
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/ProspectWindowMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines, which name the plugin's
    runtime headers; the harness supplies the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class ProspectWindowBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/prospect-window-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/prospect_window_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_PROSPECTWINDOW", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "prospectwindow.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("prospectwindow.exe" if os.name == "nt" else "prospectwindow")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "prospectwindow.obj"}"\n'
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

    # ---- baseline: off is vanilla ------------------------------------------

    def test_baseline_off_leaves_the_vanilla_size(self):
        self.assertScenario("baseline/off_decide_is_identity")

    def test_baseline_off_applies_to_nothing(self):
        self.assertScenario("baseline/off_never_applies")

    # ---- target: on --------------------------------------------------------

    def test_target_scales_by_the_declared_factor(self):
        self.assertScenario("target/on_scales_by_factor")

    def test_target_clamps_to_the_declared_cap(self):
        self.assertScenario("target/on_caps_at_max")

    def test_target_never_makes_a_grid_smaller(self):
        self.assertScenario("target/on_never_shrinks")

    def test_target_applies_once_per_window_instance(self):
        self.assertScenario("target/once_per_instance")
        self.assertScenario("target/latch_is_bounded")

    def test_target_refuses_and_counts_a_nonsense_vanilla_size(self):
        self.assertScenario("target/invalid_vanilla_refused_and_counted")

    def test_target_disabling_clears_the_latch_but_not_the_counters(self):
        self.assertScenario("target/disable_clears_latch")

    def test_target_stat_line_names_what_the_mod_did(self):
        self.assertScenario("target/statline_names_what_it_did")


if __name__ == "__main__":
    unittest.main()
