"""Run the real ForgePact::DungeonChest decision against a fake dungeon.

Dungeon chest opens early (`dungeonchest <pct>|off|status`, issue #31): in a
key dungeon the end chest becomes openable once <pct> % (50..95) of the
dungeon's monsters are dead. "The dungeon's monsters" is the planned total T,
every monster the dungeon will make, spawned yet or not, asked of the
adapter's total source when the chest is first seen and then fixed; progress
is the kills counted since then and the threshold is ceil(pct % of T). The
unlock is the `instance_exists` detour: once the threshold latches, the
chest's own poll for an Enemy_Parent_obj is answered `false`. A countdown
(`Chest: <n> kills to go`) shows when 50 or fewer kills remain, above the
player's head, as chat lines at milestones, both, or neither.

These scenarios pin the decision, the tally, the detour's decision and the
countdown: nothing at all while the mode is off or the room holds no chest,
the latch at exactly the threshold (the 20th of a planned 40 at 50 %, the
300th of Live procedure 1's 600, not the one before), a monotone countdown
while the spawners top the room back up, nothing decided or shown while the
total is unknown, the chest's poll answered only for the chest, only for an
enemy argument and only after the latch, the countdown text's window, one
count per enemy id, a reset on a room change or when the chest is gone, the
chat milestones (each once, one line for a kill that passes several), the
chat forms refused while no chat call is available or after one failed, and a
latch whose unlock action fails reported as `unlocked=0` with no `ready to
open` line and the chest's poll left alone.

The header is compiled three times. Once as written, where every scenario must
pass; once with the unlock decision replaced by one that never unlocks, where
the baselines and the form and command scenarios must still pass and every
target must fail. That second run is the negative control: it shows the
targets measure the latch itself, and that a "not yet" target cannot pass for
a decision that does nothing.

The third run is the same kind of control for the command's refusal: a share
outside 50..95, or any share while the kill hook is not on both routes
(failed or table-only), the detour is not installed or no total source is
supplied, must be refused, so `dungeonchest 49` cannot report itself armed,
and neither can a share that would count no kill, never open the chest or
have nothing to be a share of. This build replaces the store-or-refuse line
with one that stores every mode, and the refusal scenario must fail there.
"""
import os
import shutil
import subprocess
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "plugin/include/ForgePact/DungeonChestMod.hpp"
HARNESS = ROOT / "tests/dungeon_chest_harness.cpp"
# The unlock decision's one line; the negative control swaps it.
DECISION_LINE = "return kills >= threshold;"
NEVER_UNLOCKS = "return false;"
# The command's store-or-refuse decision; its negative control stores every mode.
STORE_LINE = 'return pct == 0 || (InRange(pct) && hook == "ok" && unlock == "ok" && totalAvailable);'
STORES_EVERY_MODE = "return true;"
REFUSAL = "command/refused"

BASELINES = ("baseline/off_never_latches", "baseline/no_chest_no_tally")
TOTAL = ("target/total-planned", "target/total-unknown")
POLL = ("target/poll-answered-while-latched", "target/poll-untouched-otherwise")
TARGETS = (
    "target/latch_at_threshold",
    "target/unlock-failed",
) + TOTAL + POLL + (
    "target/countdown_text",
    "target/kill_once_per_id",
    "target/room_change_resets",
    "target/chat-milestones",
    "target/chat-milestones-double-kill",
)
# The countdown's forms: what is drawn and what is sent, whatever the latch does.
FORMS = ("form/countdown-head", "form/countdown-none", "form/chat-refused", "form/chat-failed")
COMMANDS = ("command/words_parsed", REFUSAL, "command/status_line")


def _compile_and_run(name, header_text):
    klass = "\n".join(
        line for line in header_text.split("\n")
        if not line.strip().startswith("#pragma once") and '#include "Common.hpp"' not in line
    )
    out = ROOT / "build/dungeon-chest-behavior"
    out.mkdir(parents=True, exist_ok=True)
    cpp = out / f"{name}.cpp"
    cpp.write_text(HARNESS.read_text(encoding="utf-8").replace("// PRODUCTION_DUNGEONCHEST", klass), encoding="utf-8")
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


class DungeonChestBehaviorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        header = HEADER.read_text(encoding="utf-8")
        assert header.count(DECISION_LINE) == 1, "the unlock decision moved; update DECISION_LINE"
        cls.output = _compile_and_run("dungeonchest", header)
        cls.never_unlocks = _compile_and_run("dungeonchest-never-unlocks", header.replace(DECISION_LINE, NEVER_UNLOCKS))
        assert header.count(STORE_LINE) == 1, "the store decision moved; update STORE_LINE"
        cls.stores_every_mode = _compile_and_run(
            "dungeonchest-stores-every-mode", header.replace(STORE_LINE, STORES_EVERY_MODE))

    def assertScenario(self, label):
        line = _line(self.output, label)
        self.assertTrue(line.startswith("PASS "), line)

    def test_all_scenarios_pass(self):
        self.assertIn("RESULT OK", self.output, self.output)

    def test_baseline_mode_off_and_no_chest_are_the_vanilla_game(self):
        for label in BASELINES:
            self.assertScenario(label)

    def test_target_the_chest_unlocks_at_exactly_the_threshold(self):
        self.assertScenario("target/latch_at_threshold")
        self.assertScenario("target/unlock-failed")

    def test_target_total_planned_fixed_at_first_sight(self):
        """T = 600 at 50 % latches at the 300th kill; the countdown is monotone; the source is asked once."""
        self.assertScenario("target/total-planned")

    def test_target_total_unknown_decides_and_shows_nothing(self):
        """No source: refused with total=unavailable; a source answering 0: never latches, asked again each poll."""
        self.assertScenario("target/total-unknown")

    def test_target_unlock_poll_answered_only_for_the_chest_while_latched(self):
        """The instance_exists detour's decision, with its negative control beside it."""
        for label in POLL:
            self.assertScenario(label)

    def test_target_countdown_text_window(self):
        """Absent above 50 remaining, present at 50 and at 1, absent at 0."""
        self.assertScenario("target/countdown_text")

    def test_target_tally_counts_each_enemy_once_and_resets_per_room(self):
        self.assertScenario("target/kill_once_per_id")
        self.assertScenario("target/room_change_resets")

    def test_target_chat_milestones(self):
        self.assertScenario("target/chat-milestones")
        self.assertScenario("target/chat-milestones-double-kill")

    def test_countdown_forms(self):
        """countdown-head (the default), countdown-none, chat-refused, chat-failed."""
        for label in FORMS:
            self.assertScenario(label)

    def test_command_words_and_status(self):
        self.assertScenario("command/words_parsed")
        self.assertScenario("command/status_line")

    def test_refused_shares_and_failed_hook_leave_the_mode(self):
        """49, 96 and abc refused; 50, 73 and 95 stored; a failed or table-only kill hook or unlock detour, or no total source, refuses every share."""
        self.assertScenario(REFUSAL)

    def test_negative_control_refusal_fails_when_every_mode_is_stored(self):
        line = _line(self.stores_every_mode, REFUSAL)
        self.assertTrue(line.startswith("FAIL "),
                        f"{REFUSAL} passed against a decision that stores every mode: {line}")
        # Only the store decision was swapped: every other scenario still passes there.
        for label in BASELINES + TARGETS + FORMS + ("command/words_parsed", "command/status_line"):
            self.assertTrue(_line(self.stores_every_mode, label).startswith("PASS "), label)

    def test_negative_control_baselines_pass_and_targets_fail_without_the_unlock(self):
        for label in BASELINES + FORMS + COMMANDS:
            line = _line(self.never_unlocks, label)
            self.assertTrue(line.startswith("PASS "), line)
        for label in TARGETS:
            line = _line(self.never_unlocks, label)
            self.assertTrue(line.startswith("FAIL "),
                            f"{label} passed against a decision that never unlocks: {line}")


if __name__ == "__main__":
    unittest.main()
