"""Run the real toggle-skill indicator read against a controlled game API.

Companion to toggle_skill_harness.cpp and test_toggle_skill_contract.py (which
asserts on source text). This file proves the READ ITSELF - what
ToggleIndicatorRead() decides from a counted enumeration - end to end, before
any drawing code exists (P1; issue #11, Track B). The research doc
(docs/toggle-skills-research.md, "The read, and exactly what has been
proven") measured that the planned read shape ran exactly twice, on the wrong
objects, before any cast - never a non-zero on the AOE object itself - so the
indicator workorder's own positive control has to be this: the production
read, called from where the indicator will call it, deciding ON vs OFF vs
UNREADABLE against instances this harness controls directly.
"""
import os
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


def declaration(source, prefix):
    """One whole single-line declaration, so its value is never restated here."""
    for line in source.split("\n"):
        if line.strip().startswith(prefix):
            return line
    raise AssertionError(f"not found: {prefix}")


class ToggleSkillBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")
        header = (ROOT / "plugin/include/ForgePact/ToggleSkillMod.hpp").read_text(encoding="utf-8")

        # The real class/struct/enum, verbatim, minus the include of
        # Common.hpp (the harness supplies the stand-ins Common.hpp would
        # have pulled in) - same shape as test_relic_filter_behavior.py.
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        constants = declaration(cls.plugin, "static constexpr int kToggleIndicatorScanCap")
        production = "\n".join([
            implementation(cls.plugin, "static bool ToggleIndicatorResolveAoeObject("),
            implementation(cls.plugin, "static ForgePact::ToggleIndicatorState ToggleIndicatorRead("),
        ])

        out = ROOT / "build/toggle-skill-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/toggle_skill_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_CONSTANTS", constants)
        code = code.replace("// PRODUCTION_TOGGLESKILL", klass)
        code = code.replace("// PRODUCTION_FUNCTIONS", production)
        cpp = out / "toggleskill.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("toggleskill.exe" if os.name == "nt" else "toggleskill")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "toggleskill.obj"}"\n'
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

    def test_harness_ran(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_no_aoe_is_off_without_resolving_player(self):
        self.assertScenario("read/no_aoe_is_off_without_resolving_player")
        self.assertScenario("read/no_aoe_is_off_without_resolving_player/resolve_calls")

    def test_own_aoe_is_on(self):
        self.assertScenario("read/own_aoe_is_on")

    def test_valueref_player_is_on(self):
        # This runner hands back VALUE_REF for the local player, not
        # VALUE_OBJECT - Known Limitations item 7 is what happens when a kind
        # check decides whether the work happens at all.
        self.assertScenario("read/valueref_player_is_on")

    def test_foreign_aoe_is_off(self):
        self.assertScenario("read/foreign_aoe_is_off")

    def test_own_and_foreign_is_on(self):
        self.assertScenario("read/own_and_foreign_is_on")

    def test_two_own_is_on(self):
        self.assertScenario("read/two_own_is_on")

    def test_unattributed_only_is_unreadable(self):
        # A foreign AOE fails toward "absent"; an unattributed one (its own
        # playerNumber could not be read) must not be guessed either way.
        self.assertScenario("read/unattributed_only_is_unreadable")

    def test_no_local_player_is_unreadable(self):
        self.assertScenario("read/no_local_player_is_unreadable")

    def test_local_number_unreadable_is_unreadable(self):
        self.assertScenario("read/local_number_unreadable_is_unreadable")

    def test_object_unresolved_is_unreadable(self):
        # A different, stronger failure than "resolved but zero instances".
        self.assertScenario("read/object_unresolved_is_unreadable")

    def test_scan_is_capped(self):
        self.assertScenario("read/scan_is_capped")

    def test_reread_every_call(self):
        # No caching across calls - the same point-of-use rule as the guide's
        # Known Limitations item 13.
        self.assertScenario("read/reread_every_call")

    def test_override_number_excludes_own(self):
        # `spurn as <n>`: the non-mutating negative control.
        self.assertScenario("read/override_number_excludes_own")

    def test_instance_number_throw_is_unreadable(self):
        # A threw instance_number call is a failed read, not a measured zero
        # - the catch's `d.n = 0` fallback must not decide a real, cheap Off.
        self.assertScenario("read/instance_number_throw_is_unreadable")
        self.assertScenario("read/instance_number_throw_is_unreadable/countReadFailed")


if __name__ == "__main__":
    unittest.main()
