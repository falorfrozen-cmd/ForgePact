"""Run the real auto-prospect core against its baseline and target.

Companion to auto_prospect_harness.cpp. ForgePact issue #9's Stage B is
auto-prospect on insert (the human's decision, 2026-09-18): each item moved
into the Prospect Cube's grid is prospected at once by the game's own Prospect
operation. What the mod decides - when an insert counts, when it invokes, when
it refuses and what it says - lives in
plugin/include/ForgePact/AutoProspectMod.hpp, a header that is game-independent
by contract, so it is spliced in whole with no runtime stub.

Baseline scenarios pin that the mod is off by default and, off, never invokes
whatever it is fed. Target scenarios pin one invoke per landed insert into the
ProspectGrid (whether the cells update in the same frame or later), nothing for
an insert elsewhere, a rearrangement, an insert the game makes inside our own
invoke or a pending insert on a replaced node, coalescing, the grid-full, no
window and no button refusals, the settled count following a new node and a
removal, `ran-no-effect` one frame later, each refusal reported once, and a
stat line that names what the mod did (Known Limitations item 7).
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/AutoProspectMod.hpp"


def spliceable(header_text):
    """The header without its #pragma/#include lines, which name the plugin's
    runtime headers; the harness supplies the standard library itself."""
    return "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith(("#pragma", "#include"))
    )


class AutoProspectBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        out = ROOT / "build/auto-prospect-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/auto_prospect_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_AUTOPROSPECT", spliceable(HEADER.read_text(encoding="utf-8")))
        cpp = out / "autoprospect.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("autoprospect.exe" if os.name == "nt" else "autoprospect")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "autoprospect.obj"}"\n'
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
            self.assertTrue(label.startswith(("baseline/", "target/", "adapter/")), label)

    # ---- baseline: off is vanilla ------------------------------------------

    def test_baseline_off_by_default(self):
        self.assertScenario("baseline/off_by_default")

    def test_baseline_off_never_invokes(self):
        self.assertScenario("baseline/off_never_invokes")

    # ---- target: on --------------------------------------------------------

    def test_target_insert_into_prospect_grid_invokes_once(self):
        self.assertScenario("target/insert_into_prospect_grid_invokes_once")
        self.assertScenario("target/late_landing_invokes_when_it_lands")

    def test_target_insert_elsewhere_is_ignored(self):
        self.assertScenario("target/insert_elsewhere_ignored")

    def test_target_rearrangement_never_invokes_and_expires_not_landed(self):
        self.assertScenario("target/rearrangement_never_invokes_and_expires_not_landed")

    def test_target_insert_while_invoking_is_ignored_and_counted(self):
        self.assertScenario("target/insert_while_invoking_ignored_and_counted")

    def test_target_inserts_in_one_frame_coalesce(self):
        self.assertScenario("target/inserts_in_one_frame_coalesce")

    def test_target_grid_full_refuses_and_counts(self):
        self.assertScenario("target/grid_full_refuses_and_counts")

    def test_target_no_window_or_button_refuses_and_drops_the_insert(self):
        self.assertScenario("target/no_window_refuses_and_drops_pending")
        self.assertScenario("target/no_button_refuses_and_drops_pending")
        self.assertScenario("target/pending_on_a_replaced_node_is_dropped")

    def test_target_new_node_resets_the_settled_count(self):
        self.assertScenario("target/new_node_resets_settled_count")
        self.assertScenario("target/removal_lowers_settled_count")

    def test_target_ran_no_effect_is_counted_one_frame_later(self):
        self.assertScenario("target/ran_no_effect_counted_one_frame_later")

    def test_target_first_refusal_is_reported_once_per_reason(self):
        self.assertScenario("target/first_refusal_reported_once_per_reason")

    def test_target_stat_line_names_what_the_mod_did(self):
        self.assertScenario("target/statline_names_what_it_did")


if __name__ == "__main__":
    unittest.main()
