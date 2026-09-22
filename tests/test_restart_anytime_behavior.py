"""Run the real RestartAnytimeMod and HookRestartAnytimeSetFocus against a stand-in runner.

Companion to test_restart_anytime_contract.py, which pins source text. This
runs the shipped hook end to end (ForgePact issue #8, `restartanytime`): off
is the vanilla path and reads nothing; on, only the Restart button - told
apart by its own `uiNodeCallstack`, never by position or `self` - has its
`manualDisable` written open, in the kind it was read in, before the game's
own `UiSetFocus` body runs, and the original is called exactly once every
time. The stand-in answers the kinds this runner was measured to hand over
(VALUE_REF for the button, VALUE_BOOL for its members, VALUE_STRING for its
key), so a hook that only accepted another kind would fail here rather than
live. Skips without a C++ toolchain, like the other behavior suites.
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


def strip_release_blocks(code):
    """Compile the hook as the player build sees it: drop `#ifndef FORGEPACT_RELEASE` blocks."""
    out, depth = [], 0
    for line in code.split("\n"):
        stripped = line.strip()
        if depth == 0 and stripped.startswith("#ifndef FORGEPACT_RELEASE"):
            depth = 1
            continue
        if depth:
            if stripped.startswith("#if"):
                depth += 1
            elif stripped.startswith("#endif"):
                depth -= 1
            continue
        out.append(line)
    return "\n".join(out)


class RestartAnytimeBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8").replace("\r\n", "\n")
        header = (ROOT / "plugin/include/ForgePact/RestartAnytimeMod.hpp").read_text(encoding="utf-8").replace("\r\n", "\n")

        # The real header, verbatim, minus `#pragma once` and its include of
        # Common.hpp (the harness supplies the stand-ins Common.hpp pulls in).
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        production = "\n\n".join([
            implementation(plugin, "static bool HhUsableInstance("),
            implementation(plugin, "static bool RestartAnytimeReadGate("),
            implementation(plugin, "static RValue& HookRestartAnytimeSetFocus("),
        ])
        production = strip_release_blocks(production)

        out = ROOT / "build/restart-anytime-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/restart_anytime_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_HEADER", klass).replace("// PRODUCTION_FUNCTIONS", production)
        cpp = out / "restartanytime.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("restartanytime.exe" if os.name == "nt" else "restartanytime")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "restartanytime.obj"}"\n'
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

    def scenario(self, label):
        for line in self.output.split("\n"):
            if line.startswith(f"SCENARIO {label} "):
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def fields(self, label):
        return dict(piece.split("=", 1) for piece in self.scenario(label).split(" ")[2:] if "=" in piece)

    def count(self, label, key):
        return int(self.fields(label)[key])

    def logs(self, label):
        prefix = f"LOG {label} :: "
        return [line[len(prefix):] for line in self.output.split("\n") if line.startswith(prefix)]

    def test_harness_ran(self):
        self.assertIn("HARNESS DONE", self.output, self.output)

    def test_every_planned_scenario_is_in_the_harness(self):
        source = (ROOT / "tests/restart_anytime_harness.cpp").read_text(encoding="utf-8")
        for label in ("baseline/off_calls_original_and_writes_nothing",
                      "baseline/off_by_default",
                      "target/on_writes_only_when_arg0_is_the_restart_button",
                      "target/on_leaves_every_other_node_untouched",
                      "target/on_unreadable_member_passes_and_counts",
                      "target/on_calls_the_original_exactly_once",
                      "target/on_preserves_the_kind_read_at_entry"):
            self.assertIn(f'"{label}"', source, label)
        # Each target scenario carries the line it failed with against a
        # pass-through body: the red half of red-then-green.
        labels = re.findall(r'report\("(target/[^"]+)"', source)
        self.assertGreaterEqual(len(labels), 5)
        for label in labels:
            at = source.index(f'report("{label}"')
            previous = source.rfind("report(", 0, at)
            self.assertIn("red (pass-through body)", source[previous:at], label)

    # ---- baseline: off is the vanilla path ------------------------------------

    def test_off_by_default(self):
        label = "baseline/off_by_default"
        f = self.fields(label)
        self.assertEqual(f["enabled"], "0", self.output)
        self.assertEqual(self.count(label, "origcalls"), 1, self.output)
        self.assertEqual(self.count(label, "builtins"), 0, self.output)
        self.assertEqual(self.count(label, "writes"), 0, self.output)
        self.assertEqual(f["restartValue"], "bool:true", self.output)

    def test_off_calls_original_and_writes_nothing(self):
        label = "baseline/off_calls_original_and_writes_nothing"
        self.assertEqual(self.count(label, "origcalls"), 1, self.output)
        self.assertEqual(self.count(label, "builtins"), 0, self.output)   # nothing read either
        self.assertEqual(self.count(label, "writes"), 0, self.output)
        for key in ("written", "passed", "otherNode", "unreadable"):
            self.assertEqual(self.count(label, key), 0, key)
        self.assertEqual(self.fields(label)["restartValue"], "bool:true", self.output)

    # ---- target: on -------------------------------------------------------------

    def test_on_writes_only_when_arg0_is_the_restart_button(self):
        label = "target/on_writes_only_when_arg0_is_the_restart_button"
        self.assertEqual(self.count(label, "writes"), 1, "writes")
        self.assertEqual(self.count(label, "written"), 1, "written")
        f = self.fields(label)
        self.assertEqual(f["restartValue"], "bool:false", self.output)
        # Only manualDisable: `enabled` alone did not unlock the press (T3a),
        # so it is never touched.
        self.assertEqual(f["enabledValue"], "bool:false", self.output)
        self.assertEqual(self.count(label, "origcalls"), 1, self.output)

    def test_on_leaves_every_other_node_untouched(self):
        label = "target/on_leaves_every_other_node_untouched"
        self.assertEqual(self.count(label, "otherNode"), 6, "otherNode")
        self.assertEqual(self.count(label, "writes"), 0, "writes")
        self.assertEqual(self.count(label, "written"), 0, "written")
        self.assertEqual(self.count(label, "origcalls"), 6, "origcalls")
        f = self.fields(label)
        for key in ("resumeValue", "keylessValue", "restartValue"):
            self.assertEqual(f[key], "bool:true", key)

    def test_on_unreadable_member_passes_and_counts(self):
        label = "target/on_unreadable_member_passes_and_counts"
        self.assertEqual(self.count(label, "unreadable"), 3, "unreadable")
        self.assertEqual(self.count(label, "writes"), 0, "writes")
        self.assertEqual(self.count(label, "written"), 0, "written")
        self.assertEqual(self.count(label, "origcalls"), 3, "origcalls")
        f = self.fields(label)
        self.assertEqual(f["absentValue"], "undefined", self.output)
        self.assertEqual(f["textValue"], "string:true", self.output)

    def test_on_calls_the_original_exactly_once(self):
        label = "target/on_calls_the_original_exactly_once"
        self.assertEqual(self.count(label, "origcalls"), 1, "origcalls")
        # The write lands before the game's own body runs.
        self.assertEqual(self.fields(label)["seenAtOriginal"], "bool:false", "seenAtOriginal")
        self.assertEqual(self.fields(label)["sameResult"], "1", self.output)

    def test_on_preserves_the_kind_read_at_entry(self):
        label = "target/on_preserves_the_kind_read_at_entry"
        f = self.fields(label)
        self.assertEqual(f["boolWritten"], "bool:false", self.scenario(label))
        self.assertEqual(f["realWritten"], "real:0", self.scenario(label))
        self.assertEqual(self.count(label, "written"), 2, "written")

    def test_on_gate_already_open_writes_nothing(self):
        label = "target/on_gate_already_open_writes_nothing"
        self.assertEqual(self.count(label, "passed"), 1, "passed")
        self.assertEqual(self.count(label, "writes"), 0, "writes")
        self.assertEqual(self.count(label, "origcalls"), 1, "origcalls")

    def test_blind_install_stays_off(self):
        label = "target/blind_install_stays_off"
        f = self.fields(label)
        self.assertEqual(f["enabled"], "0", self.output)
        self.assertEqual(f["pending"], "0", self.output)
        self.assertEqual(self.count(label, "builtins"), 0, self.output)
        self.assertEqual(f["restartValue"], "bool:true", self.output)

    def test_first_write_is_said_once(self):
        lines = [line for line in self.output.split("\n") if "restartanytime: first write - " in line]
        self.assertEqual(len(lines), 1, self.output)
        self.assertIn("target/on_writes_only_when_arg0_is_the_restart_button", lines[0])


if __name__ == "__main__":
    unittest.main()
