"""Run the real RelicFilterMod and the real Hook_DropRelic against controlled game API responses.

Companion to test_relic_filter_contract.py, which asserts on source text.
Origin's review of PR #4 showed why that is not enough on its own: the
`relicfilter` log line reported the scan's INPUT count, taken before the guards
and repository writes that decide whether anything is held back, so it
announced a working filter on three paths where nothing was applied. Source
assertions could not see the difference; these scenarios compare what was
logged against what was actually written to the repository.
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


class RelicFilterBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        plugin = (ROOT / "plugin/ModuleMain.cpp").read_text(encoding="utf-8")
        header = (ROOT / "plugin/include/ForgePact/RelicFilterMod.hpp").read_text(encoding="utf-8")

        # The real class, verbatim, minus the include of Common.hpp (the
        # harness supplies the stand-ins Common.hpp would have pulled in).
        klass = "\n".join(
            line for line in header.split("\n")
            if not line.strip().startswith("#pragma once")
            and '#include "Common.hpp"' not in line
        )
        hook = implementation(plugin, "static RValue& Hook_DropRelic(")

        out = ROOT / "build/relic-filter-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/relic_filter_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_RELICFILTER", klass).replace("// PRODUCTION_FUNCTIONS", hook)
        cpp = out / "relicfilter.cpp"
        cpp.write_text(code, encoding="utf-8")

        cls.binary = out / ("relicfilter.exe" if os.name == "nt" else "relicfilter")
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
                f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "relicfilter.obj"}"\n'
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
                return line
        raise AssertionError(f"scenario {label!r} not in harness output:\n{self.output}")

    def logs(self, label):
        prefix = f"LOG {label} :: "
        return [line[len(prefix):] for line in self.output.split("\n") if line.startswith(prefix)]

    def counts(self, label):
        parts = dict(piece.split("=") for piece in self.scenario(label).split(" ")[2:] if "=" in piece)
        return {key: int(value) for key, value in parts.items()}

    def test_harness_ran(self):
        self.assertIn("HARNESS DONE", self.output, self.output)

    def test_positive_control_holds_one_back_and_restores_it(self):
        """The feature must still work: one maxed relic is suppressed then restored."""
        counts = self.counts("positive_control")
        self.assertEqual(counts["suppressed"], 1, self.output)
        self.assertEqual(counts["restored"], 1, self.output)
        self.assertEqual(counts["origcalls"], 1, self.output)
        self.assertIn("relicfilter: holding back 1 of 1 maxed relic(s) on this roll",
                      self.logs("positive_control"), self.output)

    def test_all_maxed_does_not_claim_to_hold_anything_back(self):
        """REPORTED: logged 'holding back 156' while the bypass applied nothing."""
        counts = self.counts("all_maxed")
        self.assertEqual(counts["suppressed"], 0, self.output)
        logged = " ".join(self.logs("all_maxed"))
        self.assertNotIn("holding back", logged)
        self.assertIn("all 156 relics maxed", logged)
        self.assertIn("stands down", logged)

    def test_failed_repository_lookup_does_not_claim_a_filtered_roll(self):
        """REPORTED: logged 'holding back 1' while no write reached the repository."""
        counts = self.counts("repo_lookup_fails")
        self.assertEqual(counts["suppressed"], 0, self.output)
        logged = " ".join(self.logs("repo_lookup_fails"))
        self.assertIn("held back none", logged)
        self.assertIn("repository lookup failed", logged)

    def test_missing_player_is_reported_as_no_scan_not_as_zero_maxed(self):
        """REPORTED: logged 'holding back 0', which reads as a successful empty scan."""
        counts = self.counts("no_player")
        self.assertEqual(counts["suppressed"], 0, self.output)
        logged = " ".join(self.logs("no_player"))
        self.assertIn("no player resolved yet", logged)
        self.assertIn("nothing scanned", logged)
        self.assertNotIn("no maxed relics to hold back", logged)

    def test_a_real_empty_scan_is_distinguishable_from_a_missing_player(self):
        logged = " ".join(self.logs("scanned_none_maxed"))
        self.assertIn("scanned, no maxed relics to hold back", logged)
        self.assertNotIn("nothing scanned", logged)

    def test_a_thrown_write_is_not_counted_as_held_back(self):
        """REPORTED: the rollback list grew before the write, so a swallowed throw read as success."""
        counts = self.counts("write_throws")
        self.assertEqual(counts["suppressed"], 0, self.output)
        logged = " ".join(self.logs("write_throws"))
        self.assertNotIn("holding back 1 of 1", logged)
        self.assertIn("held back none", logged)
        self.assertIn("drop table write failed", logged)

    def test_a_write_that_reports_success_but_changes_nothing_is_not_counted(self):
        """The harness returns SUCCESS and writes nothing, so only a read-back can catch it."""
        counts = self.counts("write_fails_silently")
        self.assertEqual(counts["suppressed"], 0, self.output)
        logged = " ".join(self.logs("write_fails_silently"))
        self.assertNotIn("holding back", logged)
        self.assertIn("found 2 maxed relic(s) but held back none", logged)
        self.assertIn("drop table write failed", logged)

    def test_a_partial_write_reports_the_confirmed_count_and_names_the_shortfall(self):
        counts = self.counts("partial_write")
        self.assertEqual(counts["suppressed"], 1, self.output)
        logged = " ".join(self.logs("partial_write"))
        self.assertIn("holding back 1 of 2 maxed relic(s) on this roll", logged)
        self.assertIn("1 write(s) failed", logged)

    def test_the_rollback_list_still_covers_every_attempt(self):
        """Bookkeeping and the applied count answer different questions.

        A write whose outcome is unknown must still be restored, so restoration
        attempts track attempts - not confirmed suppressions.
        """
        # partial_write attempted two writes and confirmed one: two restores.
        self.assertEqual(self.counts("partial_write")["restored"], 2, self.output)
        self.assertEqual(self.counts("partial_write")["suppressed"], 1, self.output)
        # write_fails_silently confirmed none and still restores both attempts.
        self.assertEqual(self.counts("write_fails_silently")["restored"], 2, self.output)
        self.assertEqual(self.counts("write_fails_silently")["suppressed"], 0, self.output)

    def test_every_scenario_reports_a_distinct_state(self):
        """The line dedupes on its own text, so each state must read differently."""
        lines = [self.logs(label)[-1] for label in (
            "positive_control", "all_maxed", "repo_lookup_fails", "no_player",
            "scanned_none_maxed", "write_throws", "write_fails_silently", "partial_write")]
        self.assertEqual(len(set(lines)), len(lines), lines)


if __name__ == "__main__":
    unittest.main()
