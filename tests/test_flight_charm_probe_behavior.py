"""Run charmprobe's real core against controlled samples.

`charmprobe` (research build only, ForgePact #158 phase 1) reads the equipped
charms, spawns research charms, dresses built items and calls DropItemHeroic,
so the live session can find which itemInfoStruct keys hold an item's
footprint and sprite and what the Heroic roll places. These scenarios pin the
decisions ModuleMain.cpp's adapter takes from
plugin/include/ForgePact/FlightCharmProbe.hpp, compiled whole.

The size keys: identified from two charms of different sizes; `unknown` from
one size only, from a key that matched on one sample but not another, from
two candidate keys, and from one key standing for both sides. The sprite key:
identified when exactly one key is a sprite on every sample and took two
values, `unknown` otherwise, and set by hand. The parsers for spawn (its
`sprite` flag included), dress, sprite, heroic and repo accept their forms and
name the reason for every refusal; the dress rules cap at eight, one per `a`;
the Heroic summary and the bare marker print their documented lines.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FlightCharmProbeBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = (ROOT / "plugin/include/ForgePact/FlightCharmProbe.hpp").read_text(encoding="utf-8")
        core = "\n".join(line for line in header.split("\n") if not line.strip().startswith("#pragma once"))
        out = ROOT / "build/flight-charm-probe-behavior"
        out.mkdir(parents=True, exist_ok=True)
        code = (ROOT / "tests/flight_charm_probe_harness.cpp").read_text(encoding="utf-8")
        code = code.replace("// PRODUCTION_FLIGHTCHARMS", core)
        cpp = out / "flightcharms.cpp"
        cpp.write_text(code, encoding="utf-8")
        cls.binary = out / ("flightcharms.exe" if os.name == "nt" else "flightcharms")
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
                f'cl /nologo /std:c++20 /EHsc /O2 /W4 /WX /I "{ROOT / "plugin/include"}" "{cpp}" /Fe:"{cls.binary}" /Fo:"{out / "flightcharms.obj"}"\n'
                f'exit /b %errorlevel%\n', encoding="utf-8")
            command = ["cmd", "/d", "/c", str(batch)]
        else:
            compiler = shutil.which("c++")
            if not compiler:
                raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
            command = [compiler, "-std=c++20", "-O2", "-Wall", "-I", str(ROOT / "plugin/include"), str(cpp), "-o", str(cls.binary)]
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
        self.assertNotIn("FAIL ", self.output, self.output)

    # ---- the size keys --------------------------------------------------------

    def test_size_keys_found_from_two_charms_of_different_sizes(self):
        self.assertScenario("size_keys/found_from_two_sizes")
        self.assertScenario("size_keys/three_sizes_identified_without_one")

    def test_size_keys_unknown_from_one_size_only(self):
        self.assertScenario("size_keys/unknown_from_one_size_only")
        self.assertScenario("size_keys/no_samples")

    def test_size_keys_unknown_from_a_key_matching_on_one_sample_but_not_another(self):
        self.assertScenario("size_keys/unknown_from_a_key_matching_one_sample_only")
        self.assertScenario("size_keys/unknown_when_the_key_is_missing_on_a_sample")

    def test_size_keys_unknown_from_two_candidate_keys(self):
        for label in ("size_keys/unknown_from_two_candidate_keys", "size_keys/unknown_from_three_candidate_keys_on_square_samples",
                      "size_keys/a_constant_key_equal_to_w_keeps_it_unknown",
                      "size_keys/one_key_for_both_sides_is_unknown"):
            self.assertScenario(label)

    def test_size_keys_refuse_bad_sizes_and_reset(self):
        self.assertScenario("size_keys/refuses_a_size_outside_1_to_10")
        self.assertScenario("size_keys/reset_forgets_the_samples")

    # ---- the sprite key -------------------------------------------------------

    def test_sprite_key_found_when_one_key_is_a_sprite_on_every_sample(self):
        self.assertScenario("sprite_key/found_from_two_samples_with_different_values")

    def test_sprite_key_unknown_otherwise(self):
        for label in ("sprite_key/unknown_from_one_sample", "sprite_key/unknown_with_no_samples",
                      "sprite_key/unknown_when_the_value_never_changed", "sprite_key/unknown_from_two_candidate_keys",
                      "sprite_key/unknown_when_no_key_is_a_sprite_on_every_sample",
                      "sprite_key/a_dropped_key_stays_dropped"):
            self.assertScenario(label)

    def test_sprite_key_set_by_hand(self):
        self.assertScenario("sprite_key/set_by_hand")
        self.assertScenario("sprite_key/reset_forgets_the_hand_key")

    # ---- the parsers ----------------------------------------------------------

    def test_spawn_parser_accepts_its_forms_and_names_every_refusal(self):
        for label in ("parse/spawn_accepted_forms", "parse/spawn_refused_forms_name_their_reason",
                      "spawn/seed_key_json_and_stats_text"):
            self.assertScenario(label)

    def test_dress_parser_and_rules(self):
        for label in ("parse/dress_accepted_forms", "parse/dress_refused_forms_name_their_reason",
                      "dress/at_most_8_rules_and_one_per_a", "dress/only_a_whole_a_matches", "dress/line_and_off"):
            self.assertScenario(label)

    def test_sprite_parser(self):
        self.assertScenario("parse/sprite_accepted_forms")
        self.assertScenario("parse/sprite_refused_forms_name_their_reason")

    def test_heroic_and_repo_parsers(self):
        self.assertScenario("parse/heroic_1_to_20")
        self.assertScenario("parse/repo_forms")

    # ---- the lines ------------------------------------------------------------

    def test_heroic_summary_line(self):
        self.assertScenario("heroic/summary_with_nothing_placed")
        self.assertScenario("heroic/summary_line")

    def test_text_and_marker(self):
        self.assertScenario("text/numbers_and_info_dump")
        self.assertScenario("marker/line")


if __name__ == "__main__":
    unittest.main()
