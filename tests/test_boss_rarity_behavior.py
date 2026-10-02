"""Run the real ForgePact::BossRarity decision against fake bosses.

The Bosses control (`bossrarity off|rare|ancient`, issue #44) makes every boss
the game spawns at rarity 1 roll as Rare (3) or Ancient (4) inside the shared
EnemyRaritySettings hook, with the sliders' affix top-up (2 for rare, 3 for
ancient). These scenarios pin the decision and its counters: nothing at all
while the mode is off, a rank-1 boss raised to the chosen tier, a boss the game
already made champion/rare/ancient left alone, an ordinary monster never
touched, a boss another monster created left alone, and a status line that
counts what was raised, not what was asked for.

The header is compiled twice. Once as written, where every scenario must pass;
once with the decision replaced by one that never raises, where the two
baselines must still pass and every target must fail. That second run is the
negative control: it shows the targets measure the raise itself, and that the
"left alone" targets cannot pass for a decision that does nothing.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/BossRarityMod.hpp"
HARNESS = ROOT / "tests/boss_rarity_harness.cpp"
# The decision's one line that answers a tier; the negative control swaps it.
DECISION_LINE = "return enemyRarity == 1.0 ? TierFor(mode) : 0;"
NEVER_RAISES = "return 0;"

BASELINES = ("baseline/off_boss_untouched", "baseline/off_normal_monster_untouched")
TARGETS = (
    "target/rare_boss_rank1_to_3",
    "target/ancient_boss_rank1_to_4",
    "target/boss_already_rare_untouched",
    "target/normal_monster_untouched_in_boss_mode",
    "target/enemy_born_boss_untouched",
    "target/status_line_counts_raised",
)


def _compile_and_run(name, header_text):
    klass = "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith("#pragma once") and '#include "Common.hpp"' not in line
    )
    out = ROOT / "build/boss-rarity-behavior"
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / f"{name}.cpp"
    cpp.write_text(HARNESS.read_text(encoding="utf-8").replace("// PRODUCTION_BOSSRARITY", klass), encoding="utf-8")
    binary = out / (f"{name}.exe" if os.name == "nt" else name)
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
        batch = out / f"compile-{name}.cmd"
        batch.write_text(
            f'@echo off\ncall "{vcvars}" >nul\nif errorlevel 1 exit /b 1\n'
            f'cl /nologo /std:c++20 /EHsc /O2 "{cpp}" /Fe:"{binary}" /Fo:"{out / (name + ".obj")}"\n'
            f'exit /b %errorlevel%\n', encoding="utf-8")
        command = ["cmd", "/d", "/c", str(batch)]
    else:
        compiler = shutil.which("c++")
        if not compiler:
            raise unittest.SkipTest("A C++20 compiler is required for native behavior tests")
        command = [compiler, "-std=c++20", "-O2", str(cpp), "-o", str(binary)]
    # The compiler speaks the machine's locale; decode leniently.
    result = subprocess.run(command, cwd=out, capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / f"compile-{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise AssertionError(result.stdout + result.stderr)
    run = subprocess.run([str(binary)], capture_output=True, text=True, encoding="utf-8", errors="replace")
    (out / f"{name}.log").write_text(run.stdout + run.stderr, encoding="utf-8")
    return run.stdout


def _line(output, label):
    for line in output.split("\n"):
        if line.split(" ")[1:2] == [label]:
            return line
    raise AssertionError(f"scenario {label!r} not in harness output:\n{output}")


class BossRarityBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = HEADER.read_text(encoding="utf-8")
        assert header.count(DECISION_LINE) == 1, "the decision line moved; update DECISION_LINE"
        cls.output = _compile_and_run("bossrarity", header)
        cls.never_raises = _compile_and_run("bossrarity-never-raises", header.replace(DECISION_LINE, NEVER_RAISES))

    def assertScenario(self, label):
        line = _line(self.output, label)
        self.assertTrue(line.startswith("PASS "), line)

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_baseline_mode_off_is_the_vanilla_game(self):
        for label in BASELINES:
            self.assertScenario(label)

    def test_target_a_rank_one_boss_takes_the_chosen_tier(self):
        self.assertScenario("target/rare_boss_rank1_to_3")
        self.assertScenario("target/ancient_boss_rank1_to_4")

    def test_target_what_the_mode_leaves_alone(self):
        self.assertScenario("target/boss_already_rare_untouched")
        self.assertScenario("target/normal_monster_untouched_in_boss_mode")
        self.assertScenario("target/enemy_born_boss_untouched")

    def test_target_status_line_counts_what_was_raised(self):
        self.assertScenario("target/status_line_counts_raised")
        self.assertScenario("command/failed_write_not_counted_raised")

    def test_command_words(self):
        self.assertScenario("command/modes_parsed")

    def test_negative_control_baselines_pass_and_targets_fail_without_the_raise(self):
        for label in BASELINES:
            line = _line(self.never_raises, label)
            self.assertTrue(line.startswith("PASS "), line)
        for label in TARGETS:
            line = _line(self.never_raises, label)
            self.assertTrue(line.startswith("FAIL "),
                            f"{label} passed against a decision that never raises: {line}")


if __name__ == "__main__":
    unittest.main()
